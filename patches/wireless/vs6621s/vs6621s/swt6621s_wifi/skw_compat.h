/* SPDX-License-Identifier: GPL-2.0 */

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

#ifndef __SKW_COMPAT_H__
#define __SKW_COMPAT_H__

#include <linux/version.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>

/* EID block */
#define SKW_WLAN_EID_EXT_HE_CAPABILITY                            35
#define SKW_WLAN_EID_EXT_HE_OPERATION                             36
#define SKW_WLAN_EID_MULTI_BSSID_IDX                              85
#define SKW_WLAN_EID_EXTENSION                                    255
#define SKW_WLAN_EID_FRAGMENT                                     242

/* compat for ieee80211 */
#define SKW_HE_MAC_CAP0_HTC_HE                                    0x01
#define SKW_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US                       0x08
#define SKW_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8                    0x70

#define SKW_HE_MAC_CAP2_BSR                                       0x08
#define SKW_HE_MAC_CAP2_MU_CASCADING                              0x40
#define SKW_HE_MAC_CAP2_ACK_EN                                    0x80

#define SKW_HE_MAC_CAP3_GRP_ADDR_MULTI_STA_BA_DL_MU               0x01
#define SKW_HE_MAC_CAP3_OMI_CONTROL                               0x02
#define SKW_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_VHT_2                   0x10

#define SKW_HE_MAC_CAP4_AMDSU_IN_AMPDU                            0x40

#define SKW_HE_PHY_CAP0_DUAL_BAND                                 0x01
#define SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G             0x02
#define SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G       0x04
#define SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G            0x08
#define SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G      0x10

#define SKW_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK                     0x0f
#define SKW_HE_PHY_CAP1_DEVICE_CLASS_A                            0x10
#define SKW_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD                    0x20
#define SKW_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS                   0X80

#define SKW_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US                      0x02
#define SKW_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ                       0x04
#define SKW_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ                       0x08
#define SKW_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO                        0x40
#define SKW_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO                     0x80

#define SKW_WLAN_REASON_TDLS_TEARDOWN_UNREACHABLE                 25
#define SKW_WLAN_CATEGORY_RADIO_MEASUREMENT                       5

#define SKW_IEEE80211_CHAN_NO_20MHZ                               BIT(11)
/* end of compat for ieee80211 */

#define SKW_WIPHY_FEATURE_SCAN_RANDOM_MAC                         BIT(29)
#define SKW_EXT_CAPA_BSS_TRANSITION                               19
#define SKW_EXT_CAPA_MBSSID                                       22
#define SKW_EXT_CAPA_TDLS_SUPPORT                                 37
#define SKW_EXT_CAPA_TWT_REQ_SUPPORT                              77

#define SKW_BSS_MEMBERSHIP_SELECTOR_HT_PHY                        127
#define SKW_BSS_MEMBERSHIP_SELECTOR_VHT_PHY                       126

#ifndef MIN_NICE
#define MIN_NICE                                                  -20
#endif

#ifndef TASK_IDLE
#define TASK_IDLE                                                 TASK_INTERRUPTIBLE
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#define SKW_BSS_TYPE_ESS                              IEEE80211_BSS_TYPE_ESS
#define SKW_BSS_TYPE_IBSS                             IEEE80211_BSS_TYPE_IBSS
#define SKW_PRIVACY_ESS_ANY                           IEEE80211_PRIVACY_ANY
#define SKW_PRIVACY_IBSS_ANY                          IEEE80211_PRIVACY_ANY
#else
#define SKW_BSS_TYPE_ESS                              WLAN_CAPABILITY_ESS
#define SKW_BSS_TYPE_IBSS                             WLAN_CAPABILITY_IBSS
#define SKW_PRIVACY_ESS_ANY                           WLAN_CAPABILITY_ESS
#define SKW_PRIVACY_IBSS_ANY                          WLAN_CAPABILITY_ESS
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define SKW_PASSIVE_SCAN (IEEE80211_CHAN_NO_IR | IEEE80211_CHAN_RADAR)
#else
#define SKW_PASSIVE_SCAN IEEE80211_CHAN_PASSIVE_SCAN
#endif

