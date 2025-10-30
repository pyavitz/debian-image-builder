#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>

#include "skw_core.h"
#include "skw_core_dbg.h"
#include "skw_cfg80211.h"

static int skw_repeater_show(struct seq_file *seq, void *data)
{
	struct skw_core *skw = seq->private;

	if (test_bit(SKW_FLAG_REPEATER, &skw->flags))
		seq_puts(seq, "enable\n");
	else
		seq_puts(seq, "disable\n");

	return 0;
}

static int skw_repeater_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_repeater_show, inode->i_private);
}

static ssize_t skw_repeater_write(struct file *fp, const char __user *buf,
				size_t len, loff_t *offset)
{
	int i;
	char cmd[32] = {0};
	struct skw_core *skw = fp->f_inode->i_private;

	for (i = 0; i < len; i++) {
		char c;

		if (get_user(c, buf))
			return -EFAULT;

		if (c == '\n' || c == '\0')
			break;

		cmd[i] = tolower(c);
		buf++;
	}

	if (strcmp(cmd, "enable") == 0)
		set_bit(SKW_FLAG_REPEATER, &skw->flags);
	else if (strcmp(cmd, "disable") == 0)
		clear_bit(SKW_FLAG_REPEATER, &skw->flags);
	else
		skw_chip_warn(skw->idx, "rx_reorder support setting values of \"enable\" or \"disable\"\n");

	return len;
}

static const struct file_operations skw_repeater_fops = {
	.owner = THIS_MODULE,
	.open = skw_repeater_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_repeater_write,
};

static void skw_timestap_print(struct skw_core *skw, struct seq_file *seq)
{
	u64 ts;
	struct rtc_time tm;
	unsigned long rem_nsec;

	ts = skw->fw.host_timestamp;
	rem_nsec = do_div(ts, 1000000000);
	skw_compat_rtc_time_to_tm(skw->fw.host_seconds, &tm);

	seq_printf(seq,
		   "Timestamp: %u - %lu.%06lu (%d-%02d-%02d %02d:%02d:%02d UTC)\n",
		   skw->fw.timestamp, (unsigned long)ts,
		   rem_nsec / 1000, tm.tm_year + 1900,
		   tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		   tm.tm_min, tm.tm_sec);
}

static void skw_drv_info_print(struct skw_core *skw, struct seq_file *seq)
{
       int i;

#define FLAG_TEST(n) test_bit(SKW_FLAG_##n, &skw->flags)
       seq_printf(seq,
                  "SKW Flags:\t 0x%lx %s\n"
                  "TX Packets:\t %ld\n"
                  "RX Packets:\t %ld\n"
                  "txqlen_pending:\t %d\n"
                  "nr lmac:\t %d\n"
                  "skb_recycle_qlist:%d\n",
                  skw->flags, FLAG_TEST(FW_ASSERT) ? "(Assert)" : "",
                  skw->tx_packets,
                  skw->rx_packets,
                  atomic_read(&skw->txqlen_pending),
                  skw->hw.nr_lmac,
                  READ_ONCE(skw->skb_recycle_qlist.qlen));

       for (i = 0; i < skw->hw.nr_lmac; i++)
               seq_printf(seq, "    credit[%d]:\t %d (%s)\n",
                          i, skw_get_hw_credit(skw, i),
                          skw_lmac_is_actived(skw, i) ? "active" : "inactive");
#undef FLAG_TEST
}

static void skw_fw_info_print(struct skw_core *skw, struct seq_file *seq)
{
	if (!skw->hw_pdata)
		return;

	/* Firmware & BSP Info */
	seq_printf(seq, "Calib File:\t %s\n"
			"FW Build:\t %s\n"
			"FW Version:\t %s-%s (BSP-MAC)\n"
			"BUS Type:\t 0x%x (%s)\n"
			"Align Size:\t %d\n"
			"TX Limit:\t %d\n",
			skw->fw.calib_file,
			skw->fw.build_time,
			skw->fw.plat_ver,
			skw->fw.wifi_ver,
			skw->hw_pdata->bus_type,
			skw_bus_name(skw->hw.bus),
			skw->hw_pdata->align_value,
			skw->hw.pkt_limit);
}

