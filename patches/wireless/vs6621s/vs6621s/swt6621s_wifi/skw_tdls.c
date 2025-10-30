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
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>

#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_work.h"
#include "skw_log.h"
#include "skw_tx.h"
#include "skw_compat.h"
#include "skw_msg.h"
#include "skw_tdls.h"

static size_t skw_skip_ie(const u8 *ies, size_t ielen, size_t pos)
{
	/* we assume a validly formed IEs buffer */
	u8 len = ies[pos + 1];

	pos += 2 + len;

	/* the IE itself must have 255 bytes for fragments to follow */
	if (len < 255)
		return pos;

	while (pos < ielen && ies[pos] == SKW_WLAN_EID_FRAGMENT) {
		len = ies[pos + 1];
		pos += 2 + len;
	}

	return pos;
}

static bool skw_id_in_list(const u8 *ids, int n_ids, u8 id, bool id_ext)
{
	int i = 0;

	/* Make sure array values are legal */
	if (WARN_ON(ids[n_ids - 1] == SKW_WLAN_EID_EXTENSION))
		return false;

	while (i < n_ids) {
		if (ids[i] == SKW_WLAN_EID_EXTENSION) {
			if (id_ext && (ids[i + 1] == id))
				return true;

			i += 2;
			continue;
		}

		if (ids[i] == id && !id_ext)
			return true;

		i++;
	}

	return false;
}

static size_t skw_ie_split_ric(const u8 *ies, size_t ielen,
			const u8 *ids, int n_ids,
			const u8 *after_ric, int n_after_ric,
			size_t offset)
{
	size_t pos = offset;

	while (pos < ielen) {
		u8 ext = 0;

		if (ies[pos] == SKW_WLAN_EID_EXTENSION)
			ext = 2;
		if ((pos + ext) >= ielen)
			break;

		if (!skw_id_in_list(ids, n_ids, ies[pos + ext],
					  ies[pos] == SKW_WLAN_EID_EXTENSION))
			break;

		if (ies[pos] == WLAN_EID_RIC_DATA && n_after_ric) {
			pos = skw_skip_ie(ies, ielen, pos);

			while (pos < ielen) {
				if (ies[pos] == SKW_WLAN_EID_EXTENSION)
					ext = 2;
				else
					ext = 0;

				if ((pos + ext) >= ielen)
					break;

				if (!skw_id_in_list(after_ric,
							  n_after_ric,
							  ies[pos + ext],
							  ext == 2))
					pos = skw_skip_ie(ies, ielen, pos);
				else
					break;
			}
		} else {
			pos = skw_skip_ie(ies, ielen, pos);
		}
	}

	return pos;
}

static bool skw_chandef_to_operating_class(struct cfg80211_chan_def *chandef,
					  u8 *op_class)
{
	u8 vht_opclass;
	u32 freq = chandef->center_freq1;

	if (freq >= 2412 && freq <= 2472) {
		if (chandef->width > NL80211_CHAN_WIDTH_40)
			return false;

		/* 2.407 GHz, channels 1..13 */
		if (chandef->width == NL80211_CHAN_WIDTH_40) {
			if (freq > chandef->chan->center_freq)
				*op_class = 83; /* HT40+ */
			else
				*op_class = 84; /* HT40- */
		} else {
			*op_class = 81;
		}

		return true;
	}

	if (freq == 2484) {
		if (chandef->width > NL80211_CHAN_WIDTH_40)
			return false;

		*op_class = 82; /* channel 14 */
		return true;
	}

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_80:
		vht_opclass = 128;
		break;
	case NL80211_CHAN_WIDTH_160:
		vht_opclass = 129;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		vht_opclass = 130;
		break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	case NL80211_CHAN_WIDTH_10:
	case NL80211_CHAN_WIDTH_5:
		return false; /* unsupported for now */
#endif
	default:
		vht_opclass = 0;
		break;
	}

	/* 5 GHz, channels 36..48 */
	if (freq >= 5180 && freq <= 5240) {
		if (vht_opclass) {
			*op_class = vht_opclass;
		} else if (chandef->width == NL80211_CHAN_WIDTH_40) {
			if (freq > chandef->chan->center_freq)
				*op_class = 116;
			else
				*op_class = 117;
		} else {
			*op_class = 115;
		}

		return true;
	}

	/* 5 GHz, channels 52..64 */
	if (freq >= 5260 && freq <= 5320) {
		if (vht_opclass) {
			*op_class = vht_opclass;
		} else if (chandef->width == NL80211_CHAN_WIDTH_40) {
			if (freq > chandef->chan->center_freq)
				*op_class = 119;
			else
				*op_class = 120;
		} else {
			*op_class = 118;
		}

		return true;
	}

	/* 5 GHz, channels 100..144 */
	if (freq >= 5500 && freq <= 5720) {
		if (vht_opclass) {
			*op_class = vht_opclass;
		} else if (chandef->width == NL80211_CHAN_WIDTH_40) {
			if (freq > chandef->chan->center_freq)
				*op_class = 122;
			else
				*op_class = 123;
		} else {
			*op_class = 121;
		}

		return true;
	}

	/* 5 GHz, channels 149..169 */
	if (freq >= 5745 && freq <= 5845) {
		if (vht_opclass) {
			*op_class = vht_opclass;
		} else if (chandef->width == NL80211_CHAN_WIDTH_40) {
			if (freq > chandef->chan->center_freq)
				*op_class = 126;
			else
				*op_class = 127;
		} else if (freq <= 5805) {
			*op_class = 124;
		} else {
			*op_class = 125;
		}

		return true;
	}

	/* 56.16 GHz, channel 1..4 */
	if (freq >= 56160 + 2160 * 1 && freq <= 56160 + 2160 * 4) {
		if (chandef->width >= NL80211_CHAN_WIDTH_40)
			return false;

		*op_class = 180;
		return true;
	}

	/* not supported yet */
	return false;
}