#define skw_from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#define skw_compat_setup_timer(timer, fn) \
	timer_setup(timer, fn, 0)
#else
typedef void (*tfunc)(unsigned long);
#define skw_compat_setup_timer(timer, fn) \
	setup_timer(timer, (tfunc)fn, (unsigned long)timer)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
enum nl80211_bss_scan_width {
	NL80211_BSS_CHAN_WIDTH_20,
	NL80211_BSS_CHAN_WIDTH_10,
	NL80211_BSS_CHAN_WIDTH_5,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
#define NUM_NL80211_BANDS    3
#endif

#if (KERNEL_VERSION(4, 6, 0) <= LINUX_VERSION_CODE)
#define SKW_IS_COMPAT_TASK in_compat_syscall
#else
#define SKW_IS_COMPAT_TASK is_compat_task
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static inline struct sk_buff *
skw_compat_vendor_event_alloc(struct wiphy *wiphy, struct wireless_dev *wdev,
			int roxlen, int event_idx, gfp_t gfp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	return cfg80211_vendor_event_alloc(wiphy, wdev, roxlen, event_idx, gfp);
#else
	return cfg80211_vendor_event_alloc(wiphy, roxlen, event_idx, gfp);
#endif
}
#endif

static inline void skw_compat_cqm_rssi_notify(struct net_device *dev,
			enum nl80211_cqm_rssi_threshold_event rssi_event,
			s32 rssi_level, gfp_t gfp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	cfg80211_cqm_rssi_notify(dev, rssi_event, rssi_level, gfp);
#else
	cfg80211_cqm_rssi_notify(dev, rssi_event, gfp);
#endif
}

static inline void skw_compat_page_frag_free(void *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	page_frag_free(addr);
#else
	put_page(virt_to_head_page(addr));
#endif
}

static inline void
skw_compat_scan_done(struct cfg80211_scan_request *req, bool aborted)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	struct cfg80211_scan_info info = {
		.aborted = aborted,
	};

	cfg80211_scan_done(req, &info);
#else
	cfg80211_scan_done(req, aborted);
#endif
}

static inline void skw_compat_disconnected(struct net_device *ndev, u16 reason,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
				   u8 *ie,
#else
				   const u8 *ie,
#endif
				   size_t ie_len,
				   bool locally_generated, gfp_t gfp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	cfg80211_disconnected(ndev, reason, ie, ie_len, locally_generated, gfp);
#else
	cfg80211_disconnected(ndev, reason, ie, ie_len, gfp);
#endif
}

static inline bool skw_compat_reg_can_beacon(struct wiphy *wiphy,
			     struct cfg80211_chan_def *chandef,
			     enum nl80211_iftype iftype)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	return cfg80211_reg_can_beacon_relax(wiphy, chandef, iftype);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	return cfg80211_reg_can_beacon(wiphy, chandef, iftype);
#else
	return cfg80211_reg_can_beacon(wiphy, chandef);
#endif
}

static inline unsigned int
skw_compat_classify8021d(struct sk_buff *skb, void *qos_map)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	return cfg80211_classify8021d(skb);
#else
	return cfg80211_classify8021d(skb, qos_map);
#endif
}

static inline void
skw_compat_rx_mlme_mgmt(struct net_device *dev, void *buf, size_t len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_rx_mlme_mgmt(dev, buf, len);
#else
	//FIXME: Dead lock wdev_lock->sta_core
	struct ieee80211_mgmt *mgmt = buf;

	mutex_unlock(&dev->ieee80211_ptr->mtx);

	if (ieee80211_is_auth(mgmt->frame_control))
		cfg80211_send_rx_auth(dev, buf, len);
	else if (ieee80211_is_deauth(mgmt->frame_control))
		cfg80211_send_deauth(dev, buf, len);
	else if (ieee80211_is_disassoc(mgmt->frame_control))
		cfg80211_send_disassoc(dev, buf, len);

	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline const u8 *skw_compat_bssid(struct cfg80211_connect_params *req)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	return req->bssid ? req->bssid : req->bssid_hint;
#else
	return req->bssid;
#endif
}

