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

#ifndef __SKW_IFACE_H__
#define __SKW_IFACE_H__

#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include "skw_work.h"
#include "skw_util.h"
#include "skw_dfs.h"

#ifndef IEEE80211_CCMP_PN_LEN
#define IEEE80211_CCMP_PN_LEN		6
#endif

#define SKW_PN_LEN                      6
#define SKW_NR_TID                      8
#define SKW_MAX_DEFRAG_ENTRY            4

/* enable 80211W */
#define SKW_NUM_DEFAULT_KEY             4
#define SKW_NUM_DEFAULT_MGMT_KEY        2

/* SKW_NUM_DEFAULT_KEY + SKW_NUM_DEFAULT_MGMT_KEY */
#define SKW_NUM_MAX_KEY                 6

#define SKW_INVALID_ID                  0xff
#define SKW_PEER_ALIGN                  32

#define SKW_PEER_FLAG_TAINT             BIT(0)
#define SKW_PEER_FLAG_BAD_ID            BIT(1)
#define SKW_PEER_FLAG_BAD_ADDR          BIT(2)
#define SKW_PEER_FLAG_ACTIVE            BIT(3)
#define SKW_PEER_FLAG_DEAUTHED          BIT(4)

#define SKW_IFACE_FLAG_LEGACY_P2P_DEV   BIT(0)
#define SKW_IFACE_FLAG_DEAUTH           BIT(1)
#define SKW_IFACE_FLAG_OPENED           BIT(2)
#define SKW_IFACE_FLAG_AP_STARTED       BIT(3)
#define SKW_IFACE_FLAG_BUF_KEY          BIT(4)

#define SKW_IFACE_STA_ROAM_FLAG_CQM_LOW		BIT(0)
/**
 * enum SKW_STATES - STA state
 *
 * @SKW_STATE_NONE: STA exists without special state
 * @SKW_STATE_AUTHING: STA is trying to authentiacate with a BSS
 * @SKW_STATE_AUTHED: STA is authenticated
 * @SKW_STATE_ASSOCING: STA is trying to assoc with a BSS
 * @SKW_STATE_ASSOCED: STA is associated
 * @SKW_STATE_COMPLETED, STA connection is compeleted
 */
enum  SKW_STATES {
	SKW_STATE_NONE,
	SKW_STATE_AUTHING,
	SKW_STATE_AUTHED,
	SKW_STATE_ASSOCING,
	SKW_STATE_ASSOCED,
	SKW_STATE_COMPLETED,
};

enum  SKW_STA_WORK_RETRY_STATE {
	SKW_RETRY_NONE,
	SKW_RETRY_AUTH,
	SKW_RETRY_ASSOC,
};

enum SKW_WMM_AC {
	SKW_WMM_AC_VO = 0,
	SKW_WMM_AC_VI,
	SKW_WMM_AC_BE,
	SKW_WMM_AC_BK,
	SKW_WMM_AC_MAX,
};

#define SKW_ACK_TXQ                      SKW_WMM_AC_MAX

#define SKW_FRAG_STATUS_ACTIVE           BIT(0)
#define SKW_FRAG_STATUS_CHK_PN           BIT(1)

enum skw_wireless_mode {
	SKW_WIRELESS_11B = 1,
	SKW_WIRELESS_11G,
	SKW_WIRELESS_11A,
	SKW_WIRELESS_11N,
	SKW_WIRELESS_11AC,
	SKW_WIRELESS_11AX,
	SKW_WIRELESS_11G_ONLY,
	SKW_WIRELESS_11N_ONLY,
};

enum interface_mode {
	SKW_NONE_MODE = 0,
	SKW_STA_MODE = 1,
	SKW_AP_MODE = 2,
	SKW_GC_MODE = 3,
	SKW_GO_MODE = 4,
	SKW_P2P_DEV_MODE = 5,
	SKW_IBSS_MODE = 6,
	SKW_MONITOR_MODE = 7,

	MAX_MODE_TYPE,
};

enum SKW_CHAN_BW_INFO {
	SKW_CHAN_WIDTH_20,
	SKW_CHAN_WIDTH_40,
	SKW_CHAN_WIDTH_80,
	SKW_CHAN_WIDTH_80P80,
	SKW_CHAN_WIDTH_160,