static void skw_tdls_add_link_ie(struct net_device *ndev, struct sk_buff *skb,
		const u8 *peer, bool initiator)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct ieee80211_tdls_lnkie *lnk;
	const u8 *src_addr, *dst_addr;

	if (initiator) {
		src_addr = ndev->dev_addr;
		dst_addr = peer;
	} else {
		src_addr = peer;
		dst_addr = ndev->dev_addr;
	}

	lnk = (struct ieee80211_tdls_lnkie *)skb_put(skb, sizeof(*lnk));

	lnk->ie_type = WLAN_EID_LINK_ID;
	lnk->ie_len = sizeof(struct ieee80211_tdls_lnkie) - 2;

	memcpy(lnk->bssid, iface->sta.core.bss.bssid, ETH_ALEN);
	memcpy(lnk->init_sta, src_addr, ETH_ALEN);
	memcpy(lnk->resp_sta, dst_addr, ETH_ALEN);
}

static int skw_add_srates_ie(struct net_device *ndev, struct sk_buff *skb,
		bool need_basic, enum nl80211_band band)
{
	struct ieee80211_supported_band *sband;
	struct skw_iface *iface = netdev_priv(ndev);
	int rate, shift = 0;
	u8 i, rates, *pos;
	//u32 basic_rates = sdata->vif.bss_conf.basic_rates;
	u32 basic_rates = 0xFFFF;
	u32 rate_flags = 0;

	//shift = ieee80211_vif_get_shift(&sdata->vif);
	//shift = ieee80211_vif_get_shift(&sdata->vif);
	//rate_flags = ieee80211_chandef_rate_flags(&sdata->vif.bss_conf.chandef);
	sband = iface->wdev.wiphy->bands[band];
	rates = 0;
	for (i = 0; i < sband->n_bitrates; i++) {
		if ((rate_flags & sband->bitrates[i].flags) != rate_flags)
			continue;
		rates++;
	}
	if (rates > 8)
		rates = 8;

	if (skb_tailroom(skb) < rates + 2)
		return -ENOMEM;

	pos = skb_put(skb, rates + 2);
	*pos++ = WLAN_EID_SUPP_RATES;
	*pos++ = rates;
	for (i = 0; i < rates; i++) {
		u8 basic = 0;

		if ((rate_flags & sband->bitrates[i].flags) != rate_flags)
			continue;

		if (need_basic && basic_rates & BIT(i))
			basic = 0x80;
		rate = DIV_ROUND_UP(sband->bitrates[i].bitrate,
				    5 * (1 << shift));
		*pos++ = basic | (u8) rate;
	}

	return 0;
}

static int skw_add_ext_srates_ie(struct net_device *ndev,
				struct sk_buff *skb, bool need_basic,
				enum nl80211_band band)
{
	struct ieee80211_supported_band *sband;
	int rate, shift = 0;
	u8 i, exrates, *pos;
	//u32 basic_rates = sdata->vif.bss_conf.basic_rates;
	u32 basic_rates = 0xFFFF;
	u32 rate_flags = 0;
	struct skw_iface *iface = netdev_priv(ndev);