static void skw_dbg_print(struct skw_core *skw, struct seq_file *seq)
{
	int i;
	u64 nsecs;
	unsigned long rem_nsec;

	for (i = 0; i < skw->dbg.nr_cmd; i++) {
		struct skw_dbg_cmd *cmd = &skw->dbg.cmd[i];

		seq_printf(seq, "cmd[%d].id: %d, seq: %d, flags: 0x%lx, loop: %d\n",
			i, cmd->id, cmd->seq, cmd->flags, cmd->loop);

		nsecs = cmd->trigger;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.trigger: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->build;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.build: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->xmit;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.xmit: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->done;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.done: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->ack;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.ack: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->assert;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    cmd.%d.%d.assert: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		seq_puts(seq, "\n");
	}

	for (i = 0; i < skw->dbg.nr_dat; i++) {
		struct skw_dbg_dat *dat = &skw->dbg.dat[i];

		seq_printf(seq, "dat[%d], tx_qlen: %d\n", i, dat->qlen);

		nsecs = dat->trigger;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    dat.%d.%d.trigger: %5lu.%06lu\n",
			i, dat->qlen, (unsigned long)nsecs, rem_nsec / 1000);

		nsecs = dat->done;
		rem_nsec = do_div(nsecs, 1000000000);
		seq_printf(seq, "    dat.%d.%d.done: %5lu.%06lu\n",
			i, dat->qlen, (unsigned long)nsecs, rem_nsec / 1000);

		seq_puts(seq, "\n");
	}
}

static int skw_core_show(struct seq_file *seq, void *data)
{
	struct skw_core *skw = seq->private;

	seq_puts(seq, "\n");
	skw_timestap_print(skw, seq);

	seq_puts(seq, "\n");
	skw_drv_info_print(skw, seq);

	seq_puts(seq, "\n");
	skw_fw_info_print(skw, seq);

	seq_puts(seq, "\n");
	skw_dbg_print(skw, seq);

	seq_puts(seq, "\n");

	return 0;
}

static int skw_core_open(struct inode *inode, struct file *file)
{
	// return single_open(file, skw_core_show, inode->i_private);
	return single_open(file, skw_core_show, skw_pde_data(inode));
}

