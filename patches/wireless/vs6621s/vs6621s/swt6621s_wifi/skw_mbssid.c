// SPDX-License-Identifier: GPL-2.0

/******************************************************************************
 *
 * Copyright (C) 2020 SeekWave Technology Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>

#include "skw_core.h"
#include "skw_log.h"
#include "skw_mbssid.h"
#include "skw_cfg80211.h"
#include "skw_compat.h"

#define SKW_GENMASK_ULL(h, l)   (((~0ULL) - (1ULL << (l)) + 1) & \
				(~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

static __always_inline u16 skw_get_unaligned_le16(const void *p)
{
	return le16_to_cpup((__le16 *)p);
}

static struct skw_element *skw_find_elem(u8 eid, const u8 *ies,
					int len, const u8 *match,
					unsigned int match_len,
					unsigned int match_offset)
{
	struct skw_element *elem;

	skw_foreach_element_id(elem, eid, ies, len) {
		if (elem->datalen >= match_offset - 2 + match_len &&
		    !memcmp(elem->data + match_offset - 2, match, match_len))
			return (void *)elem;
	}

	return NULL;
}

static const struct skw_element *
skw_get_profile_continuation(const u8 *ie, size_t ie_len,
			const struct skw_element *mbssid_elem,
			const struct skw_element *sub_elem)
{
	const u8 *mbssid_end = mbssid_elem->data + mbssid_elem->datalen;
	const struct skw_element *next_mbssid;
	const struct skw_element *sub;

	next_mbssid = skw_find_elem(WLAN_EID_MULTIPLE_BSSID,
			mbssid_end, ie_len - (mbssid_end - ie),
			NULL, 0, 0);

	if (!next_mbssid ||
	    (sub_elem->data + sub_elem->datalen < mbssid_end - 1))
		return NULL;

	if (next_mbssid->datalen < 4)
		return NULL;

	sub = (void *)&next_mbssid->data[1];

	if (next_mbssid->data + next_mbssid->datalen < sub->data + sub->datalen)
		return NULL;

	if (sub->id != 0 || sub->datalen < 2)
		return NULL;

	return sub->data[0] == WLAN_EID_NON_TX_BSSID_CAP ?  NULL : next_mbssid;
}

static size_t skw_merge_profile(const u8 *ie, size_t ie_len,
				const struct skw_element *mbssid_elem,
				const struct skw_element *sub_elem,
				u8 *merged_ie, size_t max_copy_len)
{
	size_t copied_len = sub_elem->datalen;
	const struct skw_element *next_mbssid;

	if (sub_elem->datalen > max_copy_len)
		return 0;

	memcpy(merged_ie, sub_elem->data, sub_elem->datalen);

	while ((next_mbssid = skw_get_profile_continuation(ie, ie_len,
					mbssid_elem, sub_elem))) {
		const struct skw_element *next = (void *)&next_mbssid->data[1];

		if (copied_len + next->datalen > max_copy_len)
			break;

		memcpy(merged_ie + copied_len, next->data, next->datalen);

		copied_len += next->datalen;
	}

	return copied_len;
}

static inline void skw_gen_new_bssid(const u8 *bssid, u8 max_bssid,
				u8 mbssid_index, u8 *new_bssid)
{
	u64 bssid_u64 = skw_mac_to_u64(bssid);
	u64 mask = SKW_GENMASK_ULL(max_bssid - 1, 0);
	u64 new_bssid_u64;

	new_bssid_u64 = bssid_u64 & ~mask;

	new_bssid_u64 |= ((bssid_u64 & mask) + mbssid_index) & mask;

	skw_u64_to_mac(new_bssid_u64, new_bssid);
}

static bool is_skw_element_inherited(const struct skw_element *elem,
		const struct skw_element *non_inherit_elem)
{
	u8 id_len, ext_id_len, i, loop_len, id;
	const u8 *list;

	if (elem->id == WLAN_EID_MULTIPLE_BSSID)
		return false;

	if (!non_inherit_elem || non_inherit_elem->datalen < 2)
		return true;

	id_len = non_inherit_elem->data[1];
	if (non_inherit_elem->datalen < 3 + id_len)
		return true;

	ext_id_len = non_inherit_elem->data[2 + id_len];
	if (non_inherit_elem->datalen < 3 + id_len + ext_id_len)
		return true;

	if (elem->id == SKW_WLAN_EID_EXTENSION) {
		if (!ext_id_len)
			return true;

		loop_len = ext_id_len;
		list = &non_inherit_elem->data[3 + id_len];
		id = elem->data[0];
	} else {
		if (!id_len)
			return true;

		loop_len = id_len;
		list = &non_inherit_elem->data[2];
		id = elem->id;
	}

	for (i = 0; i < loop_len; i++) {
		if (list[i] == id)
			return false;
	}

	return true;
}

static size_t skw_gen_new_ie(const u8 *ie, size_t ielen,
		const u8 *subelement, size_t subie_len,
		u8 *new_ie, gfp_t gfp)
{
	u8 eid;
	u8 *pos, *tmp;
	const u8 *tmp_old, *tmp_new;
	const struct skw_element *non_inherit;
	u8 *sub_copy;

	sub_copy = SKW_KMEMDUP(subelement, subie_len, gfp);
	if (!sub_copy)
		return 0;

	pos = &new_ie[0];

	/* set new ssid */
	tmp_new = cfg80211_find_ie(WLAN_EID_SSID, sub_copy, subie_len);
	if (tmp_new) {
		memcpy(pos, tmp_new, tmp_new[1] + 2);
		pos += (tmp_new[1] + 2);
	}

	/* get non inheritance list if exists */
	eid = SKW_EID_EXT_NON_INHERITANCE;
	non_inherit = skw_find_elem(SKW_WLAN_EID_EXTENSION, sub_copy,
					subie_len, &eid, 1, 0);

	tmp_old = cfg80211_find_ie(WLAN_EID_SSID, ie, ielen);
	tmp_old = (tmp_old) ? tmp_old + tmp_old[1] + 2 : ie;

	while (tmp_old + tmp_old[1] + 2 - ie <= ielen) {
		if (tmp_old[0] == 0) {
			tmp_old++;
			continue;
		}

		if (tmp_old[0] == SKW_WLAN_EID_EXTENSION) {
			tmp = (u8 *)skw_find_elem(SKW_WLAN_EID_EXTENSION,
					sub_copy, subie_len, &tmp_old[2], 1, 2);
		} else {
			tmp = (u8 *)cfg80211_find_ie(tmp_old[0], sub_copy,
					subie_len);
		}

		if (!tmp) {
			const struct skw_element *old_elem = (void *)tmp_old;

			if (is_skw_element_inherited(old_elem, non_inherit)) {
				memcpy(pos, tmp_old, tmp_old[1] + 2);
				pos += tmp_old[1] + 2;
			}
		} else {
			if (tmp_old[0] == WLAN_EID_VENDOR_SPECIFIC) {
				if (!memcmp(tmp_old + 2, tmp + 2, 5)) {
					memcpy(pos, tmp, tmp[1] + 2);
					pos += tmp[1] + 2;
					tmp[0] = WLAN_EID_SSID;
				} else {
					memcpy(pos, tmp_old, tmp_old[1] + 2);
					pos += tmp_old[1] + 2;
				}
			} else {
				memcpy(pos, tmp, tmp[1] + 2);
				pos += tmp[1] + 2;
				tmp[0] = WLAN_EID_SSID;
			}
		}

		if (tmp_old + tmp_old[1] + 2 - ie == ielen)
			break;

		tmp_old += tmp_old[1] + 2;
	}

	tmp_new = sub_copy;
	while (tmp_new + tmp_new[1] + 2 - sub_copy <= subie_len) {
		if (!(tmp_new[0] == WLAN_EID_NON_TX_BSSID_CAP ||
		    tmp_new[0] == WLAN_EID_SSID)) {
			memcpy(pos, tmp_new, tmp_new[1] + 2);
			pos += tmp_new[1] + 2;
		}

		if (tmp_new + tmp_new[1] + 2 - sub_copy == subie_len)
			break;

		tmp_new += tmp_new[1] + 2;
	}

	SKW_KFREE(sub_copy);

	return pos - new_ie;
}