	//rate_flags = ieee80211_chandef_rate_flags(&sdata->vif.bss_conf.chandef);
	//shift = ieee80211_vif_get_shift(&sdata->vif);

	sband = iface->wdev.wiphy->bands[band];
	exrates = 0;
	for (i = 0; i < sband->n_bitrates; i++) {
		if ((rate_flags & sband->bitrates[i].flags) != rate_flags)
			continue;
		exrates++;
	}

	if (exrates > 8)
		exrates -= 8;
	else
		exrates = 0;

	if (skb_tailroom(skb) < exrates + 2)
		return -ENOMEM;

	if (exrates) {
		pos = skb_put(skb, exrates + 2);
		*pos++ = WLAN_EID_EXT_SUPP_RATES;
		*pos++ = exrates;
		for (i = 8; i < sband->n_bitrates; i++) {
			u8 basic = 0;

			if ((rate_flags & sband->bitrates[i].flags)
			    != rate_flags)
				continue;
			if (need_basic && basic_rates & BIT(i))
				basic = 0x80;
			rate = DIV_ROUND_UP(sband->bitrates[i].bitrate,
					    5 * (1 << shift));
			*pos++ = basic | (u8) rate;
		}
	}

	return 0;
}

static u8
skw_tdls_add_subband(struct net_device *ndev, struct sk_buff *skb,
		u16 start, u16 end, u16 spacing)
{
	u8 subband_cnt = 0, ch_cnt = 0;
	struct ieee80211_channel *ch;
	struct cfg80211_chan_def chandef;
	int i, subband_start;
	struct skw_iface *iface = netdev_priv(ndev);
	struct wiphy *wiphy = iface->wdev.wiphy;

	for (i = start; i <= end; i += spacing) {
		if (!ch_cnt)
			subband_start = i;

		ch = ieee80211_get_channel(iface->wdev.wiphy, i);
		if (ch) {
			/* we will be active on the channel */
			cfg80211_chandef_create(&chandef, ch,
						NL80211_CHAN_NO_HT);
			if (skw_compat_reg_can_beacon(wiphy, &chandef,
						      iface->wdev.iftype)) {
				ch_cnt++;
				/*
				 * check if the next channel is also part of
				 * this allowed range
				 */
				continue;
			}
		}

		/*
		 * we've reached the end of a range, with allowed channels
		 * found
		 */
		if (ch_cnt) {
			u8 *pos = skb_put(skb, 2);
			*pos++ = skw_freq_to_chn(subband_start);
			*pos++ = ch_cnt;

			subband_cnt++;
			ch_cnt = 0;
		}
	}

	/* all channels in the requested range are allowed - add them here */
	if (ch_cnt) {
		u8 *pos = skb_put(skb, 2);
		*pos++ = skw_freq_to_chn(subband_start);
		*pos++ = ch_cnt;

		subband_cnt++;
	}

	return subband_cnt;
}

static void
skw_tdls_add_supp_channels(struct net_device *ndev, struct sk_buff *skb)
{
	/*
	 * Add possible channels for TDLS. These are channels that are allowed
	 * to be active.
	 */
	u8 subband_cnt;
	u8 *pos = skb_put(skb, 2);

	*pos++ = WLAN_EID_SUPPORTED_CHANNELS;

	/*
	 * 5GHz and 2GHz channels numbers can overlap. Ignore this for now, as
	 * this doesn't happen in real world scenarios.
	 */

	/* 2GHz, with 5MHz spacing */
	subband_cnt = skw_tdls_add_subband(ndev, skb, 2412, 2472, 5);

	/* 5GHz, with 20MHz spacing */
	subband_cnt += skw_tdls_add_subband(ndev, skb, 5000, 5825, 20);

	/* length */
	*pos = 2 * subband_cnt;
}

static void skw_tdls_add_ext_capab(struct net_device *ndev,
				struct sk_buff *skb)
{
	u8 cap;
	//struct ieee80211_supported_band *sband;
	//struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

	//bool wider_band = ieee80211_hw_check(&local->hw, TDLS_WIDER_BW) &&
			  //!ifmgd->tdls_wider_bw_prohibited;
	//bool buffer_sta = ieee80211_hw_check(&local->hw,
	//				     SUPPORTS_TDLS_BUFFER_STA);
#ifdef WLAN_EXT_CAPA8_TDLS_WIDE_BW_ENABLED
	struct skw_iface *iface = netdev_priv(ndev);
	enum nl80211_band band = iface->sta.core.bss.channel->band;
	struct ieee80211_supported_band *sband = iface->wdev.wiphy->bands[band];
	bool vht = sband && sband->vht_cap.vht_supported;
	bool wider_band = false;
#endif
	u8 *pos = skb_put(skb, 10);

