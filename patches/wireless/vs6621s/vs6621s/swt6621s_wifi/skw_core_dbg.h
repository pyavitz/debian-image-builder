#ifndef __SKW_CORE_DBG_H__
#define __SKW_CORE_DBG_H__

#include "skw_core.h"

#define RPI_MAX_LEVEL                            6
#define NOISE_MAX_LEVEL                          6
#define SKW_MAX_INST_NUM                         4
#define SKW_MAX_W_LEVEL                          5

struct tx_mib_acc_info {
	u32 acc_tx_psdu_cnt;
	u16 acc_tx_wi_ack_to_cnt;
	u16 acc_tx_wi_ack_fail_cnt;
	u32 acc_rts_wo_cts_cnt;
	u16 acc_tx_post_busy_cnt;
	u8 tx_en_perct;
} __packed;

struct tx_hmac_per_hw_rpt_info {
	u32 hmac_tx_stat[7];
} __packed;

struct tx_mib_tb_acc_info {
	/*from tx reg*/
	u32 rx_basic_trig_cnt;
	u32 rx_basic_trig_succ_cnt;

	u32 tb_gen_invoke_cnt;
	u32 tb_gen_abort_cnt;

	u32 tb_cs_fail_acc_cnt;
	u32 all_tb_pre_tx_cnt;

	u32 tb_tx_acc_cnt;
	u32 tb_tx_acc_scucc_cnt;
	u32 tb_tx_suc_ratio;

	/*from rx req*/
	u32 rx_trig_reg_cnt;
	u32 rx_trig_start_tx_cnt;
	u32 rx_trig_suc_ratio;
} __packed;

struct skw_get_hw_tx_info_s {
	u32 hif_tx_all_cnt;
	u32 hif_rpt_all_credit_cnt;
	u32 cur_free_buf_cnt;
	u32 pendding_tx_pkt_cnt;
	u32 lst_cmd_seq;
	u32 lst_evt_seq;
	u32 lmac_tx_bc_cnt;
	u32 lmac_tx_uc_cnt[5];
	struct tx_hmac_per_hw_rpt_info hmac_tx_stat;
	struct tx_mib_acc_info tx_mib_acc_info;
	struct tx_mib_tb_acc_info tx_mib_tb_acc_info;
} __packed;

struct skw_cs_monitor_rpt {
	u8 p20_lst_busy_perctage;
	u8 p40_lst_busy_perctage;
	u8 p80_lst_busy_perctage;
	u8 p20_lst_nav_perctage;
	u8 p40_lst_nav_perctage;
	u8 p80_lst_nav_perctage;
} __packed;

struct skw_rpi_monitor_rpt {
	int8_t no_uc_direct_rpi_level;
	int8_t uc_direct_rpi_level;
	int8_t rx_ba_rssi;
	u8 rx_idle_percent_in_rpi;
	u8 rx_percent_in_rpi;
	/* measure dur*/
	u32 rpi_class_dur[RPI_MAX_LEVEL];
} __packed;

struct skw_noise_monitor_rpt {
	u32 noise_class_dur[RPI_MAX_LEVEL];
} __packed;

struct skw_get_inst_info_s {
	u8 inst_mode;
	u8 hw_id;
	u8 enable;
	u8 mac_id;
	u32 asoc_peer_map;
	u32 peer_ps_state;
	struct skw_cs_monitor_rpt cs_info;
	struct skw_rpi_monitor_rpt rpi_info;
	struct skw_noise_monitor_rpt noise_info;
} __packed;

struct skw_inst_nss_info {
       u8 valid_id;
       u8 inst_rx_nss:4;
       u8 inst_tx_nss:4;
} __packed;

struct skw_mac_nss_info {
       u8 mac_nss[SKW_MAX_LMAC_SUPPORT];
       u8 is_dbdc_mode;
       u8 max_inst_num;
       struct skw_inst_nss_info inst_nss[SKW_MAX_INST_NUM];
} __packed;