static void skw_debug_print_hw_tx_info(struct seq_file *seq, struct skw_get_hw_tx_info_s *info) {
	int i = 0;
	if (!info) {
		seq_printf(seq, "HW Tx Info structure is NULL\n");
		return;
	}
	seq_printf(seq, "Hardware Tx info:\n\n");
	seq_printf(seq, "\tHIF TX All Count: %u\n", info->hif_tx_all_cnt);
	seq_printf(seq, "\tHIF Report All Credit Count: %u\n", info->hif_rpt_all_credit_cnt);
	seq_printf(seq, "\tCurrent Free Buffer Count: %u\n", info->cur_free_buf_cnt);
	seq_printf(seq, "\tPending TX Packet Count: %u\n", info->pendding_tx_pkt_cnt);
	seq_printf(seq, "\tLast Command Sequence: %u\n", info->lst_cmd_seq);
	seq_printf(seq, "\tLast Event Sequence: %u\n", info->lst_evt_seq);
	seq_printf(seq, "\tLMAC TX Broadcast Count: %u\n", info->lmac_tx_bc_cnt);
	seq_printf(seq, "\tLMAC TX Unicast Count: %u\n", info->lmac_tx_uc_cnt[0]);

	// Print array lmac_tx_uc_cnt
	seq_printf(seq, "\tLMAC TX Unicast Count Array: ");
	for (i = 0; i < 5; i++) {
		seq_printf(seq, "%u ", info->lmac_tx_uc_cnt[i]);
	}
	seq_puts(seq, "\n");

	// Print hmac_tx_stat
	seq_printf(seq, "\tHMAC TX Stat: ");
	for (i = 0; i < 7; i++) {
		seq_printf(seq, "%u ", info->hmac_tx_stat.hmac_tx_stat[i]);
	}
	seq_puts(seq, "\n");

	// Print tx_mib_acc_info
	seq_printf(seq, "\tAccumulated TX PSDU Count: %u\n", info->tx_mib_acc_info.acc_tx_psdu_cnt);
	seq_printf(seq, "\tAccumulated TX With ACK Timeout Count: %u\n", info->tx_mib_acc_info.acc_tx_wi_ack_to_cnt);
	seq_printf(seq, "\tAccumulated TX With ACK Fail Count: %u\n", info->tx_mib_acc_info.acc_tx_wi_ack_fail_cnt);
	seq_printf(seq, "\tAccumulated RTS Without CTS Count: %u\n", info->tx_mib_acc_info.acc_rts_wo_cts_cnt);
	seq_printf(seq, "\tAccumulated TX Post Busy Count: %u\n", info->tx_mib_acc_info.acc_tx_post_busy_cnt);
	seq_printf(seq, "\tTX Enable Percentage: %u%%\n", info->tx_mib_acc_info.tx_en_perct);

	// Print tx_mib_tb_acc_info
	seq_printf(seq, "\tTB Basic Trigger Count: %u\n", info->tx_mib_tb_acc_info.rx_basic_trig_cnt);
	seq_printf(seq, "\tTB Basic Trigger Success Count: %u\n", info->tx_mib_tb_acc_info.rx_basic_trig_succ_cnt);
	seq_printf(seq, "\tTB Generate Invoke Count: %u\n", info->tx_mib_tb_acc_info.tb_gen_invoke_cnt);
	seq_printf(seq, "\tTB Generate Abort Count: %u\n", info->tx_mib_tb_acc_info.tb_gen_abort_cnt);
	seq_printf(seq, "\tTB CS Fail Accumulated Count: %u\n", info->tx_mib_tb_acc_info.tb_cs_fail_acc_cnt);
	seq_printf(seq, "\tAll TB Pre TX Count: %u\n", info->tx_mib_tb_acc_info.all_tb_pre_tx_cnt);
	seq_printf(seq, "\tTB TX Accumulated Count: %u\n", info->tx_mib_tb_acc_info.tb_tx_acc_cnt);
	seq_printf(seq, "\tTB TX Success Accumulated Count: %u\n", info->tx_mib_tb_acc_info.tb_tx_acc_scucc_cnt);
	seq_printf(seq, "\tTB TX Success Ratio: %u\n", info->tx_mib_tb_acc_info.tb_tx_suc_ratio);
	seq_printf(seq, "\tRX Trigger Register Count: %u\n", info->tx_mib_tb_acc_info.rx_trig_reg_cnt);
	seq_printf(seq, "\tRX Trigger Start TX Count: %u\n", info->tx_mib_tb_acc_info.rx_trig_start_tx_cnt);
	seq_printf(seq, "\tRX Trigger Success Ratio: %u\n", info->tx_mib_tb_acc_info.rx_trig_suc_ratio);
}