	*pos++ = WLAN_EID_EXT_CAPABILITY;
	*pos++ = 8; /* len */
	*pos++ = 0x0;
	*pos++ = 0x0;
	*pos++ = 0x0;

	cap = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	cap |= WLAN_EXT_CAPA4_TDLS_BUFFER_STA;

	if (iface->wdev.wiphy->features & NL80211_FEATURE_TDLS_CHANNEL_SWITCH)
		cap |= WLAN_EXT_CAPA4_TDLS_CHAN_SWITCH;
#endif
	*pos++ = cap;
	*pos++ = WLAN_EXT_CAPA5_TDLS_ENABLED;
	*pos++ = 0;
	*pos++ = 0;
#ifdef WLAN_EXT_CAPA8_TDLS_WIDE_BW_ENABLED
	*pos++ = (vht && wider_band) ? WLAN_EXT_CAPA8_TDLS_WIDE_BW_ENABLED : 0;
#else
	*pos++ = 0;
#endif
}

/**
 * @brief append wmm ie
 *
 * @param skb              A pointer to sk_buff structure
 * @param wmm_type         SKW_WMM_TYPE_INFO/SKW_WMM_TYPE_PARAMETER
 * @param pQosInfo         A pointer to qos info
 *
 * @return                      N/A
 */