static inline struct ieee80211_channel *
skw_compat_channel(struct cfg80211_connect_params *req)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	return req->channel ? req->channel : req->channel_hint;
#else
	return req->channel;
#endif
}

// fixme:
// disable interface combination check for debug
#if 0
static inline int skw_compat_check_combs(struct wiphy *wiphy, int nr_channels,
				u8 radar, int type_num[NUM_NL80211_IFTYPES])
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	struct iface_combination_params params = {
		.num_different_channels = nr_channels,
		.radar_detect = radar,
	};

	memcpy(params.iftype_num, type_num, NUM_NL80211_IFTYPES * sizeof(int));

	return cfg80211_check_combinations(wiphy, &params);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	return cfg80211_check_combinations(wiphy, nr_channels, radar, type_num);
#else
	// TODO:
	// implement function to check combinations
	return 0;
#endif
}
#else
static inline int skw_compat_check_combs(struct wiphy *wiphy, int nr_channels,
				u8 radar, int type_num[NUM_NL80211_IFTYPES])
{
	return 0;
}
#endif

static inline void skw_compat_auth_timeout(struct net_device *dev, const u8 *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_auth_timeout(dev, addr);
#else
	mutex_unlock(&dev->ieee80211_ptr->mtx);
	cfg80211_send_auth_timeout(dev, addr);
	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline void skw_cfg80211_tx_mlme_mgmt(struct net_device *dev,
				const u8 *buf, size_t len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	cfg80211_tx_mlme_mgmt(dev, buf, len, true);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_tx_mlme_mgmt(dev, buf, len);
#else
	struct ieee80211_mgmt *mgmt = (void *)buf;

	if (ieee80211_is_deauth(mgmt->frame_control))
		__cfg80211_send_deauth(dev, buf, len);
	else
		__cfg80211_send_disassoc(dev, buf, len);
#endif
}

static inline void skw_compat_rtc_time_to_tm(unsigned long time, struct rtc_time *tm)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
	rtc_time_to_tm(time, tm);
#else
	rtc_time64_to_tm(time, tm);
#endif
}

static inline bool skw_compat_cfg80211_rx_mgmt(struct wireless_dev *wdev,
				int freq, int sig_dbm, const u8 *buf,
				size_t len, u32 flags, gfp_t gfp)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
	return cfg80211_rx_mgmt(wdev, freq, sig_dbm, buf, len, gfp);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
	return cfg80211_rx_mgmt(wdev, freq, sig_dbm, buf, len, flags, gfp);
#else
	return cfg80211_rx_mgmt(wdev, freq, sig_dbm, buf, len, flags);
#endif
}

static inline void *skw_pde_data(const struct inode *inode)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	return pde_data(inode);
#else
	return PDE_DATA(inode);
#endif
}

static inline int skw_set_wiphy_regd_sync(struct wiphy *wiphy,
				struct ieee80211_regdomain *rd)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	return regulatory_set_wiphy_regd_sync(wiphy, rd);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	return regulatory_set_wiphy_regd_sync_rtnl(wiphy, rd);
#else
	return -EINVAL;
#endif
}

#ifdef __SKW_ANDROID__
static inline void skw_compat_rx_assoc_resp(struct net_device *dev,
			struct cfg80211_bss *bss, const u8 *buf, size_t len,
			int uapsd, const u8 *req_ies, size_t req_ies_len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
	struct cfg80211_rx_assoc_resp_data assoc_resp = {
		.buf = buf,
		.len = len,
		.req_ies = req_ies,
		.req_ies_len = req_ies_len,
		.ap_mld_addr = NULL,
		.links[0] = {
			.bss = bss,
		},
	};

	cfg80211_rx_assoc_resp(dev, &assoc_resp);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)
	struct cfg80211_rx_assoc_resp assoc_resp = {
		.buf = buf,
		.len = len,
		.req_ies = req_ies,
		.req_ies_len = req_ies_len,
		.ap_mld_addr = NULL,
		.links[0] = {
			.bss = bss,
		},
	};

	cfg80211_rx_assoc_resp(dev, &assoc_resp);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len, uapsd, req_ies, req_ies_len);