static void skw_debug_print_inst_info(struct seq_file *seq, struct skw_get_inst_info_s *info)
{
	int i = 0;
	if (!info) {
		seq_printf(seq, "\tInst Info structure is NULL\n");
		return;
	}
	seq_printf(seq, "Inst info:\n\n");
	seq_printf(seq, "\tInst Mode: %u\n", info->inst_mode);
	seq_printf(seq, "\tHW ID: %u\n", info->hw_id);
	seq_printf(seq, "\tEnable: %u\n", info->enable);
	seq_printf(seq, "\tMAC ID: %u\n", info->mac_id);
	seq_printf(seq, "\tASOC Peer Map: %u\n", info->asoc_peer_map);
	seq_printf(seq, "\tPeer PS State: %u\n", info->peer_ps_state);

	seq_printf(seq, "\tCS Info:\n");
	seq_printf(seq, "\t\tP20 Last Busy Percentage: %u\n", info->cs_info.p20_lst_busy_perctage);
	seq_printf(seq, "\t\tP40 Last Busy Percentage: %u\n", info->cs_info.p40_lst_busy_perctage);
	seq_printf(seq, "\t\tP80 Last Busy Percentage: %u\n", info->cs_info.p80_lst_busy_perctage);
	seq_printf(seq, "\t\tP20 Last NAV Percentage: %u\n", info->cs_info.p20_lst_nav_perctage);
	seq_printf(seq, "\t\tP40 Last NAV Percentage: %u\n", info->cs_info.p40_lst_nav_perctage);
	seq_printf(seq, "\t\tP80 Last NAV Percentage: %u\n", info->cs_info.p80_lst_nav_perctage);

	seq_printf(seq, "\tRPI Info:\n");
	seq_printf(seq, "\t\tNo UC Direct RPI Level: %d\n", info->rpi_info.no_uc_direct_rpi_level);
	seq_printf(seq, "\t\tUC Direct RPI Level: %d\n", info->rpi_info.uc_direct_rpi_level);
	seq_printf(seq, "\t\tRX BA RSSI: %d\n", info->rpi_info.rx_ba_rssi);
	seq_printf(seq, "\t\tRX Idle Percent in RPI: %u\n", info->rpi_info.rx_idle_percent_in_rpi);
	seq_printf(seq, "\t\tRX Percent in RPI: %u\n", info->rpi_info.rx_percent_in_rpi);
	seq_printf(seq, "\t\tRPI Class Duration: ");
	for (i = 0; i < RPI_MAX_LEVEL; i++) {
		seq_printf(seq, "%u ", info->rpi_info.rpi_class_dur[i]);
	}
	seq_printf(seq, "\n");

	seq_printf(seq, "\tNoise Info:\n");
	seq_printf(seq, "\t\tNoise Class Duration: ");
	for (i = 0; i < NOISE_MAX_LEVEL; i++) {
		seq_printf(seq, "%u ", info->noise_info.noise_class_dur[i]);
	}
	seq_printf(seq, "\n");
}