struct skw_hw_w_debug {
       u16 w_total_cnt;
       u16 w_cnt[SKW_MAX_W_LEVEL];
       u16 w_ctrl_thresh[SKW_MAX_W_LEVEL - 1];
} __packed;

struct skw_debug_info_w {
	struct skw_hw_w_debug hw_w[SKW_MAX_LMAC_SUPPORT];
} __packed;

struct skw_debug_info_s {
	u16 tlv;
	u8 val[1];
} __packed;

/* TLV: SKW_MIB_GET_HW_TX_INFO_E */
/* No val data for debug info cmd */
/** Rsp of this TLV **/
struct skw_hw_tx_info_rsp {
	u16 status;
	u16 tlv;
	u16 len;
	u8 val[0];
} __packed;

/* TLV: SKW_MIB_GET_INST_INFO_E */
/* No val data for debug info cmd */
/** Rsp of this TLV **/
struct skw_inst_info_rsp {
	u16 status;
	u16 tlv;
	u16 len;
	u8 val[0];
} __packed;

/* TLV: SKW_MIB_GET_PEER_INFO_E */
/* No val data for debug info cmd */
/** Rsp of this TLV **/
struct skw_peer_info_rsp {
	u16 status;
	u16 tlv;
	u16 len;
	u8 val[0];
} __packed;

struct skw_hw_rx_info_rsp {
	u16 status;
	u16 tlv;
	u16 len;
	u8 val[0];
} __packed;

struct skw_get_hw_rx_info {
	u16 phy_rx_all_cnt;
	u16 phy_rx_11b_cnt;
	u16 phy_rx_11g_cnt;
	u16 phy_rx_11n_cnt;
	u16 phy_rx_11ac_cnt;
	u16 phy_rx_11ax_cnt;
	u16 mac_rx_mpdu_cnt;
	u16 mac_rx_mpdu_fcs_pass_cnt;
	u16 mac_rx_mpdu_fcs_pass_dir_cnt;
} __packed;

/* TLV: SKW_MIB_GET_INST_TSF_E */
/* No val data for debug info cmd */
/** Rsp of this TLV **/
struct skw_inst_tsf {
	u32 tsf_l;
	u32 tsf_h;
} __packed;

struct skw_get_inst_tsf_rsp {
	u16 status;
	u16 tlv;
	u16 len;
	u8 val[0];
} __packed;

struct skw_tx_hmac_per_peer_rpt_info {
	u32 hmac_tx_stat[5];
	u32 txc_isr_read_dscr_fifo_cnt;
} __packed;

struct skw_rate_struct {
	u8 flags;
	u8 mcs_idx;
	u16 legacy_rate;
	u8 nss;
	u8 bw;
	u8 gi;
	u8 ru;
	u8 dcm;
} __packed;

struct skw_rx_msdu_info_rpt {
	u8 ppdu_mode;
	u8 data_rate;
	u8 gi_type;
	u8 sbw;
	u8 dcm;
	u8 nss;
} __packed;

struct skw_tx_dbg_stats_per_peer {
	u32 rts_fail_cnt;
	u32 psdu_ack_timeout_cnt;
	u32 tx_mpdu_cnt;
	u32 tx_wait_time;
} __packed;

struct skw_get_peer_info {
	u8 is_valid;
	u8 inst_id;
	u8 hw_id;
	int8_t rx_rsp_rssi;
	u16 avg_add_delay_in_ms;
	u32 tx_max_delay_time;
	struct skw_tx_hmac_per_peer_rpt_info hmac_tx_stat;
	struct skw_rate_struct tx_rate_info;
	struct skw_rx_msdu_info_rpt rx_msdu_info_rpt;
	struct skw_tx_dbg_stats_per_peer tx_stats_info;
	u16 tx_pending_pkt_cnt[4];
} __packed;

void skw_dbg_dump(struct skw_core *skw);
void skw_core_dbg_init(struct wiphy *wiphy);
void skw_core_dbg_deinit(struct wiphy *wiphy);

#endif