static void
skw_add_wmm_ie(struct skw_iface *iface, struct sk_buff *skb,
		u8 wmm_type, u8 *pQosInfo)
{
	u8 wmmInfoElement[] = { 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01 };
	u8 wmmParamElement[] = { 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01};

	u8 qosInfo = 0x0;
	u8 reserved = 0;
	u8 wmmParamIe_len = 24;
	u8 wmmInfoIe_len = 7;
	u8 len = 0;
	u8 *pos;

	if (skb_tailroom(skb) < wmmParamIe_len + 2)
		return;

	qosInfo = (pQosInfo == NULL) ? 0xf : (*pQosInfo);

	/*wmm parameter */
	if (wmm_type == SKW_WMM_TYPE_PARAMETER) {
		pos = skb_put(skb, wmmParamIe_len + 2);
		len = wmmParamIe_len;
	} else {
		pos = skb_put(skb, wmmInfoIe_len + 2);
		len = wmmInfoIe_len;
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = len;

	/*wmm parameter */
	if (wmm_type == SKW_WMM_TYPE_PARAMETER) {
		memcpy(pos, wmmParamElement, sizeof(wmmParamElement));
		pos += sizeof(wmmParamElement);
	} else {
		memcpy(pos, wmmInfoElement, sizeof(wmmInfoElement));
		pos += sizeof(wmmInfoElement);
	}
	*pos++ = qosInfo;

	/* wmm parameter */
	if (wmm_type == SKW_WMM_TYPE_PARAMETER) {
		*pos++ = reserved;
		/* Use the same WMM AC parameters as STA for TDLS link */
		memcpy(pos, &iface->wmm.ac[0], sizeof(struct skw_ac_param));
		pos += sizeof(struct skw_ac_param);
		memcpy(pos, &iface->wmm.ac[1], sizeof(struct skw_ac_param));
		pos += sizeof(struct skw_ac_param);
		memcpy(pos, &iface->wmm.ac[2], sizeof(struct skw_ac_param));
		pos += sizeof(struct skw_ac_param);
		memcpy(pos, &iface->wmm.ac[3], sizeof(struct skw_ac_param));
	}
}

static void
skw_tdls_add_oper_classes(struct net_device *ndev, struct sk_buff *skb)
{
	u8 *pos;
	u8 op_class;
	int freq;
	struct skw_iface *iface = netdev_priv(ndev);
	struct cfg80211_chan_def chandef;
	struct ieee80211_channel *channel, *bss_chn;

	bss_chn = iface->sta.core.bss.channel;
	freq = ieee80211_channel_to_frequency(bss_chn->hw_value, bss_chn->band);
	channel = ieee80211_get_channel(ndev->ieee80211_ptr->wiphy, freq);

	cfg80211_chandef_create(&chandef, channel, NL80211_CHAN_NO_HT);

	if (!skw_chandef_to_operating_class(&chandef, &op_class))
		return;

	pos = skb_put(skb, 4);
	*pos++ = WLAN_EID_SUPPORTED_REGULATORY_CLASSES;
	*pos++ = 2; /* len */

	*pos++ = op_class;
	*pos++ = op_class; /* give current operating class as alternate too */
}

#if 0
u8 *skw_ie_build_ht_cap(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
			      u16 cap)
{
	__le16 tmp;

	*pos++ = WLAN_EID_HT_CAPABILITY;
	*pos++ = sizeof(struct ieee80211_ht_cap);
	memset(pos, 0, sizeof(struct ieee80211_ht_cap));

	/* capability flags */
	tmp = cpu_to_le16(cap);
	memcpy(pos, &tmp, sizeof(u16));
	pos += sizeof(u16);

	/* AMPDU parameters */
	*pos++ = ht_cap->ampdu_factor |
		 (ht_cap->ampdu_density <<
			IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT);

	/* MCS set */
	memcpy(pos, &ht_cap->mcs, sizeof(ht_cap->mcs));
	pos += sizeof(ht_cap->mcs);

	/* extended capabilities */
	pos += sizeof(__le16);

	/* BF capabilities */
	pos += sizeof(__le32);

	/* antenna selection */
	pos += sizeof(u8);

	return pos;
}

u8 *ieee80211_ie_build_vht_cap(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
			       u32 cap)
{
	__le32 tmp;

	*pos++ = WLAN_EID_VHT_CAPABILITY;
	*pos++ = sizeof(struct ieee80211_vht_cap);
	memset(pos, 0, sizeof(struct ieee80211_vht_cap));

	/* capability flags */
	tmp = cpu_to_le32(cap);
	memcpy(pos, &tmp, sizeof(u32));
	pos += sizeof(u32);

	/* VHT MCS set */
	memcpy(pos, &vht_cap->vht_mcs, sizeof(vht_cap->vht_mcs));
	pos += sizeof(vht_cap->vht_mcs);

	return pos;
}
#endif

static void
skw_tdls_add_setup_start_ies(struct net_device *ndev, struct sk_buff *skb,
		const u8 *peer, u32 peer_cap, u8 action_code, bool initiator,
		const u8 *ies, size_t ies_len)
{
	struct ieee80211_supported_band *sband;
	//struct ieee80211_sta_ht_cap ht_cap;
	//struct ieee80211_sta_vht_cap vht_cap;
	size_t offset = 0, noffset;
	struct skw_iface *iface = netdev_priv(ndev);
	//u8 *pos;
	enum nl80211_band band;

	if (iface->sta.core.bss.channel) {
		band = iface->sta.core.bss.channel->band;
	} else {
		skw_err("bss is null\n");
		return;
	}

	sband = iface->wdev.wiphy->bands[band];
	if (!sband)
		return;

	skw_add_srates_ie(ndev, skb, false, band);
	skw_add_ext_srates_ie(ndev, skb, false, band);
	skw_tdls_add_supp_channels(ndev, skb);

	/* Add any custom IEs that go before Extended Capabilities */
	if (ies_len) {
		static const u8 before_ext_cap[] = {
			WLAN_EID_SUPP_RATES,
			WLAN_EID_COUNTRY,
			WLAN_EID_EXT_SUPP_RATES,
			WLAN_EID_SUPPORTED_CHANNELS,
			WLAN_EID_RSN,
		};
		noffset = skw_ie_split_ric(ies, ies_len, before_ext_cap,
				ARRAY_SIZE(before_ext_cap), NULL, 0, offset);
		skw_put_skb_data(skb, ies + offset, noffset - offset);
		offset = noffset;
	}

	skw_tdls_add_ext_capab(ndev, skb);

	/* add the QoS element if we support it */
	if (action_code != WLAN_PUB_ACTION_TDLS_DISCOVER_RES)
		skw_add_wmm_ie(iface, skb, SKW_WMM_TYPE_INFO, NULL);

	/* add any custom IEs that go before HT capabilities */
	if (ies_len) {
		static const u8 before_ht_cap[] = {
			WLAN_EID_SUPP_RATES,
			WLAN_EID_COUNTRY,
			WLAN_EID_EXT_SUPP_RATES,
			WLAN_EID_SUPPORTED_CHANNELS,
			WLAN_EID_RSN,
			WLAN_EID_EXT_CAPABILITY,
			WLAN_EID_QOS_CAPA,
			WLAN_EID_FAST_BSS_TRANSITION,
			WLAN_EID_TIMEOUT_INTERVAL,
			WLAN_EID_SUPPORTED_REGULATORY_CLASSES,
		};
		noffset = skw_ie_split_ric(ies, ies_len, before_ht_cap,
				ARRAY_SIZE(before_ht_cap), NULL, 0,  offset);
		skw_put_skb_data(skb, ies + offset, noffset - offset);
		offset = noffset;
	}

	skw_tdls_add_oper_classes(ndev, skb);
	skw_tdls_add_link_ie(ndev, skb, peer, initiator);

	/* add any remaining IEs */
	if (ies_len) {
		noffset = ies_len;
		skw_put_skb_data(skb, ies + offset, noffset - offset);
	}
}

static void
skw_tdls_add_setup_cfm_ies(struct net_device *ndev,
			struct sk_buff *skb, const u8 *peer,
			u32 peer_cap, bool initiator,
			const u8 *extra_ies, size_t extra_ies_len)
{
	struct skw_iface *iface = netdev_priv(ndev);
	size_t offset = 0, noffset;
	struct ieee80211_supported_band *sband = NULL;
	enum nl80211_band band;

	band = iface->sta.core.bss.channel->band;
	sband = iface->wdev.wiphy->bands[band];

	if (!sband)
		return;

	/* add any custom IEs that go before the QoS IE */
	if (extra_ies_len) {
		static const u8 before_qos[] = {
			WLAN_EID_RSN,
		};
		noffset = skw_ie_split_ric(extra_ies, extra_ies_len,
					     before_qos,
					     ARRAY_SIZE(before_qos),
					     NULL, 0,
					     offset);
		skw_put_skb_data(skb, extra_ies + offset, noffset - offset);
		offset = noffset;
	}
	/* add the QoS param IE if both the peer and we support it */
	if (peer_cap & SKW_TDLS_PEER_WMM)
		skw_add_wmm_ie(iface, skb, SKW_WMM_TYPE_PARAMETER, NULL);

	/* add any custom IEs that go before HT operation */
	if (extra_ies_len) {
		static const u8 before_ht_op[] = {
			WLAN_EID_RSN,
			WLAN_EID_QOS_CAPA,
			WLAN_EID_FAST_BSS_TRANSITION,
			WLAN_EID_TIMEOUT_INTERVAL,
		};
		noffset = skw_ie_split_ric(extra_ies, extra_ies_len,
					     before_ht_op,
					     ARRAY_SIZE(before_ht_op),
					     NULL, 0,
					     offset);
		skw_put_skb_data(skb, extra_ies + offset, noffset - offset);
		offset = noffset;
	}

	skw_tdls_add_link_ie(ndev, skb, peer, initiator);

	/* add any remaining IEs */
	if (extra_ies_len) {
		noffset = extra_ies_len;
		skw_put_skb_data(skb, extra_ies + offset, noffset - offset);
	}
}

static void skw_tdls_add_chan_switch_req_ies(struct net_device *ndev,
				       struct sk_buff *skb, const u8 *peer,
				       bool initiator, const u8 *extra_ies,
				       size_t extra_ies_len, u8 oper_class,
				       struct cfg80211_chan_def *chandef)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	struct ieee80211_tdls_data *tf;
	size_t offset = 0, noffset;

	if (WARN_ON_ONCE(!chandef))
		return;

	tf = (void *)skb->data;
	tf->u.chan_switch_req.target_channel =
		skw_freq_to_chn(chandef->chan->center_freq);
	tf->u.chan_switch_req.oper_class = oper_class;

	if (extra_ies_len) {
		static const u8 before_lnkie[] = {
			WLAN_EID_SECONDARY_CHANNEL_OFFSET,
		};
		noffset = skw_ie_split_ric(extra_ies, extra_ies_len,
					     before_lnkie,
					     ARRAY_SIZE(before_lnkie),
					     NULL, 0,
					     offset);
		skw_put_skb_data(skb, extra_ies + offset, noffset - offset);
		offset = noffset;
	}

	skw_tdls_add_link_ie(ndev, skb, peer, initiator);

	/* add any remaining IEs */
	if (extra_ies_len) {
		noffset = extra_ies_len;
		skw_put_skb_data(skb, extra_ies + offset, noffset - offset);
	}
#endif
}