	SKW_CHAN_WIDTH_MAX,
};

enum skw_rate_info_flags {
	SKW_RATE_INFO_FLAGS_LEGACY,
	SKW_RATE_INFO_FLAGS_HT,
	SKW_RATE_INFO_FLAGS_VHT,
	SKW_RATE_INFO_FLAGS_HE,
};

#define SKW_OPEN_FLAG_OFFCHAN_TX         BIT(0)
struct skw_open_dev_param {
	u16 mode;
	u16 flags; /* reference SKW_OPEN_FLAG_ */
	u8 mac_addr[6];
} __packed;

struct skw_frag_entry {
	u8 id;
	u8 status; /* reference SKW_FRAG_STATUS */
	u16 pending_len;
	u8 tid;
	u8 frag_num;
	u16 sn;
	unsigned long start;
	struct sk_buff_head skb_list;

	/* PN of the last fragment if CCMP was used */
	u8 last_pn[IEEE80211_CCMP_PN_LEN];
};

struct skw_key {
	struct rcu_head rcu;
	u32 key_len;
	u8 key_data[WLAN_MAX_KEY_LEN];
	u8 rx_pn[IEEE80211_NUM_TIDS][SKW_PN_LEN];
};

#define SKW_KEY_FLAG_WEP_SHARE        BIT(0)
#define SKW_KEY_FLAG_WEP_UNICAST      BIT(1)
#define SKW_KEY_FLAG_WEP_MULTICAST    BIT(2)

struct skw_key_conf {
	u8 skw_cipher;
	u8 installed_bitmap;
	u8 flags; /* reference to SKW_KEY_FLAG_ */
	u8 wep_idx;
	struct mutex lock;
	struct skw_key __rcu *key[SKW_NUM_MAX_KEY];
};

struct skw_tid_rx {
	u16 win_start;
	u16 win_size;
	u32 stored_num;
	int ref_cnt;
	struct rcu_head rcu_head;
	struct skw_reorder_rx *reorder;
	struct sk_buff_head *reorder_buf;
};

struct skw_rx_todo {
	spinlock_t lock;
	struct list_head list;
	u16 seq;
	u16 reason;
	bool actived;
};

struct skw_rx_timer {
	u16 sn;
	u16 resv;
	int ref_cnt;
};

struct skw_reorder_rx {
	u32 tid: 4;
	u32 inst: 2;
	u32 peer_idx: 5;
	u32 resv: 21;

	atomic_t ref_cnt;
	struct skw_core *skw;
	struct skw_peer *peer;
	struct timer_list timer;
	struct skw_rx_timer expired;

	struct skw_rx_todo todo;

	spinlock_t lock;
	struct skw_tid_rx __rcu *tid_rx;
};

struct skw_ctx_entry {
	u8 idx;
	u8 padding;
	u8 addr[ETH_ALEN];
	struct rcu_head rcu;
	struct skw_peer *peer;
};

#define SKW_SM_FLAG_SAE_RX_CONFIRM     BIT(0)

struct skw_sm {
	u8 *addr;
	u8 inst;
	u8 iface_iftype;
	u16 flags; /* reference SKW_SM_FLAG_ */
	enum SKW_STATES state;
	enum SKW_STA_WORK_RETRY_STATE rty_state;
};

struct skw_txba_ctrl {
	u16 bitmap;
	u16 blacklist;
	u8 tx_try[SKW_NR_TID];
};

enum skw_msdu_filter {
	SKW_MSDU_FILTER_SUCCESS,
	SKW_MSDU_FILTER_SNAP_MISMATCH,
	SKW_MSDU_FILTER_ARP,
	SKW_MSDU_FILTER_VLAN,
	SKW_MSDU_FILTER_WAPI,
	SKW_MSDU_FILTER_EAP = 5,
	SKW_MSDU_FILTER_PPPOE,
	SKW_MSDU_FILTER_TDLS,
	SKW_MSDU_FILTER_DHCP = 11,
	SKW_MSDU_FILTER_DHCPV6 = 12,
};

#define SKW_RX_FILTER_NONE      0
#define SKW_RX_FILTER_SET       (BIT(SKW_MSDU_FILTER_EAP) | BIT(SKW_MSDU_FILTER_WAPI))

