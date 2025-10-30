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

#ifndef __SKW_RX_H__
#define __SKW_RX_H__

#include <net/ieee80211_radiotap.h>

#include "skw_platform_data.h"
#include "skw_iface.h"
#include "skw_core.h"
#include "skw_tx.h"

#define SKW_MAX_AMPDU_BUF_SIZE            0x100 /* 256 */

#define SKW_AMSDU_FLAG_TAINT              BIT(0)
#define SKW_AMSDU_FLAG_VALID              BIT(1)

#define SKW_SDIO_RX_DESC_HDR_OFFSET       0
#define SKW_SDIO_RX_DESC_MSDU_OFFSET      52
#define SKW_USB_RX_DESC_HDR_OFFSET        52
#define SKW_USB_RX_DESC_MSDU_OFFSET       0
#define SKW_PCIE_RX_DESC_HDR_OFFSET       44
#define SKW_PCIE_RX_DESC_MSDU_OFFSET      8

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW (ETH_P_ECONET + 1)
#endif

enum SKW_RELEASE_REASON {
	SKW_RELEASE_INVALID,
	SKW_RELEASE_EXPIRED,
	SKW_RELEASE_OOB,
	SKW_RELEASE_BAR,
	SKW_RELEASE_FREE,
};

struct skw_skb_rxcb {
	unsigned long rx_time;
	u16 rx_desc_offset;
	u8 amsdu_bitmap;
	u8 amsdu_mask;
	u16 amsdu_flags;
	u8 skw_created;
	u8 lmac_id;
	u8 skip_replay_detect;
};

struct skw_drop_sn_info {
	u16 sn;
	u8 amsdu_idx;
	u8 amsdu_first: 1;
	u8 amsdu_last: 1;
	u8 is_amsdu: 1;
	u8 qos: 1;
	u8 tid: 4;
	u32 peer_idx: 5;
	u32 inst: 2;
	u32 resved: 25;
} __packed;

struct skw_rx_desc {
	/* word 13 */
	u16 eosp:1;
	u16 more_data:1;
	u16 pm:1;
	u16 retry_frame:1;
	u16 is_eof:1; //mpdu_eof_flag
	u16 ba_session:1;
	u16 resv1:1;
	u16 resv:1;
	u16 cipher:4;
	u16 snap_type:1;
	u16 vlan:1;
	u16 eapol:1;
	u16 rcv_in_ps_mode:1;
	u16 msdu_len;

	/* word 14 */
	u8 csum_valid:1;
	u8 is_ampdu:1;
	u8 snap_match:1;
	u8 is_amsdu:1;
	u8 is_qos_data:1;
	u8 amsdu_first_idx:1;
	u8 amsdu_last_idx:1;
	u8 mpdu_sniff:1;
	u16 csum;
	u8 msdu_filter;

	/* word 15 */
	u16 sn:12; /* seq number */
	u16 frag_num:4;
	u16 inst_id:2; //mpdu_ra_index
	u16 inst_id_valid:1;
	u16 more_frag:1;
	u16 peer_idx:5;
	u16 peer_idx_valid:1;
	u16 is_mc_addr:1; //bc_mc_flag
	u16 first_msdu_in_buff:1;
	u16 tid:4;

	/* word 16 & word17*/
	u8 pn[6];   //u16 msdu_len; //32:47
	u8 msdu_offset;
	u8 amsdu_idx:6;
	u16 need_forward:1;//da_ra_diff
	u16 mac_drop_frag:1;
} __packed;

struct skw_phy_rx_desc {
	/* word 8 */
	u32 lgacy_len:12;
	u32 psdu_len:20;

	/* word 9 */
	u32 flock_rssi0:11;
	u32 flock_rssi1:11;
	u32 lp_snr0:7;
	u32 nss:2;
	u32 sigb_dcm:1;

	/* word 10 */
	u32 full_rssi0:11;
	u32 full_rssi1:11;
	u32 lp_snr1:7;
	u32 sbw:3;

	/* word 11 */
	u8 agc_gain0;
	u8 agc_gain1;
	u8 ppdu_mode:4;
	u8 dcm:1;
	u8 gi_type:2;
	u8 fec_coding:1;
	u8 data_rate:6;
	u8 ess_n_est_ss:2;

	/* word 12 */
	u16 sta_id:11;
	u16 ru_size:3;
	u16 he_sigb_comp:1;
	u16 doppler:1;
	u16 sr4:4;
	u16 sr3:4;
	u16 sr2:4;
	u16 sr1:4;

	/* word 13 */
	u16 grp_id:6;
	u16 partial_aid:9;
	u16 befmed:1;
	u16 top_dura:14;
	u16 ltf_type:2;