#elif defined SKW_RX_ASSOC_RESP_EXT
	cfg80211_rx_assoc_resp_ext(dev, bss, buf, len, uapsd, req_ies, req_ies_len);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len, uapsd);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len);
#else
	mutex_unlock(&dev->ieee80211_ptr->mtx);
	cfg80211_send_rx_assoc(dev, bss, buf, len);
	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline void skw_compat_assoc_failure(struct net_device *dev,
				struct cfg80211_bss *bss, bool timeout)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)
	struct cfg80211_assoc_failure info = {
		.ap_mld_addr = NULL,
		.bss[0] = bss,
		.timeout = timeout,
	};

	cfg80211_assoc_failure(dev, &info);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 2) || LINUX_VERSION_CODE == KERNEL_VERSION(4, 8, 17))
	if (timeout)
		cfg80211_assoc_timeout(dev, bss);
	else
		cfg80211_abandon_assoc(dev, bss);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_assoc_timeout(dev, bss);
#else
	mutex_unlock(&dev->ieee80211_ptr->mtx);
	cfg80211_send_assoc_timeout(dev, bss->bssid);
	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline void skw_compat_cfg80211_roamed(struct net_device *dev,
			const u8 *bssid, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, gfp_t gfp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41)
	struct cfg80211_roam_info roam_info = {
		.req_ie = req_ie,
		.req_ie_len = req_ie_len,
		.resp_ie = resp_ie,
		.resp_ie_len = resp_ie_len,
		.valid_links = 0,
		.links[0] = {
			.bss = NULL,
			.bssid = bssid,
		},
	};

	cfg80211_roamed(dev, &roam_info, gfp);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	struct cfg80211_roam_info roam_info = {
		.bss = NULL,
		.bssid = bssid,
		.req_ie = req_ie,
		.req_ie_len = req_ie_len,
		.resp_ie = resp_ie,
		.resp_ie_len = resp_ie_len,
	};

	cfg80211_roamed(dev, &roam_info, gfp);
#else
	// fixme:
	// fix channel
	cfg80211_roamed(dev, NULL, bssid, req_ie, req_ie_len,
			resp_ie, resp_ie_len, gfp);
#endif
}

static inline void skw_ch_switch_started_notify(struct net_device *dev,
		struct cfg80211_chan_def *chandef, u8 count,  bool quiet)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)
	cfg80211_ch_switch_started_notify(dev, chandef, 0, count, quiet);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 35)
	cfg80211_ch_switch_started_notify(dev, chandef, count, quiet);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	cfg80211_ch_switch_started_notify(dev, chandef, count);
#else
#endif
}

static inline void skw_ch_switch_notify(struct net_device *dev,
		struct cfg80211_chan_def *chandef)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)
	cfg80211_ch_switch_notify(dev, chandef, 0);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41)
	cfg80211_ch_switch_notify(dev, chandef, 0);
#else
	cfg80211_ch_switch_notify(dev, chandef);
#endif

}

#else /* Linux */
static inline void skw_compat_rx_assoc_resp(struct net_device *dev,
			struct cfg80211_bss *bss, const u8 *buf, size_t len,
			int uapsd, const u8 *req_ies, size_t req_ies_len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
	struct cfg80211_rx_assoc_resp_data assoc_resp = {
		.buf = buf,
		.len = len,
		.req_ies = req_ies,
		.req_ies_len = req_ies_len,
		.ap_mld_addr = NULL,
		.links[0] = {
			.bss = bss,
		},
	};

	cfg80211_rx_assoc_resp(dev, &assoc_resp);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	struct cfg80211_rx_assoc_resp assoc_resp = {
		.buf = buf,
		.len = len,
		.req_ies = req_ies,
		.req_ies_len = req_ies_len,
		.ap_mld_addr = NULL,
		.links[0] = {
			.bss = bss,
		},
	};

	cfg80211_rx_assoc_resp(dev, &assoc_resp);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len, uapsd, req_ies, req_ies_len);
#elif defined SKW_RX_ASSOC_RESP_EXT
	cfg80211_rx_assoc_resp_ext(dev, bss, buf, len, uapsd, req_ies, req_ies_len);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len, uapsd);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_rx_assoc_resp(dev, bss, buf, len);