#define SKW_RX_FILTER_EXCL      (BIT(SKW_MSDU_FILTER_EAP) |  \
				 BIT(SKW_MSDU_FILTER_WAPI) | \
				 BIT(SKW_MSDU_FILTER_ARP) |  \
				 BIT(SKW_MSDU_FILTER_DHCP) | \
				 BIT(SKW_MSDU_FILTER_DHCPV6))

#define SKW_RX_FILTER_DBG       (BIT(SKW_MSDU_FILTER_EAP) |  \
				 BIT(SKW_MSDU_FILTER_WAPI) | \
				 BIT(SKW_MSDU_FILTER_ARP) |  \
				 BIT(SKW_MSDU_FILTER_DHCP) | \
				 BIT(SKW_MSDU_FILTER_DHCPV6))

enum SKW_RX_MPDU_DESC_PPDUMODE {
	SKW_PPDUMODE_11B_SHORT = 0,
	SKW_PPDUMODE_11B_LONG,
	SKW_PPDUMODE_11G,
	SKW_PPDUMODE_HT_MIXED,
	SKW_PPDUMODE_VHT_SU,
	SKW_PPDUMODE_VHT_MU,
	SKW_PPDUMODE_HE_SU,
	SKW_PPDUMODE_HE_TB,
	SKW_PPDUMODE_HE_ER_SU,
	SKW_PPDUMODE_HE_MU,
};

struct skw_stats_info {
	s16 rssi;
	u64 pkts;
	u64 bytes;
	u64 drops;
	u64 cal_time;
	u64 cal_bytes;
	u8  tx_psr;
	u32 tx_failed;
	u16 filter_cnt[35];
	u16 filter_drop_offload_cnt[35];
	u8 percent;
	struct skw_rate rate;
};

struct skw_peer {
	u8 idx;
	u8 flags; /* reference SKW_PEER_FLAG_ */
	u8 addr[ETH_ALEN];
	u16 channel;
	u16 rx_tid_map;
	__be32 ip_addr;

	atomic_t rx_filter;
	struct skw_sm sm;
	struct skw_iface *iface;
	struct skw_key_conf ptk_conf, gtk_conf;

	struct skw_txba_ctrl txba;
	struct skw_reorder_rx reorder[SKW_NR_TID];
	struct skw_stats_info tx, rx;
};

struct skw_bss_cfg {
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	u8 ctx_idx;
	u8 bssid[ETH_ALEN];

	enum nl80211_auth_type auth_type;
	enum SKW_CHAN_BW_INFO  width;
	enum SKW_CHAN_BW_INFO  ht_cap_chwidth;
	struct cfg80211_crypto_settings crypto;

	struct ieee80211_channel *channel;
	struct ieee80211_ht_cap *ht_cap;
	struct ieee80211_vht_cap *vht_cap;
};

struct skw_survey_data {
	u32 time;
	u32 time_busy;
	u32 time_ext_busy;
	u8 chan;
	u8 band;
	s8 noise;
	u8 resv;
} __packed;

struct skw_survey_info {
	struct list_head list;
	struct skw_survey_data data;
};

struct skw_ac_param {
	u8 aifsn:4;
	u8 acm:1;
	u8 aci:2;
	u8 recv:1;
	u8 ecw;
	u16 txop_limit;
} __packed;

struct skw_wmm {
	u8 id;
	u8 len;
	u8 oui[3];
	u8 type;
	u8 sub_type;
	u8 version;
	u8 qos;
	u8 resv;
	struct skw_ac_param ac[SKW_WMM_AC_MAX];
} __packed;

struct skw_list {
	int count;
	spinlock_t lock;
	struct list_head list;
};

struct skw_peer_ctx {
	int idx;
	struct mutex lock;
	struct skw_peer *peer;
	struct skw_ctx_entry __rcu *entry;
};

struct skw_iftype_ext_cap {
	u8 iftype;
	u8 ext_cap[10];
	u8 ext_cap_len;
};

struct skw_ctx_pending {
	unsigned long ctx_start;
	unsigned long step_start;
	unsigned long ctx_to;
	u8 *auth_cmd;
	int auth_cmd_len;
	u8 *assoc_cmd;
	int assoc_cmd_len;
	int retry;
	int redo;
	enum nl80211_auth_type auth_type;
};

struct skw_sta_core {
	struct mutex lock;
	struct timer_list timer;
	struct skw_ctx_pending pending;