static void skw_debug_print_peer_info(struct seq_file *seq, struct skw_get_peer_info *peer_info)
{
	int i = 0;
	if (!peer_info) {
		seq_printf(seq, "\tPeer Info structure is NULL\n");
	return;
	}

	seq_printf(seq, "Peer info:\n\n");
	seq_printf(seq, "\tIs Valid: %u\n", peer_info->is_valid);
	seq_printf(seq, "\tInstance ID: %u\n", peer_info->inst_id);
	seq_printf(seq, "\tHW ID: %u\n", peer_info->hw_id);
	seq_printf(seq, "\tRX RSSI: %d\n", peer_info->rx_rsp_rssi);
	seq_printf(seq, "\tAverage ADD Delay (ms): %u\n", peer_info->avg_add_delay_in_ms);
	seq_printf(seq, "\tTX Max Delay Time: %u\n", peer_info->tx_max_delay_time);

	// Print TX Hmac Per Peer Report
	seq_printf(seq, "\tHMAC TX Stat:\n");
	seq_printf(seq, "\t\t");
	for (i = 0; i < 5; i++) {
		seq_printf(seq, "%u ", peer_info->hmac_tx_stat.hmac_tx_stat[i]);
	}
	seq_printf(seq, "\n");
	seq_printf(seq, "\tTXC ISR Read DSCR FIFO Count: %u\n", peer_info->hmac_tx_stat.txc_isr_read_dscr_fifo_cnt);

	// Print TX Rate Info
	seq_printf(seq, "\tTX Rate Info:\n");
	seq_printf(seq, "\t\tFlags: %u\n", peer_info->tx_rate_info.flags);
	seq_printf(seq, "\t\tMCS Index: %u\n", peer_info->tx_rate_info.mcs_idx);
	seq_printf(seq, "\t\tLegacy Rate: %u\n", peer_info->tx_rate_info.legacy_rate);
	seq_printf(seq, "\t\tNSS: %u\n", peer_info->tx_rate_info.nss);
	seq_printf(seq, "\t\tBW: %u\n", peer_info->tx_rate_info.bw);
	seq_printf(seq, "\t\tGI: %u\n", peer_info->tx_rate_info.gi);
	seq_printf(seq, "\t\tRU: %u\n", peer_info->tx_rate_info.ru);
	seq_printf(seq, "\t\tDCM: %u\n", peer_info->tx_rate_info.dcm);

	// Print RX MSDU Info Report
	seq_printf(seq, "\tRX MSDU Info:\n");
	seq_printf(seq, "\t\tPPDU Mode: %u\n", peer_info->rx_msdu_info_rpt.ppdu_mode);
	seq_printf(seq, "\t\tData Rate: %u\n", peer_info->rx_msdu_info_rpt.data_rate);
	seq_printf(seq, "\t\tGI Type: %u\n", peer_info->rx_msdu_info_rpt.gi_type);
	seq_printf(seq, "\t\tSBW: %u\n", peer_info->rx_msdu_info_rpt.sbw);
	seq_printf(seq, "\t\tDCM: %u\n", peer_info->rx_msdu_info_rpt.dcm);
	seq_printf(seq, "\t\tNSS: %u\n", peer_info->rx_msdu_info_rpt.nss);

	// Print TX Debug Stats Per Peer
	seq_printf(seq, "\tTX Stats Info:\n");
	seq_printf(seq, "\t\tRTS Fail Count: %u\n", peer_info->tx_stats_info.rts_fail_cnt);
	seq_printf(seq, "\t\tPSDU Ack Timeout Count: %u\n", peer_info->tx_stats_info.psdu_ack_timeout_cnt);
	seq_printf(seq, "\t\tTX MPDU Count: %u\n", peer_info->tx_stats_info.tx_mpdu_cnt);
	seq_printf(seq, "\t\tTX Wait Time: %u\n", peer_info->tx_stats_info.tx_wait_time);

	// Print TX Pending Packet Count
	seq_printf(seq, "\tTX Pending Packet Count: ");
	for (i = 0; i < 4; i++) {
		seq_printf(seq, "%u ", peer_info->tx_pending_pkt_cnt[i]);
	}
	seq_printf(seq, "\n");
}

static void skw_debug_print_hw_rx_info(struct seq_file *seq, struct skw_get_hw_rx_info *rx_info)
{
	if (!rx_info) {
		seq_printf(seq, "\tInfo structure is NULL\n");
	return;
	}
	seq_printf(seq, "rx info:\n\n");
	seq_printf(seq, "\tPHY RX All Count: %u\n", rx_info->phy_rx_all_cnt);
	seq_printf(seq, "\tPHY RX 11B Count: %u\n", rx_info->phy_rx_11b_cnt);
	seq_printf(seq, "\tPHY RX 11G Count: %u\n", rx_info->phy_rx_11g_cnt);
	seq_printf(seq, "\tPHY RX 11N Count: %u\n", rx_info->phy_rx_11n_cnt);
	seq_printf(seq, "\tPHY RX 11AC Count: %u\n", rx_info->phy_rx_11ac_cnt);
	seq_printf(seq, "\tPHY RX 11AX Count: %u\n", rx_info->phy_rx_11ax_cnt);
	seq_printf(seq, "\tMAC RX MPDU Count: %u\n", rx_info->mac_rx_mpdu_cnt);
	seq_printf(seq, "\tMAC RX MPDU FCS Pass Count: %u\n", rx_info->mac_rx_mpdu_fcs_pass_cnt);
	seq_printf(seq, "\tMAC RX MPDU FCS Pass Direct Count: %u\n", rx_info->mac_rx_mpdu_fcs_pass_dir_cnt);
}