static void skw_tdls_add_ies(struct net_device *ndev, struct sk_buff *skb,
		const u8 *peer, u8 action, u16 status_code, u32 peer_cap,
		bool initiator, const u8 *ies, size_t ies_len)
{

	switch (action) {
	case WLAN_TDLS_SETUP_REQUEST:
	case WLAN_TDLS_SETUP_RESPONSE:
	case WLAN_PUB_ACTION_TDLS_DISCOVER_RES:
		if (status_code == 0)
			skw_tdls_add_setup_start_ies(ndev, skb, peer, peer_cap,
				action, initiator, ies, ies_len);
		break;
	case WLAN_TDLS_SETUP_CONFIRM:
		if (status_code == 0)
			skw_tdls_add_setup_cfm_ies(ndev, skb, peer, peer_cap,
				initiator, ies, ies_len);
		break;
	case WLAN_TDLS_TEARDOWN:
	case WLAN_TDLS_DISCOVERY_REQUEST:
		if (ies_len)
			skw_put_skb_data(skb, ies, ies_len);

		if (status_code == 0 || action == WLAN_TDLS_TEARDOWN)
			skw_tdls_add_link_ie(ndev, skb, peer, initiator);
		break;
	case WLAN_TDLS_CHANNEL_SWITCH_REQUEST:
		skw_tdls_add_chan_switch_req_ies(ndev, skb, peer,
			initiator, ies, ies_len, 0, NULL);
		break;
	default:
		break;
	}
}