	struct skw_sm sm;
	struct skw_bss_cfg bss;

	unsigned long auth_start;

	u8 *assoc_req_ie;
	u32 assoc_req_ie_len;

	struct cfg80211_bss *cbss;
};

struct skw_wmm_info {
	u8 acm;
	bool qos_enabled;
	s8 factor[SKW_WMM_AC_MAX];
	struct skw_ac_param ac[SKW_WMM_AC_MAX];
};

#define SKW_MAX_BUF_KEYS     4
struct skw_key_params {
	u8 mac_addr[ETH_ALEN];
	u8 key_type;
	u8 cipher_type;
	u8 pn[6];
	u8 key_id;
	u8 key_len;
	u8 key[WLAN_MAX_KEY_LEN];
} __packed;


#define SKW_AID_DWORD BITS_TO_LONGS(64)

struct skw_monitor_dbg_iface {
	u8 addr[ETH_ALEN];
	struct net_device *ndev;
	u32 frame_cnt;
};

struct skw_iface {
	u8 id;
	u8 lmac_id;
	u8 addr[ETH_ALEN];

	atomic_t peer_map;
	atomic_t actived_ctx;

	struct mutex lock;
	struct skw_core *skw;
	struct net_device *ndev;
	struct wireless_dev wdev;
	struct list_head survey_list;
	struct skw_key_conf key_conf;
	struct cfg80211_qos_map *qos_map;
	struct skw_event_work event_work;
	struct proc_dir_entry *procfs;
	struct skw_wmm_info wmm;

	u8 flags;  /* reference SKW_IFACE_FLAG_ */
	u8 rand_mac_oui[3];
	u8 buf_keys_idx;
	s16 default_multicast;
	u16 mgmt_frame_bitmap;
	int cpu_id;

	struct sk_buff_head txq[SKW_WMM_AC_MAX + 1];
	struct sk_buff_head tx_cache[SKW_WMM_AC_MAX + 1];
	struct skw_frag_entry frag[SKW_MAX_DEFRAG_ENTRY];
	struct skw_key_params buf_keys[SKW_MAX_BUF_KEYS];

	struct {
		enum skw_wireless_mode wireless_mode;
		u16 scan_band_filter;
		u16 resv;
	} extend;

	union {
		struct {
			bool sme_external;
			struct skw_bss_cfg cfg;

			u8 max_sta_allowed;
			struct cfg80211_acl_data *acl;

			bool ht_required, vht_required;

			/* sme external */
			struct skw_list mlme_client_list;
			unsigned long aid_map[SKW_AID_DWORD];

			u8 *probe_resp;
			size_t probe_resp_len;
			int ap_isolate;

			struct {
				u32 cac_time_ms;
				unsigned long flags;
				struct delayed_work cac_work;
			} dfs;
		} sap;

		struct {
			bool sme_external;
			bool is_roam_connect;
			bool is_wep;
			bool report_deauth;
			struct skw_sta_core core;
			struct work_struct work;
			struct skw_connect_param *conn;

			struct {
				spinlock_t lock;
				u8 flags;
				u8 target_bssid[ETH_ALEN];
				u8 target_chn;
			} roam_data;

			u16 last_seq_ctrl;
			__be32 ipv4;
		} sta;

		struct {
			u8 ssid[IEEE80211_MAX_SSID_LEN];
			u8 bssid[ETH_ALEN];
			u8 ssid_len;
			u8 bw;
			u16 flags;
			bool joined;
			u8 channel;
			u8 band;
			u16 beacon_int;
			u32 center_freq1;
			u32 center_freq2;
			struct cfg80211_chan_def chandef;
		} ibss;
	};
};

bool skw_acl_allowed(struct skw_iface *iface, u8 *addr);

static inline const char *skw_state_name(enum SKW_STATES state)
{
	static const char * const st_name[] = {"NONE", "AUTHING", "AUTHED",
					"ASSOCING", "ASSOCED", "COMPLETED"};

	if (state >= ARRAY_SIZE(st_name))
		return "unknown";

	return st_name[state];
}

static inline void skw_list_init(struct skw_list *list)
{
	spin_lock_init(&list->lock);
	INIT_LIST_HEAD(&list->list);
	list->count = 0;
}

