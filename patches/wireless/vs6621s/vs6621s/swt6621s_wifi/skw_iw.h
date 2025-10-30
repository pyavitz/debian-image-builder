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

#ifndef __SKW_IW_H__
#define __SKW_IW_H__

#define SKW_MAX_TLV_BUFF_LEN          1024

#define SKW_KEEPACTIVE_RULE_SEND_CNT_DEF   1
#define SKW_KEEPACTIVE_RULE_SEND_CNT_MAX   10
#define SKW_KEEPACTIVE_RULE_MAX            18
#define SKW_KEEPACTIVE_LENGTH_MAX            135
#define SKW_KEEPACTIVE_CMD_BUF_MAX            1300
#define SKW_KEEPALIVE_ALWAYS_FLAG          BIT(0)
#define SKW_KEEPALIVE_NEEDCHKSUM_FLAG      BIT(1)
#define SKW_KEEPALIVE_FWCHKSUM_FLAG        BIT(2)

struct skw_keep_active_rule_data {
	u16 is_chksumed;
	u8 payload[0];
} __packed;
struct skw_keep_active_rule {
	u32 keep_interval;
	u8 send_cnt:7;
	u8 always:1;
	u8 payload_len;
	struct skw_keep_active_rule_data data[0];
} __packed;

struct skw_keep_active_setup {
	u32 en_bitmap;
	u32 flags[SKW_KEEPACTIVE_RULE_MAX];
	struct skw_keep_active_rule *rule[SKW_KEEPACTIVE_RULE_MAX];
} __packed;

struct skw_keep_active_param {
	u8 rule_num;
	struct skw_keep_active_rule rules[0];
} __packed;

typedef int (*skw_at_handler)(struct skw_core *skw, void *param,
			char *args, char *resp, int resp_len);

struct skw_at_cmd {
	char *name;
	skw_at_handler handler;
	char *help_info;
};

typedef int (*skw_iwpriv_handler)(struct skw_iface *iface, void *param,
			char *args, char *resp, int resp_len);

struct skw_iwpriv_cmd {
	char *name;
	skw_iwpriv_handler handler;
	char *help_info;
};

struct skw_max_ppdu_dur {
	u8 idx;
	u32 max_ppdu_dur;
} __packed;

struct skw_wmm_ac_param_s {
	/* b0-b3 aifsn
	 * b4    acm
	 * b5-b6 aci
	 * b7    revd
	 */
	u8 aci_aifn;

	/* b0-b3 ECWmin
	 * b4-b7 ECWmax
	 */
	u8 ec_wmin_wmax;
	u16 txop_limit;
} __packed;

struct skw_edca_param_s {
	u8 enable;
	struct skw_wmm_ac_param_s ac_best_effort;
	struct skw_wmm_ac_param_s ac_background;
	struct skw_wmm_ac_param_s ac_video;
	struct skw_wmm_ac_param_s ac_voice;
} __packed;

struct skw_cca_thre_nowifi {
	u8 val;
} __packed;

struct skw_cca_thre_11b {
	u8 val;
} __packed;

struct skw_cca_thre_ofdm {
	u8 val;
} __packed;

enum skw_lega_ofdm_rate_map {
	LEGA_11B_SHORT_2M  = 0x10,
	LEGA_11B_SHORT_55M = 0x11,
	LEGA_11B_SHORT_11M = 0x12,
	LEGA_11B_LONG_1M   = 0x20,
	LEGA_11B_LONG_2M   = 0x21,
	LEGA_11B_LONG_55M  = 0x22,
	LEGA_11B_LONG_11M  = 0x23,

	OFDM_6M            = 0x30,
	OFDM_9M            = 0x31,
	OFDM_12M           = 0x32,
	OFDM_18M           = 0x33,
	OFDM_24M           = 0x34,
	OFDM_36M           = 0x35,
	OFDM_48M           = 0x36,
	OFDM_54M           = 0x37,

	HT_MCS_0           = 0x40,
	HT_MCS_1           = 0x41,
	HT_MCS_2           = 0x42,
	HT_MCS_3           = 0x43,
	HT_MCS_4           = 0x44,
	HT_MCS_5           = 0x45,
	HT_MCS_6           = 0x46,
	HT_MCS_7           = 0x47,
	HT_MCS_8           = 0x48,
	HT_MCS_9           = 0x49,
	HT_MCS_10           = 0x4a,
	HT_MCS_11           = 0x4b,
	HT_MCS_12           = 0x4c,
	HT_MCS_13           = 0x4d,
	HT_MCS_14           = 0x4e,
	HT_MCS_15           = 0x4f,
	HT_MCS_16           = 0x50,
	HT_MCS_17           = 0x51,
	HT_MCS_18           = 0x52,
	HT_MCS_19           = 0x53,
	HT_MCS_20           = 0x54,
	HT_MCS_21           = 0x55,
	HT_MCS_22           = 0x56,
	HT_MCS_23           = 0x57,
	HT_MCS_24           = 0x58,
	HT_MCS_25           = 0x59,
	HT_MCS_26           = 0x5a,
	HT_MCS_27           = 0x5b,
	HT_MCS_28           = 0x5c,
	HT_MCS_29           = 0x5d,
	HT_MCS_30           = 0x5e,
	HT_MCS_31           = 0x5f,

	VHT_MCS_0           = 0x80,
	VHT_MCS_1           = 0x81,
	VHT_MCS_2           = 0x82,
	VHT_MCS_3           = 0x83,
	VHT_MCS_4           = 0x84,
	VHT_MCS_5           = 0x85,
	VHT_MCS_6           = 0x86,
	VHT_MCS_7           = 0x87,
	VHT_MCS_8           = 0x88,
	VHT_MCS_9           = 0x89,