static void skw_debug_print_inst_tsf(struct seq_file *seq, struct skw_inst_tsf *inst_tsf)
{
	if (!inst_tsf) {
		seq_printf(seq, "\tInst tsf structure is NULL\n");
		return;
	}
	seq_printf(seq, "inst tsf:\n\n");
	seq_printf(seq, "\tInst tsf low: %u\n", inst_tsf->tsf_l);
	seq_printf(seq, "\tInst tsf high: %u\n", inst_tsf->tsf_h);
}

static void skw_debug_print_winfo(struct seq_file *seq, struct skw_debug_info_w  *w_info)
{
	int i, j;
	int percent;

	seq_puts(seq, "debug_w info:\n");

	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		seq_printf(seq, "    hw[%d]:\t total[%d]\t other_count:%d\n",
			i, w_info->hw_w[i].w_total_cnt,
			w_info->hw_w[i].w_cnt[SKW_MAX_W_LEVEL - 1]);
		seq_puts(seq, "\n");
		for (j = 0; j < SKW_MAX_W_LEVEL - 1; j++) {
			percent = w_info->hw_w[i].w_cnt[j] * 1000
					/ w_info->hw_w[i].w_total_cnt;
			seq_printf(seq, "\t  %d:thresh[%d]\t"
			"count:%d\t     percent:%d.%d%%\n", j,
				w_info->hw_w[i].w_ctrl_thresh[j],
				w_info->hw_w[i].w_cnt[j],
				percent/10, percent%10);
		}
		seq_puts(seq, "\n");
	}

	seq_puts(seq, "\n");

}

static void skw_debug_print_nss_info(struct seq_file *seq, struct skw_mac_nss_info  *nss_info)
{
	int i;

	seq_puts(seq, "nss info:\n");
	seq_printf(seq,
		"\t mac0 nss: %d,\tmac1 nss: %d.\t"
		" dbdc mode:%d\n\n"
		"\t instence num:%d\n",
		nss_info->mac_nss[0],
		nss_info->mac_nss[1],
		nss_info->is_dbdc_mode,
		nss_info->max_inst_num);

	for (i = 0; i < nss_info->max_inst_num; i++)
		if (nss_info->inst_nss[i].valid_id)
			seq_printf(seq, "\t  inst[%d]:\t valid:%d\t"
				"rx_nss:%d\t tx_nss:%d\n",
				i, nss_info->inst_nss[i].valid_id,
				nss_info->inst_nss[i].inst_rx_nss,
				nss_info->inst_nss[i].inst_tx_nss);

	seq_puts(seq, "\n");

}

static int skw_debug_get_hw_tx_info(struct skw_core *skw, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_GET_HW_TX_INFO_E;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_GET_HW_TX_INFO_E ||
		tlv->len != sizeof(struct skw_get_hw_tx_info_s))
		return -1;

	return 0;
}

static int skw_debug_get_inst_info(struct skw_core *skw, u8 inst_id, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_GET_INST_INFO_E;
	ret = skw_msg_xmit(priv_to_wiphy(skw), inst_id, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_GET_INST_INFO_E ||
		tlv->len != sizeof(struct skw_get_inst_info_s))
		return -1;

	return 0;
}

static int skw_debug_get_peer_info(struct skw_core *skw, u8 peer_idx, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_GET_PEER_INFO_E;
	debug_info_param.val[0] = peer_idx;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_GET_PEER_INFO_E ||
		tlv->len != sizeof(struct skw_get_peer_info))
		return -1;

	return 0;
}


static int skw_debug_get_hw_rx_info(struct skw_core *skw,  u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_GET_HW_RX_INFO_E;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_GET_HW_RX_INFO_E ||
		tlv->len != sizeof(struct skw_get_hw_rx_info))
		return -1;

	return 0;
}

static int skw_debug_get_inst_tsf(struct skw_core *skw, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_GET_INST_TSF_E;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_GET_INST_TSF_E ||
		tlv->len != sizeof(struct skw_inst_tsf))
		return -1;

	return 0;
}