static void skw_parse_mbssid_data(struct wiphy *wiphy,
				struct ieee80211_channel *rx_channel,
				s32 signal,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
				enum cfg80211_bss_frame_type ftype,
#endif
				const u8 *bssid, u64 tsf,
				u16 beacon_interval, const u8 *ie,
				size_t ie_len, gfp_t gfp)
{
	const u8 *idx_ie;
	const struct skw_element *elem, *sub;
	size_t new_ie_len;
	u8 bssid_index;
	u8 max_indicator;
	u8 new_bssid[ETH_ALEN];
	u8 *new_ie, *profile;
	u64 seen_indices = 0;
	u16 capability;
	struct cfg80211_bss *bss;

	new_ie = SKW_ZALLOC(IEEE80211_MAX_DATA_LEN, gfp);
	if (!new_ie)
		return;

	profile = SKW_ZALLOC(ie_len, gfp);
	if (!profile)
		goto out;

	skw_foreach_element_id(elem, WLAN_EID_MULTIPLE_BSSID, ie, ie_len) {
		if (elem->datalen < 4)
			continue;

		skw_foreach_element(sub, elem->data + 1, elem->datalen - 1) {
			u8 profile_len;

			if (sub->id != 0 || sub->datalen < 4)
				continue;

			if (sub->data[0] != WLAN_EID_NON_TX_BSSID_CAP ||
			    sub->data[1] != 2) {
				continue;
			}

			memset(profile, 0, ie_len);

			profile_len = skw_merge_profile(ie, ie_len,
					elem, sub, profile, ie_len);
			idx_ie = cfg80211_find_ie(SKW_WLAN_EID_MULTI_BSSID_IDX,
						  profile, profile_len);

			if (!idx_ie || idx_ie[1] < 1 ||
			    idx_ie[2] == 0 || idx_ie[2] > 46) {
				/* No valid Multiple BSSID-Index element */
				continue;
			}

			if (seen_indices & (1ULL << (idx_ie[2])))
				net_dbg_ratelimited("Partial info for BSSID index %d\n",
						idx_ie[2]);

			seen_indices |= (1ULL << (idx_ie[2]));

			bssid_index = idx_ie[2];
			max_indicator = elem->data[0];

			skw_gen_new_bssid(bssid, max_indicator,
					bssid_index, new_bssid);

			memset(new_ie, 0, IEEE80211_MAX_DATA_LEN);
			new_ie_len = skw_gen_new_ie(ie, ie_len, profile,
						profile_len, new_ie,
						GFP_KERNEL);
			if (!new_ie_len)
				continue;

			capability = skw_get_unaligned_le16(profile + 2);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
			bss = cfg80211_inform_bss(wiphy, rx_channel, ftype,
						new_bssid, tsf, capability,
						beacon_interval, new_ie,
						new_ie_len, signal, gfp);
#else
			bss = cfg80211_inform_bss(wiphy, rx_channel,
						new_bssid, tsf, capability,
						beacon_interval, new_ie,
						new_ie_len, signal, gfp);
#endif

			if (!bss)
				break;

			skw_bss_priv(bss)->bssid_index = bssid_index;
			skw_bss_priv(bss)->max_bssid_indicator = max_indicator;

			cfg80211_put_bss(wiphy, bss);
		}
	}

	SKW_KFREE(profile);
out:
	SKW_KFREE(new_ie);
}

void skw_mbssid_data_parser(struct wiphy *wiphy, bool beacon,
		struct ieee80211_channel *chan, s32 signal,
		struct ieee80211_mgmt *mgmt, int mgmt_len)
{
	const u8 *ie = mgmt->u.probe_resp.variable;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	enum cfg80211_bss_frame_type ftype = CFG80211_BSS_FTYPE_PRESP;
#endif
	size_t len = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);

	if (!cfg80211_find_ie(WLAN_EID_MULTIPLE_BSSID, ie, mgmt_len - len))
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	if (beacon)
		ftype = CFG80211_BSS_FTYPE_BEACON;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	skw_parse_mbssid_data(wiphy, chan, signal,  ftype, mgmt->bssid,
			le64_to_cpu(mgmt->u.probe_resp.timestamp),
			le16_to_cpu(mgmt->u.probe_resp.beacon_int),
			ie, mgmt_len - len, GFP_KERNEL);
#else
	skw_parse_mbssid_data(wiphy, chan, signal, mgmt->bssid,
			le64_to_cpu(mgmt->u.probe_resp.timestamp),
			le16_to_cpu(mgmt->u.probe_resp.beacon_int),
			ie, mgmt_len - len, GFP_KERNEL);
#endif
}