	/* word 14 */
	u8 ch1_agc_gain0;
	u8 ch1_agc_gain1;
	u16 serv_field;

	/* word 15 */
	u32 sfo_ppm_init:24;
	u32 plcp_delay:8;

	/* word 16 */
	u32 slock_rssi0:11;
	u32 slock_rssi2:11;
	u32 nsts:2;
	u32 bss_color:6;
	u32 tgnf_flag0:1;
	u32 tgnf_flag1:1;

	/* word 17 */
	u8 ofdma:1;
	u8 mimo:1;
	u8 sifb_mcs:3;
	u8 pe_dur:3;
	u8 user_num:7;
	u8 stbc:1;
	u16 mu3_nsts:3;
	u16 mu2_nsts:3;
	u16 mu1_nsts:3;
	u16 mu0_nsts:3;
	u16 ltf_num:2;
	u16 mimo_ofdma:1;
	u16 resv8:1;

};

struct skw_sniffer_desc {
	u32 resv1;
	/* word 4 */
	u8 mac_hdr_proc:7;
	u8 sniff_flag:1;
	u8 mpdu_proc_status;
	u8 buf_num_mpdu;
	u8 mac_hdr_len:6;
	u8 dir_data_sniff:1;
	u8 resv2:1;

	/* word 5 */
	u16 mpdu_len:14;
	u16 resv3:2;
	u16 psdu_cnt;

	/* word 6 */
	u8 sniff_rsv_num;
	u8 resv4:1;
	u8 is_ampdu:1;
	u8 is_amsdu:1;
	u8 mpdu2host:1;
	u8 mpdu_defrag:1;
	u8 mpdu_uc:1;
	u8 mpdu_bc:1;
	u8 resv5:1;
	u8 peer_lut_idx:5;
	u8 peer_lut_idx_vaild:1;
	u8 resv6:2;
	u8 cipher:4;
	u8 inst_id:2; //mpdu_ra_index
	u8 inst_id_vaild:1;
	u8 resv7:1;

	/* word 7 */
	u16 sniff_status:12;
	u16 pad_len:4;
	u16 sn:12;
	u16 frag_num:4;

	/* word 8 - 17 */
	struct skw_phy_rx_desc phy_desc;
} __packed;

struct skw_radiotap_desc {
	struct ieee80211_radiotap_header radiotap_hdr;
	u8 radiotap_flag;
} __packed;

static inline void skw_snap_unmatch_handler(struct sk_buff *skb)
{
	skb_reset_mac_header(skb);
	eth_hdr(skb)->h_proto = htons(skb->len & 0xffff);
}

static inline void skw_event_add_credit(struct skw_core *skw, void *data)
{
	u16 *credit = data;

	skw_add_credit(skw, 0, *credit);
	skw_add_credit(skw, 1, *(credit + 1));
}

static inline void skw_data_add_credit(struct skw_core *skw, void *data)
{
}

static inline bool is_skw_monitor_data(struct skw_core *skw, void *data)
{
	struct skw_sniffer_desc *desc = NULL;

	if (skw->hw.bus == SKW_BUS_USB)
		desc = (struct skw_sniffer_desc *)((u8 *)(data + 12));	//offset word0 ~ word2
	else if (skw->hw.bus == SKW_BUS_SDIO)
		desc = (struct skw_sniffer_desc *)((u8 *)(data));
	else if (skw->hw.bus == SKW_BUS_PCIE) //TODO
		return false;

	if (!desc)
		return -EINVAL;

	skw_detail("sniffer flag:%d\n", desc->sniff_flag);

	if (desc->sniff_flag) {
		skw_detail("recv sniffer data, desc len:%ld\n", sizeof(struct skw_sniffer_desc));
		return true;
	}

	return false;
}

static inline struct skw_skb_rxcb *SKW_SKB_RXCB(struct sk_buff *skb)
{
	return (struct skw_skb_rxcb *)skb->cb;
}

int skw_add_tid_rx(struct skw_peer *peer, u16 tid, u16 ssn, u16 buf_size);
int skw_update_tid_rx(struct skw_peer *peer, u16 tid, u16 ssn, u16 win_size);
int skw_del_tid_rx(struct skw_peer *peer, u16 tid);

int skw_rx_process(struct skw_core *skw,
	struct sk_buff_head *rx_dat_q, struct skw_list *rx_todo_list);
void skw_rx_todo(struct skw_list *todo_list);

int skw_rx_init(struct skw_core *skw);
int skw_rx_deinit(struct skw_core *skw);
int skw_rx_cb(int port, struct scatterlist *sglist, int nents, void *priv);
int skw_register_rx_callback(struct skw_core *skw, void *cmd_cb, void *cmd_ctx,
			void *dat_cb, void *dat_ctx);

#endif