	HE_MCS_0            = 0xc0,
	HE_MCS_1            = 0xc1,
	HE_MCS_2            = 0xc2,
	HE_MCS_3            = 0xc3,
	HE_MCS_4            = 0xc4,
	HE_MCS_5            = 0xc5,
	HE_MCS_6            = 0xc6,
	HE_MCS_7            = 0xc7,
	HE_MCS_8            = 0xc8,
	HE_MCS_9            = 0xc9,
	HE_MCS_10            = 0xca,
	HE_MCS_11            = 0xcb,

	ER_NDCM_1SS_242TONE_MCS0          = 0xcc,
	ER_NDCM_1SS_242TONE_MCS1          = 0xcd,
	ER_NDCM_1SS_242TONE_MCS2          = 0xce,
	ER_NDCM_1SS_106TONE_MCS0          = 0xcf,

	ER_DCM_1SS_242TONE_MCS0          = 0xdc,
	ER_DCM_1SS_242TONE_MCS1          = 0xdd,
	ER_DCM_1SS_106TONE_MCS0          = 0xdf,

	NER_DCM_1SS_MCS0          = 0xec,
	NER_DCM_1SS_MCS1          = 0xed,
	NER_DCM_1SS_MCS3          = 0xee,
	NER_DCM_1SS_MCS4          = 0xef,

	NER_DCM_2SS_MCS0          = 0xfc,
	NER_DCM_2SS_MCS1          = 0xfd,
	NER_DCM_2SS_MCS3          = 0xfe,
	NER_DCM_2SS_MCS4          = 0xff,
};

struct skw_force_rts_rate {
	u8 enable;
	u8 rts_rate_24G;
	u8 rts_rate_5G;
} __packed;

struct skw_force_rx_rsp_rate {
	u8 enable;
	u8 rx_rsp_rate_11b_long;
	u8 rx_rsp_rate_11b_short;
	u8 rx_rsp_rate_ofdm;
} __packed;

struct skw_set_scan_time {
	u8 active_dwell_time;
	u8 bypass_active_acan_auto_time;
 //0 or 1
} __packed;

struct skw_set_tcpd_wakeup_host {
	u8 enable;
} __packed;

struct skw_set_rate_control_min_rate {
	u8 rstrict_min_rate;
} __packed;

struct skw_set_rate_control_rate_change {
	u8 up_rate_class_num;
	u8 down_rate_class_num;
	u8 hw_rty_limit;
	u8 per_rate_hw_rty_limit;
	u8 per_rate_probe_hw_rty_limit;
} __packed;

struct skw_set_rate_control_special_rate {
	u8 special_frm_rate;
} __packed;

struct skw_tlv_set_tx_lifetime {
	u16 lifetime;
} __packed;

struct skw_tlv_set_retry_cnt {
	u8 rtycnt;

} __packed;
struct skw_tlv_set_tx_rts_thrd {
	u16 rts_thrd;
} __packed;

struct skw_tlv_set_rx_special_80211_frame {
	u8 en;
	u8 type;
	u8 sub_type;
} __packed;

struct skw_tlv_set_rx_update_nav {
	u8 intra_rssi;
	u8 basic_rssi;
	u8 nav_max_time;
} __packed;

struct skw_tlv_set_apgo_timap {
	u8 dtimforce0;
	u8 dtimforce1;
	u8 timforce0;
	u8 timforce1;
} __packed;

struct skw_tlv_set_dbdc_disable {
	u8 disable;
} __packed;

struct skw_tlv_set_assign_addr_val {
	u32 addr;
	u32 val;
} __packed;

struct skw_tlv_get_assign_addr {
	u32 addr;
} __packed;

struct skw_tlv_get_assign_addr_rsp {
	u32 val;
} __packed;

struct skw_tlv_set_ageout_thrd {
	u8 ageout_kick_thrd;
	u8 ageout_keep_alive_thrd;
} __packed;

//TLV 42
struct skw_tlv_set_report_cqm_rssi_low_itvl {
	u16 report_cqm_low_intvl_min_dur;
	u16 report_cqm_low_intvl_max_dur;
} __packed;

//TLV 54
struct skw_tlv_set_ap_new_channel {
	u8 chan;
	u8 center_chan;
	u8 center_two_chan;
	u8 bw;
	u8 band;
} __packed;

//TLV 55
struct skw_tlv_set_tx_retry_limit_en {
	u8 short_retry_check_en;
	u8 long_retry_check_en;
	u8 ampdu_retry_check_en;
} __packed;

//TLV 56
struct skw_tlv_set_partial_twt_sched {
	u8 en;
	u32 start_time_l;
	u32 start_time_h;
	u32 interval;
	u16 duration;
	u8 duration_unit;
	u8 sub_type;
} __packed;

//TLV 57
struct skw_tlv_set_thm_thrd {
	u16 thm_high_thrd_tx_suspend;
	u16 thm_low_thrd_tx_resume;
} __packed;

//TLV 59
struct skw_tlv_set_normal_scan_with_acs {
	u8 en;
} __packed;

const void *skw_iw_handlers(void);

#ifdef CONFIG_WEXT_PRIV
u8 skw_wow_filter_remap_idx(u8 map_count);
void skw_tx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb);
void skw_rx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb);
u32 skw_fill_tcpalive_offload_info(struct wiphy *wiphy, u8 *tcp_off);
#else
static inline u8 skw_wow_filter_remap_idx(u8 map_count)
{
	return 0;
}

static inline void skw_tx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
}

static inline void skw_rx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
}

static inline u32 skw_fill_tcpalive_offload_info(struct wiphy *wiphy, u8 *tcp_off)
{
	return 0;
}
#endif

#endif