static inline void skw_list_add(struct skw_list *list, struct list_head *entry)
{
	spin_lock_bh(&list->lock);
	list_add_tail(entry, &list->list);
	list->count++;
	spin_unlock_bh(&list->lock);
}

static inline void skw_list_del(struct skw_list *list, struct list_head *entry)
{
	spin_lock_bh(&list->lock);
	list_del_init(entry);
	list->count--;
	spin_unlock_bh(&list->lock);
}

static inline void *skw_ctx_entry(const struct skw_peer *peer)
{
	return (char *)peer + ALIGN(sizeof(struct skw_peer), SKW_PEER_ALIGN);
}

static inline bool is_skw_ap_mode(struct skw_iface *iface)
{
	return iface->wdev.iftype == NL80211_IFTYPE_AP ||
	       iface->wdev.iftype == NL80211_IFTYPE_P2P_GO;
}

static inline bool is_skw_sta_mode(struct skw_iface *iface)
{
	return iface->wdev.iftype == NL80211_IFTYPE_STATION ||
	       iface->wdev.iftype == NL80211_IFTYPE_P2P_CLIENT;
}

static inline void skw_peer_ctx_lock(struct skw_peer_ctx *ctx)
{
	if (WARN_ON(!ctx))
		return;

	mutex_lock(&ctx->lock);
}

static inline void skw_peer_ctx_unlock(struct skw_peer_ctx *ctx)
{
	if (WARN_ON(!ctx))
		return;

	mutex_unlock(&ctx->lock);
}
#if 0
static inline void skw_sta_lock(struct skw_sta_core *core)
{
	mutex_lock(&core->lock);
}

static inline void skw_sta_unlock(struct skw_sta_core *core)
{
	mutex_unlock(&core->lock);
}
#endif
static inline void skw_sta_assert_lock(struct skw_sta_core *core)
{
	lockdep_assert_held(&core->lock);
}

static inline void skw_wdev_lock(struct wireless_dev *wdev)
	__acquires(wdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 15)
	mutex_lock(&wdev->mtx);
	__acquire(wdev->mtx);
#endif
}

static inline void skw_wdev_unlock(struct wireless_dev *wdev)
	__releases(wdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 15)
	__release(wdev->mtx);
    mutex_unlock(&wdev->mtx);
#endif
}

static inline void skw_wdev_assert_lock(struct skw_iface *iface)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 15)
	lockdep_assert_held(&iface->wdev.mtx);
#endif
}

struct skw_iface *skw_add_iface(struct wiphy *wiphy, const char *name,
				enum nl80211_iftype iftype, u8 *mac,
				u8 id, bool need_ndev);
int skw_del_iface(struct wiphy *wiphy, struct skw_iface *iface);

void skw_iface_set_wmm_capa(struct skw_iface *iface, const u8 *ies,
					size_t ies_len);

int skw_iface_setup(struct wiphy *wiphy, struct net_device *dev,
		    struct skw_iface *iface, const u8 *addr,
		    enum nl80211_iftype iftype, int id);

int skw_iface_teardown(struct wiphy *wiphy, struct skw_iface *iface);

int skw_cmd_open_dev(struct wiphy *wiphy, int inst, const u8 *mac_addr,
		enum nl80211_iftype type, u16 flags);
void skw_purge_survey_data(struct skw_iface *iface);
void skw_ap_check_sta_throughput(void *data);
void skw_set_sta_timer(struct skw_sta_core *core, unsigned long timeout);

struct skw_peer *skw_peer_alloc(void);
void skw_peer_init(struct skw_peer *peer, const u8 *addr, int idx);
struct skw_peer_ctx *skw_peer_ctx(struct skw_iface *iface, const u8 *mac);
void skw_peer_ctx_transmit(struct skw_peer_ctx *ctx, bool enable);
void __skw_peer_ctx_transmit(struct skw_peer_ctx *ctx, bool enable);
int skw_peer_ctx_bind(struct skw_iface *iface, struct skw_peer_ctx *ctx,
			struct skw_peer *peer);
int __skw_peer_ctx_bind(struct skw_iface *iface, struct skw_peer_ctx *ctx,
			struct skw_peer *peer);
void skw_peer_free(struct skw_peer *peer);
void skw_purge_key_conf(struct skw_key_conf *conf);
#endif