static int
skw_tdls_build_send_encap_data(struct net_device *ndev,
		const u8 *peer, u8 action_code, u8 dialog_token,
		u16 status_code, u32 peer_cap, struct sk_buff *skb,
		bool initiator, const u8 *ies, size_t ies_len)
{
	int offset;
	struct ieee80211_tdls_data *td = NULL;

	offset = offsetof(struct ieee80211_tdls_data, u);
	td = (struct ieee80211_tdls_data *)skb_put(skb, offset);

	memcpy(td->da, peer, ETH_ALEN);
	memcpy(td->sa, ndev->dev_addr, ETH_ALEN);
	td->ether_type = cpu_to_be16(ETH_P_TDLS);
	td->payload_type = WLAN_TDLS_SNAP_RFTYPE;

	skb_set_network_header(skb, ETH_HLEN);

	switch (action_code) {
	case WLAN_TDLS_SETUP_REQUEST:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_SETUP_REQUEST;

		skb_put(skb, sizeof(td->u.setup_req));
		td->u.setup_req.dialog_token = dialog_token;
		td->u.setup_req.capability = 0;
		break;

	case WLAN_TDLS_SETUP_RESPONSE:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_SETUP_RESPONSE;

		skb_put(skb, sizeof(td->u.setup_resp));
		td->u.setup_resp.status_code = cpu_to_le16(status_code);
		td->u.setup_resp.dialog_token = dialog_token;

		td->u.setup_resp.capability = 0;
		break;

	case WLAN_TDLS_SETUP_CONFIRM:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_SETUP_CONFIRM;

		skb_put(skb, sizeof(td->u.setup_cfm));
		td->u.setup_cfm.status_code = cpu_to_le16(status_code);
		td->u.setup_cfm.dialog_token = dialog_token;
		break;

	case WLAN_TDLS_TEARDOWN:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_TEARDOWN;

		skb_put(skb, sizeof(td->u.teardown));
		td->u.teardown.reason_code = cpu_to_le16(status_code);
		break;

	case WLAN_TDLS_DISCOVERY_REQUEST:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_DISCOVERY_REQUEST;

		skb_put(skb, sizeof(td->u.discover_req));
		td->u.discover_req.dialog_token = dialog_token;
		break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	case WLAN_TDLS_CHANNEL_SWITCH_REQUEST:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_CHANNEL_SWITCH_REQUEST;

		skb_put(skb, sizeof(td->u.chan_switch_req));
		break;

	case WLAN_TDLS_CHANNEL_SWITCH_RESPONSE:
		td->category = WLAN_CATEGORY_TDLS;
		td->action_code = WLAN_TDLS_CHANNEL_SWITCH_RESPONSE;

		skb_put(skb, sizeof(td->u.chan_switch_resp));
		td->u.chan_switch_resp.status_code = cpu_to_le16(status_code);
		break;
#endif

	default:
		return -EINVAL;
	}

	skw_tdls_add_ies(ndev, skb, peer, action_code, status_code, peer_cap,
		initiator, ies, ies_len);

	return dev_queue_xmit(skb);
}