static int skw_debug_get_winfo(struct skw_core *skw, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_W_INFO;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_W_INFO ||
		tlv->len != sizeof(struct skw_debug_info_w))
		return -1;

	return 0;
}

static int skw_debug_get_nss_info(struct skw_core *skw, u8 *buf, int len)
{
	int ret = -1;
	struct skw_tlv *tlv;

	struct skw_debug_info_s debug_info_param = {0};
	debug_info_param.tlv = SKW_MIB_MAC_NSS_INFO;
	ret = skw_msg_xmit(priv_to_wiphy(skw), 0, SKW_CMD_GET_DEBUG_INFO,
						&debug_info_param, sizeof(struct skw_debug_info_s),
						buf, len);

	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	tlv = (struct skw_tlv *)buf;
	if (tlv->type != SKW_MIB_MAC_NSS_INFO ||
		tlv->len != sizeof(struct skw_mac_nss_info))
		return -1;

	return 0;
}

static int skw_debug_info_show(struct seq_file *seq, void *data)
{
	struct skw_core *skw = seq->private;
	u8 *tx_info = NULL;
	u8 *inst_info = NULL;
	u8 *peer_info = NULL;
	u8 *rx_info = NULL;
	u8 *inst_tsf = NULL;
	u8 *nss_buf = NULL;
	u8 *w_info = NULL;
	int len;

	len = sizeof(struct skw_get_hw_tx_info_s) + sizeof(struct skw_tlv);
	tx_info = SKW_ZALLOC(len, GFP_KERNEL);
	if (!tx_info) {
		skw_chip_err(skw->idx, "tx_info failed\n");
	} else {
		if (skw_debug_get_hw_tx_info(skw, tx_info, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_hw_tx_info(seq, (void *)(tx_info + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(tx_info);
	}

	len = sizeof(struct skw_get_inst_info_s) + sizeof(struct skw_tlv);
	inst_info = SKW_ZALLOC(len, GFP_KERNEL);
	if (!inst_info) {
		skw_chip_err(skw->idx, "inst_info failed\n");
	} else {
		if (skw_debug_get_inst_info(skw, 0, inst_info, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_inst_info(seq, (void *)(inst_info + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(inst_info);
	}

	len = sizeof(struct skw_get_peer_info) + sizeof(struct skw_tlv);
	peer_info = SKW_ZALLOC(len, GFP_KERNEL);
	if (!peer_info) {
		skw_chip_err(skw->idx, "peer_info failed\n");
	} else {
		if (skw_debug_get_peer_info(skw, 0, peer_info, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_peer_info(seq, (void *)(peer_info + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(peer_info);
	}

	len = sizeof(struct skw_get_hw_rx_info) + sizeof(struct skw_tlv);
	rx_info = SKW_ZALLOC(len, GFP_KERNEL);
	if (!rx_info) {
		skw_chip_err(skw->idx, "rx_info failed\n");
	} else {
		if (skw_debug_get_hw_rx_info(skw, rx_info, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_hw_rx_info(seq, (void *)(rx_info + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(rx_info);
	}

	len = sizeof(struct skw_inst_tsf) + sizeof(struct skw_tlv);
	inst_tsf = SKW_ZALLOC(len, GFP_KERNEL);
	if (!inst_tsf) {
		skw_chip_err(skw->idx, "inst_tsf failed\n");
	} else {
		if (skw_debug_get_inst_tsf(skw, inst_tsf, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_inst_tsf(seq, (void *)(inst_tsf + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(inst_tsf);
	}

	len = sizeof(struct skw_debug_info_w) + sizeof(struct skw_tlv);
	w_info = SKW_ZALLOC(len, GFP_KERNEL);
	if (!w_info) {
		skw_chip_err(skw->idx, "w_info failed\n");
	} else {
		if (skw_debug_get_winfo(skw, w_info, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_winfo(seq, (void *)(w_info + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(w_info);
	}

	len = sizeof(struct skw_mac_nss_info) + sizeof(struct skw_tlv);
	nss_buf = SKW_ZALLOC(len, GFP_KERNEL);
	if (!nss_buf) {
		skw_chip_err(skw->idx, "nss_buf failed\n");
	} else {
		if (skw_debug_get_nss_info(skw, nss_buf, len) == 0) {
			seq_puts(seq, "\n");
			skw_debug_print_nss_info(seq, (void *)(nss_buf + sizeof(struct skw_tlv)));
			seq_puts(seq, "\n");
		}

		SKW_KFREE(nss_buf);
	}

	seq_puts(seq, "\n");

	return 0;
}

static int skw_debug_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_debug_info_show, skw_pde_data(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_core_fops = {
       .proc_open = skw_core_open,
       .proc_read = seq_read,
       .proc_lseek = seq_lseek,
       .proc_release = single_release,
};
static const struct proc_ops skw_debug_info_fops = {
       .proc_open = skw_debug_info_open,
       .proc_read = seq_read,
       .proc_lseek = seq_lseek,
       .proc_release = single_release,
};
#else
static const struct file_operations skw_core_fops = {
       .owner = THIS_MODULE,
       .open = skw_core_open,
       .read = seq_read,
       .llseek = seq_lseek,
       .release = single_release,
};
static const struct file_operations skw_debug_info_fops = {
       .owner = THIS_MODULE,
       .open = skw_debug_info_open,
       .read = seq_read,
       .llseek = seq_lseek,
       .release = single_release,
};
#endif

static int skw_assert_show(struct seq_file *seq, void *data)
{
	return 0;
}

static int skw_assert_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_assert_show, inode->i_private);
}

static ssize_t skw_assert_write(struct file *fp, const char __user *buf,
				size_t len, loff_t *offset)
{
	struct skw_core *skw = fp->f_inode->i_private;

	skw_hw_assert(skw, false);

	return len;
}

static const struct file_operations skw_assert_fops = {
	.owner = THIS_MODULE,
	.open = skw_assert_open,
	.read = seq_read,
	.write = skw_assert_write,
	.release = single_release,
};

void skw_dbg_dump(struct skw_core *skw)
{
	int i;
	u64 nsecs;
	unsigned long rem_nsec;

	for (i = 0; i < skw->dbg.nr_cmd; i++) {
		struct skw_dbg_cmd *cmd = &skw->dbg.cmd[i];

		skw_info("cmd[%d].id: %d, seq: %d, flags: 0x%lx, loop: %d\n",
			i, cmd->id, cmd->seq, cmd->flags, cmd->loop);

		nsecs = cmd->trigger;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.trigger: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->build;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.build: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->xmit;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.xmit: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->done;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.done: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->ack;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.ack: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);

		nsecs = cmd->assert;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("cmd.%d.%d.assert: %5lu.%06lu\n", cmd->id, cmd->seq,
			(unsigned long)nsecs, rem_nsec / 1000);
	}

	for (i = 0; i < skw->dbg.nr_dat; i++) {
		struct skw_dbg_dat *dat = &skw->dbg.dat[i];

		nsecs = dat->trigger;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("dat.%d.%d.trigger: %5lu.%06lu\n",
			i, dat->qlen, (unsigned long)nsecs, rem_nsec / 1000);

		nsecs = dat->done;
		rem_nsec = do_div(nsecs, 1000000000);
		skw_info("dat.%d.%d.done: %5lu.%06lu\n",
			i, dat->qlen, (unsigned long)nsecs, rem_nsec / 1000);
	}
}

void skw_core_dbg_init(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_procfs_file(skw->pentry, "core", 0444, &skw_core_fops, skw);

	skw_procfs_file(skw->pentry, "debug_info", 0444, &skw_debug_info_fops, skw);

	skw_debugfs_file(skw->dentry, "assert", 0444, &skw_assert_fops, skw);

	skw_debugfs_file(skw->dentry, "repeater", 0666, &skw_repeater_fops, skw);
}

void skw_core_dbg_deinit(struct wiphy *wiphy)
{
}