#else
	mutex_unlock(&dev->ieee80211_ptr->mtx);
	cfg80211_send_rx_assoc(dev, bss, buf, len);
	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline void skw_compat_assoc_failure(struct net_device *dev,
				struct cfg80211_bss *bss, bool timeout)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	struct cfg80211_assoc_failure info = {
		.ap_mld_addr = NULL,
		.bss[0] = bss,
		.timeout = timeout,
	};

	cfg80211_assoc_failure(dev, &info);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 2) || LINUX_VERSION_CODE == KERNEL_VERSION(4, 8, 17))
	if (timeout)
		cfg80211_assoc_timeout(dev, bss);
	else
		cfg80211_abandon_assoc(dev, bss);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	cfg80211_assoc_timeout(dev, bss);
#else
	mutex_unlock(&dev->ieee80211_ptr->mtx);
	cfg80211_send_assoc_timeout(dev, bss->bssid);
	mutex_lock(&dev->ieee80211_ptr->mtx);
#endif
}

static inline void skw_compat_cfg80211_roamed(struct net_device *dev,
			const u8 *bssid, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, gfp_t gfp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	struct cfg80211_roam_info roam_info = {
		.req_ie = req_ie,
		.req_ie_len = req_ie_len,
		.resp_ie = resp_ie,
		.resp_ie_len = resp_ie_len,
		.valid_links = 0,
		.links[0] = {
			.bss = NULL,
			.bssid = bssid,
		},
	};

	cfg80211_roamed(dev, &roam_info, gfp);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	struct cfg80211_roam_info roam_info = {
		.bss = NULL,
		.bssid = bssid,
		.req_ie = req_ie,
		.req_ie_len = req_ie_len,
		.resp_ie = resp_ie,
		.resp_ie_len = resp_ie_len,
	};

	cfg80211_roamed(dev, &roam_info, gfp);
#else
	// fixme:
	// fix channel
	cfg80211_roamed(dev, NULL, bssid, req_ie, req_ie_len,
			resp_ie, resp_ie_len, gfp);
#endif
}

static inline void skw_ch_switch_started_notify(struct net_device *dev,
		struct cfg80211_chan_def *chandef, u8 count,  bool quiet)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cfg80211_ch_switch_started_notify(dev, chandef, 0, count, quiet);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	cfg80211_ch_switch_started_notify(dev, chandef, count, quiet);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	cfg80211_ch_switch_started_notify(dev, chandef, count);
#else
#endif
}

static inline void skw_ch_switch_notify(struct net_device *dev,
		struct cfg80211_chan_def *chandef)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 2)
	cfg80211_ch_switch_notify(dev, chandef, 0);
#else
	cfg80211_ch_switch_notify(dev, chandef);
#endif
}

#endif /* Linux */

static inline unsigned long skw_get_seconds(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	return ktime_get_real_seconds();
#else
	return get_seconds();
#endif
}

static inline int skw_register_netdevice(struct net_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	return cfg80211_register_netdevice(dev);
#else
	return register_netdevice(dev);
#endif
}

static inline void skw_unregister_netdevice(struct net_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	return cfg80211_unregister_netdevice(dev);
#else
	return unregister_netdevice(dev);
#endif
}

static inline int skw_compat_cfg80211_chandef_dfs_required(struct wiphy *wiphy,
				  const struct cfg80211_chan_def *chandef,
				  enum nl80211_iftype iftype)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	return cfg80211_chandef_dfs_required(wiphy, chandef, iftype);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	return cfg80211_chandef_dfs_required(wiphy, chandef);
#else
	return 0;
#endif
}

static inline void skw_compat_netif_napi_add_weight(struct net_device *dev,
		 struct napi_struct *napi, int (*poll)(struct napi_struct *, int), int weight)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	netif_napi_add_weight(dev, napi, poll, weight);
#else
	netif_napi_add(dev, napi, poll, weight);
#endif
}

static inline int skw_compat_request_firmware(const struct firmware **fw,
			    const char *name, struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	return request_firmware_direct(fw, name, dev);
#else
	return request_firmware(fw, name, dev);
#endif
}

#endif