static int
skw_tdls_build_send_direct(struct net_device *dev,
		const u8 *peer, u8 action_code, u8 dialog_token,
		u16 status_code, struct sk_buff *skb, bool initiator,
		const u8 *ies, size_t ies_len)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;
	struct wiphy *wiphy = priv_to_wiphy(skw);
	struct ieee80211_mgmt *mgmt;
	int ret, total_len;
	struct skw_mgmt_tx_param *param;

	skw_chip_dbg(skw->idx, "Enter\n");
	mgmt = skw_put_skb_zero(skb, 24);
	memcpy(mgmt->da, peer, ETH_ALEN);
	skw_ether_copy(mgmt->sa, iface->addr);
	skw_ether_copy(mgmt->bssid, iface->sta.core.bss.bssid);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					IEEE80211_STYPE_ACTION);

	switch (action_code) {
	case WLAN_PUB_ACTION_TDLS_DISCOVER_RES:
		skb_put(skb, 1 + sizeof(mgmt->u.action.u.tdls_discover_resp));
		mgmt->u.action.category = WLAN_CATEGORY_PUBLIC;
		mgmt->u.action.u.tdls_discover_resp.action_code =
			WLAN_PUB_ACTION_TDLS_DISCOVER_RES;
		mgmt->u.action.u.tdls_discover_resp.dialog_token =
			dialog_token;
		mgmt->u.action.u.tdls_discover_resp.capability =
			status_code ? 0 : (WLAN_CAPABILITY_SHORT_SLOT_TIME |
			 WLAN_CAPABILITY_SHORT_PREAMBLE);
		break;

	default:
		return -EINVAL;
	}

	skw_tdls_add_ies(dev, skb, peer, action_code, status_code, 0,
		initiator, ies, ies_len);

	skw_chip_dbg(skw->idx, "sending tdls discover response\n");

	total_len = sizeof(*param) + skb->len;
	param = SKW_ZALLOC(total_len, GFP_KERNEL);
	if (!param)
		return -ENOMEM;

	param->channel = 0xFF;
	param->wait = 0;
	param->dont_wait_for_ack = 0;
	param->cookie = 0;

	memcpy(param->mgmt, skb->data, skb->len);
	param->mgmt_frame_len = skb->len;

	skw_hex_dump("mgmt tx", skb->data, skb->len, false);

	down(&skw->cmd.mgmt_cmd_lock);
	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_TX_MGMT,
			param, total_len, NULL, 0);
	up(&skw->cmd.mgmt_cmd_lock);

	SKW_KFREE(param);
	return ret;
}

int skw_tdls_build_send_mgmt(struct skw_core *skw, struct net_device *ndev,
			const u8 *peer, u8 action_code, u8 dialog_token,
			u16 status_code, u32 peer_cap, bool initiator,
			const u8 *ies, size_t ies_len)
{
	struct sk_buff *skb;
	unsigned int skb_len;
	int ret;

	skb_len = skw->skb_headroom +
		  max(sizeof(struct ieee80211_mgmt),
		      sizeof(struct ieee80211_tdls_data)) +
		  50 + /* supported rates */
		  10 + /* ext capab */
		  26 + /* WMM */
		  2 + max(sizeof(struct ieee80211_ht_cap),
			  sizeof(struct ieee80211_ht_operation)) +
		  2 + max(sizeof(struct ieee80211_vht_cap),
			  sizeof(struct ieee80211_vht_operation)) +
		  50 + /* supported channels */
		  3 + /* 40/20 BSS coex */
		  4 + /* AID */
		  4 + /* oper classes */
		  ies_len +
		  sizeof(struct ieee80211_tdls_lnkie);

	skw_chip_dbg(skw->idx, "skb_headroom: %u skb_len: %u ies_len: %lu\n",
		skw->skb_headroom, skb_len, (long)ies_len);

	skb = netdev_alloc_skb(ndev, skb_len);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, skw->skb_headroom);

	switch (action_code) {
	case WLAN_TDLS_SETUP_REQUEST:
	case WLAN_TDLS_SETUP_RESPONSE:
	case WLAN_TDLS_SETUP_CONFIRM:
	case WLAN_TDLS_TEARDOWN:
	case WLAN_TDLS_DISCOVERY_REQUEST:
	case WLAN_TDLS_CHANNEL_SWITCH_REQUEST:
	case WLAN_TDLS_CHANNEL_SWITCH_RESPONSE:
		ret = skw_tdls_build_send_encap_data(ndev, peer,
				action_code, dialog_token, status_code,
				peer_cap, skb, initiator, ies, ies_len);
		break;

	case WLAN_PUB_ACTION_TDLS_DISCOVER_RES:
		ret = skw_tdls_build_send_direct(ndev, peer, action_code,
				dialog_token, status_code, skb, initiator,
				ies, ies_len);
		break;

	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
