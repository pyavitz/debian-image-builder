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

#include "skw_core.h"
#include "skw_iface.h"
#include "skw_rx.h"
#include "skw_mlme.h"
#include "skw_cfg80211.h"
#include "skw_timer.h"
#include "skw_tx.h"
#include "skw_dfs.h"
#include "trace.h"

static inline const char *skw_width_name(enum SKW_CHAN_BW_INFO state)
{
	static const char * const wd_name[] = {"20M", "40M", "80M",
					"80+80M", "160M"};

	if (state >= ARRAY_SIZE(wd_name))
		return "unknown";

	return wd_name[state];
}

static inline const char *skw_80211_mode_name(enum skw_rate_info_flags mode)
{
	static const char * const md_name[] = {"legacy", "ieee80211n", "ieee80211ac", "ieee80211ax"};

	if (mode >= ARRAY_SIZE(md_name))
		return "unknown";

	return md_name[mode];
}

static int skw_iface_show(struct seq_file *seq, void *data)
{
	u32 peer_idx_map, idx;
	struct skw_peer_ctx *ctx;
	struct skw_bss_cfg *bss = NULL;
	struct net_device *ndev = seq->private;
	struct skw_iface *iface = netdev_priv(ndev);
	int i;

	seq_puts(seq, "\n");
	seq_printf(seq, "Iface: \t%s (id: %d)\n"
			"    addr:  \t%pM\n"
			"    mode:  \t%s\n"
			"    cpu_id:  \t%d\n",
			netdev_name(iface->ndev),
			iface->id,
			iface->addr,
			skw_iftype_name(iface->wdev.iftype),
			iface->cpu_id);

	switch (iface->wdev.iftype) {
	case NL80211_IFTYPE_STATION:
		if (iface->flags & SKW_IFACE_FLAG_LEGACY_P2P_DEV)
			break;

		skw_fallthrough;

	case NL80211_IFTYPE_P2P_CLIENT:
		bss = &iface->sta.core.bss;
		seq_printf(seq, "    state: \t%s\n"
					"    connect width: %s\n",
				skw_state_name(iface->sta.core.sm.state),
				bss && bss->channel ? 0 != bss->channel->band ? skw_width_name(bss->width) :
				skw_width_name(min(bss->width, bss->ht_cap_chwidth)) : "");
		break;

	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		bss = &iface->sap.cfg;
		seq_printf(seq, "    max sta: \t%d\n",
				iface->sap.max_sta_allowed);
		break;

	default:
		break;
	}

	seq_printf(seq, "\nBSS Info: %s\n", bss ? "" : "null");
	if (bss) {
		seq_printf(seq, "    SSID:  \t%s\n"
				"    BSSID: \t%pM\n"
				"    channel:\t%d\n"
				"    width: \t%s\n",
				bss->ssid,
				bss->bssid,
				bss->channel ? bss->channel->hw_value : -1,
				skw_width_name(bss->width));

	}

	peer_idx_map = atomic_read(&iface->peer_map);

	seq_printf(seq, "\nPEER Info: %s\n", peer_idx_map ? "" : "null");
	while (peer_idx_map) {
		idx = ffs(peer_idx_map) - 1;
		SKW_CLEAR(peer_idx_map, BIT(idx));

		ctx = &iface->skw->hw.lmac[iface->lmac_id].peer_ctx[idx];

		mutex_lock(&ctx->lock);

		if (ctx->peer) {
			s16 rssi = ctx->peer->rx.rssi >> 3;

			if (ctx->peer->rx.rssi & BIT(10))
				rssi |= 0xff00;

			seq_printf(seq, "    %pM (%d) %s\n",
					ctx->peer->addr,
					ctx->peer->idx,
					skw_state_name(ctx->peer->sm.state));

			seq_printf(seq, "        TX: tidmap: 0x%x, %s: %d, nss:%d, %s, psr: %d, tx_failed: %d\n"
					"        RX: tidmap: 0x%x, %s: %d, nss:%d, %s\n"
					"        rssi: %d, data: %d\n"
					"        rx data band width :%s\n"
					"        filter stats :\n",
					ctx->peer->txba.bitmap,
					ctx->peer->tx.rate.flags ?
					 "mcs" : "legacy_rate",
					ctx->peer->tx.rate.flags ?
					 ctx->peer->tx.rate.mcs_idx :
					 ctx->peer->tx.rate.legacy_rate,
					 ctx->peer->tx.rate.nss,
					 skw_80211_mode_name(ctx->peer->tx.rate.flags),
					ctx->peer->tx.tx_psr,
					ctx->peer->tx.tx_failed,
					ctx->peer->rx_tid_map,
					ctx->peer->rx.rate.flags ?
					 "mcs" : "legacy_rate",
					ctx->peer->rx.rate.flags ?
					 ctx->peer->rx.rate.mcs_idx :
					 ctx->peer->rx.rate.legacy_rate,
					 ctx->peer->rx.rate.nss,
					 skw_80211_mode_name(ctx->peer->rx.rate.flags),
					ctx->peer->tx.rssi,
					rssi, skw_width_name(ctx->peer->rx.rate.bw));
			seq_puts(seq, "            fliter:");

			for (i = 0; i < 35; i++)
				seq_printf(seq, "%d ", ctx->peer->rx.filter_cnt[i]);

			seq_puts(seq, "\n            filter drop:");

			for (i = 0; i < 35; i++)
				seq_printf(seq, "%d ", ctx->peer->rx.filter_drop_offload_cnt[i]);

			seq_printf(seq, "\n        Channnel occupancy: tx: %d, rx_idle: %d\n",
				 ctx->peer->tx.percent, ctx->peer->rx.percent);

			seq_puts(seq, "\n");
		}

		mutex_unlock(&ctx->lock);
	}

	seq_puts(seq, "\nTXQ Info:\n");
	seq_printf(seq, "    [VO]: stoped: %d, qlen: %d tx_cache:%d\n"
			"    [VI]: stoped: %d, qlen: %d tx_cache:%d\n"
			"    [BE]: stoped: %d, qlen: %d tx_cache:%d\n"
			"    [BK]: stoped: %d, qlen: %d tx_cache:%d\n",
			SKW_TXQ_STOPED(ndev, SKW_WMM_AC_VO),
			skb_queue_len(&iface->txq[SKW_WMM_AC_VO]),
			skb_queue_len(&iface->tx_cache[SKW_WMM_AC_VO]),
			SKW_TXQ_STOPED(ndev, SKW_WMM_AC_VI),
			skb_queue_len(&iface->txq[SKW_WMM_AC_VI]),
			skb_queue_len(&iface->tx_cache[SKW_WMM_AC_VI]),
			SKW_TXQ_STOPED(ndev, SKW_WMM_AC_BE),
			skb_queue_len(&iface->txq[SKW_WMM_AC_BE]),
			skb_queue_len(&iface->tx_cache[SKW_WMM_AC_BE]),
			SKW_TXQ_STOPED(ndev, SKW_WMM_AC_BK),
			skb_queue_len(&iface->txq[SKW_WMM_AC_BK]),
			skb_queue_len(&iface->tx_cache[SKW_WMM_AC_BK]));

	if (iface->skw->hw.bus != SKW_BUS_PCIE)
		seq_printf(seq, "\nskw->rx_dat_q:%d\n", READ_ONCE(iface->skw->rx_dat_q.qlen));

	return 0;
}

static int skw_iface_open(struct inode *inode, struct file *file)
{
	// return single_open(file, skw_iface_show, inode->i_private);
	return single_open(file, skw_iface_show, skw_pde_data(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_iface_fops = {
	.proc_open = skw_iface_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations skw_iface_fops = {
	.owner = THIS_MODULE,
	.open = skw_iface_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*
 * skw_acl_allowed: check if sta is in acl list
 * return: true - allowd to access
 *         false - denied to access
 */
bool skw_acl_allowed(struct skw_iface *iface, u8 *addr)
{
	int i;
	bool match = false;

	if (!iface->sap.acl)
		return true;

	for (i = 0; i < iface->sap.acl->n_acl_entries; i++) {
		u8 *mac = iface->sap.acl->mac_addrs[i].addr;

		if (ether_addr_equal(addr, mac)) {
			match = true;
			break;
		}
	}

	/* white list */
	if (iface->sap.acl->acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED)
		return match;

	return !match;
}

int skw_cmd_open_dev(struct wiphy *wiphy, int inst, const u8 *mac_addr,
		enum nl80211_iftype type, u16 flags)
{
	int mode, ret;
	struct skw_open_dev_param open_param;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, inst: %d, mac: %pM, flags: 0x%x\n",
		skw_iftype_name(type), inst, mac_addr, flags);

	BUG_ON(!is_valid_ether_addr(mac_addr));

	switch (type) {
	case NL80211_IFTYPE_ADHOC:
		mode = SKW_IBSS_MODE;
		break;
	case NL80211_IFTYPE_STATION:
		mode = SKW_STA_MODE;
		break;
	case NL80211_IFTYPE_AP:
		mode = SKW_AP_MODE;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = SKW_GC_MODE;
		break;
	case NL80211_IFTYPE_P2P_GO:
		mode = SKW_GO_MODE;
		break;
	case NL80211_IFTYPE_P2P_DEVICE:
		mode = SKW_P2P_DEV_MODE;
		break;
	case NL80211_IFTYPE_MONITOR:
		mode = SKW_MONITOR_MODE;
		break;
	default:
		skw_chip_err(skw->idx, "iftype: %d not support\n", type);
		return -EINVAL;
	}

	skw_ether_copy(open_param.mac_addr, mac_addr);
	open_param.mode = mode;
	open_param.flags = flags;

#ifdef CONFIG_SWT6621S_OFFCHAN_TX
	open_param.flags |= SKW_OPEN_FLAG_OFFCHAN_TX;
#endif

	ret = skw_msg_xmit(wiphy, inst, SKW_CMD_OPEN_DEV, &open_param,
			   sizeof(open_param), NULL, 0);

	return ret;
}

static int skw_cmd_close_dev(struct wiphy *wiphy, int dev_id)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev id: %d\n", dev_id);

	return skw_msg_xmit(wiphy, dev_id, SKW_CMD_CLOSE_DEV, NULL, 0, NULL, 0);
}

void skw_purge_survey_data(struct skw_iface *iface)
{
	struct skw_survey_info *sinfo = NULL;
	LIST_HEAD(flush_list);

	list_replace_init(&iface->survey_list, &flush_list);

	while (!list_empty(&flush_list)) {
		sinfo = list_first_entry(&flush_list,
				 struct skw_survey_info, list);

		list_del(&sinfo->list);
		SKW_KFREE(sinfo);
	}
}

void skw_iface_event_work(struct work_struct *work)
{
	struct sk_buff *skb;
	struct skw_msg *msg_hdr;
	struct skw_iface *iface;

	iface = container_of(work, struct skw_iface, event_work.work);

	while ((skb = skb_dequeue(&iface->event_work.qlist))) {
		msg_hdr = (struct skw_msg *)skb->data;
		skb_pull(skb, sizeof(*msg_hdr));

		skw_event_handler(iface->skw, iface, msg_hdr,
				  skb->data, skb->len, SKW_SKB_RXCB(skb));

		kfree_skb(skb);
	}
}

static int skw_add_vif(struct wiphy *wiphy, struct skw_iface *iface)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "iface: 0x%x, bitmap: 0x%x\n", iface->id, skw->vif.bitmap);

	if (iface->id == SKW_INVALID_ID)
		return 0;

	BUG_ON(skw->vif.iface[iface->id]);

	spin_lock_bh(&skw->vif.lock);

	skw->vif.iface[iface->id] = iface;

	spin_unlock_bh(&skw->vif.lock);

	return 0;
}

static void skw_del_vif(struct wiphy *wiphy, struct skw_iface *iface)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!iface)
		return;

	skw_chip_dbg(skw->idx, "iface id: %d\n", iface->id);

	BUG_ON(iface->id >= SKW_NR_IFACE);

	spin_lock_bh(&skw->vif.lock);

	skw->vif.iface[iface->id] = NULL;

	spin_unlock_bh(&skw->vif.lock);
}

static int skw_alloc_inst(struct wiphy *wiphy, u8 id)
{
	int inst = SKW_INVALID_ID;
	struct skw_core *skw = wiphy_priv(wiphy);

	spin_lock_bh(&skw->vif.lock);

	if (id == SKW_INVALID_ID) {
		for (id = 0; id < SKW_NR_IFACE; id++) {
			if (!(skw->vif.bitmap & BIT(id))) {
				inst = id;
				break;
			}
		}
	} else if ((id != (id & 0xf)) || (skw->vif.bitmap & BIT(id))) {
		inst = SKW_INVALID_ID;
	} else {
		inst = id;
	}

	if (inst != SKW_INVALID_ID)
		SKW_SET(skw->vif.bitmap, BIT(id));

	spin_unlock_bh(&skw->vif.lock);

	return inst;
}

static void skw_release_inst(struct wiphy *wiphy, int id)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	if (id != (id & 0xf))
		return;

	spin_lock_bh(&skw->vif.lock);

	SKW_CLEAR(skw->vif.bitmap, BIT(id));

	spin_unlock_bh(&skw->vif.lock);
}

static void skw_sta_work(struct work_struct *wk)
{
	bool run_again = false;
	bool connect_failed = false;
	u64 cookie;
	struct skw_iface *iface = container_of(wk, struct skw_iface, sta.work);
	struct net_device *ndev = iface->ndev;
	struct skw_sta_core *core = &iface->sta.core;
	struct wiphy *wiphy = priv_to_wiphy(iface->skw);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_wdev_lock(&iface->wdev);

	skw_chip_dbg(iface->skw->idx, "inst: %d, state: %s, retry: %d jiff: %d step_start: %d ctx_start: %d auth_start: %d redo:%d\n",
		core->sm.inst, skw_state_name(core->sm.state),
		core->pending.retry, jiffies_to_msecs(jiffies),
		jiffies_to_msecs(core->pending.step_start),
		jiffies_to_msecs(core->pending.ctx_start),
		jiffies_to_msecs(core->auth_start),
		core->pending.redo);

	if (time_after(jiffies, core->auth_start + SKW_AUTH_TIMEOUT + SKW_ASSOC_TIMEOUT))
		connect_failed = true;

	switch (core->sm.state) {
	case SKW_STATE_AUTHED:
		if (time_after(jiffies, core->pending.step_start + SKW_STEP_TIMEOUT)) {
			iface->sta.report_deauth = false;

			skw_sta_leave(wiphy, ndev, core->bss.bssid, 3, false);

			if (iface->sta.sme_external)
				skw_compat_auth_timeout(ndev, core->bss.bssid);
			else
				skw_disconnected(ndev, 3, NULL, 0, true, GFP_KERNEL);
		} else {
			run_again = true;
		}

		break;

	case SKW_STATE_AUTHING:
		if (time_after(jiffies, core->pending.step_start + SKW_STEP_TIMEOUT)) {
			if (++core->pending.retry >= SKW_MAX_AUTH_RETRY_NUM &&
				iface->wdev.iftype == NL80211_IFTYPE_STATION &&
				core->pending.auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) {
				if (time_after(jiffies, core->pending.ctx_start + core->pending.ctx_to) ||
					core->pending.redo >= SKW_MAX_REAUTH_REDO_NUM) {
					connect_failed = true;
				} else {
					if (core->sm.rty_state == SKW_RETRY_NONE)
						core->sm.rty_state = SKW_RETRY_AUTH;

					if (skw_mgmt_frame_with_reason(iface, core->bss.bssid,
						&cookie, core->bss.bssid, core->bss.channel,
						IEEE80211_STYPE_DEAUTH, WLAN_REASON_DEAUTH_LEAVING, false))
						connect_failed = true;
					else {
						skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
						core->pending.redo++;

						if (skw_msg_xmit_timeout(wiphy,
							SKW_NDEV_ID(ndev),
							SKW_CMD_AUTH,
							core->pending.auth_cmd,
							core->pending.auth_cmd_len,
							NULL, 0, "SKW_CMD_AUTH",
							msecs_to_jiffies(300), 0))
							connect_failed = true;
						else {
							core->pending.retry = 0;
							core->pending.step_start = jiffies;
							skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHING);
						}
					}
				}
			} else if (core->pending.retry >= SKW_MAX_AUTH_RETRY_NUM)
				connect_failed = true;
			else {
				skw_set_state(skw, &core->sm, SKW_STATE_AUTHING);

				if (skw_msg_xmit_timeout(wiphy,
					SKW_NDEV_ID(ndev),
					SKW_CMD_AUTH,
					core->pending.auth_cmd,
					core->pending.auth_cmd_len,
					NULL, 0, "SKW_CMD_AUTH",
					msecs_to_jiffies(300), 0))
					connect_failed = true;
			}
		}

		if (connect_failed) {
			iface->sta.report_deauth = false;
			skw_sta_leave(wiphy, ndev, core->bss.bssid, 3, false);

			if (iface->sta.sme_external)
				skw_compat_auth_timeout(ndev, core->bss.bssid);
			else
				skw_disconnected(ndev, 3, NULL, 0, true,
					GFP_KERNEL);
		} else {
			run_again = true;
		}

		break;

	case SKW_STATE_ASSOCING:
		if (time_after(jiffies, core->pending.step_start + SKW_STEP_TIMEOUT)) {
			if ((++core->pending.retry >= SKW_MAX_ASSOC_RETRY_NUM) &&
				iface->wdev.iftype == NL80211_IFTYPE_STATION &&
				core->pending.auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) {
				if (time_after(jiffies, core->pending.ctx_start + core->pending.ctx_to) ||
					core->pending.redo >= SKW_MAX_REAUTH_REDO_NUM) {
					connect_failed = true;
				} else {
					core->sm.rty_state = SKW_RETRY_ASSOC;

					if (skw_mgmt_frame_with_reason(iface, core->bss.bssid,
						&cookie, core->bss.bssid, core->bss.channel,
						IEEE80211_STYPE_DEAUTH, WLAN_REASON_DEAUTH_LEAVING, false))
						connect_failed = true;
					else {
						skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
						core->pending.redo++;

						if (skw_msg_xmit_timeout(wiphy,
							SKW_NDEV_ID(ndev),
							SKW_CMD_AUTH,
							core->pending.auth_cmd,
							core->pending.auth_cmd_len,
							NULL, 0, "SKW_CMD_AUTH",
							msecs_to_jiffies(300), 0))
							connect_failed = true;
						else {
							core->pending.retry = 0;
							core->pending.step_start = jiffies;
							skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHING);
						}
					}
				}

			} else if (core->pending.retry >= SKW_MAX_ASSOC_RETRY_NUM)
				connect_failed = true;
			 else {
				skw_set_state(skw, &core->sm, SKW_STATE_ASSOCING);

				if (skw_msg_xmit_timeout(wiphy,
							 SKW_NDEV_ID(ndev),
							 SKW_CMD_ASSOC,
							 core->pending.assoc_cmd,
							 core->pending.assoc_cmd_len,
							 NULL, 0, "SKW_CMD_ASSOC",
							 msecs_to_jiffies(300), 0))
					connect_failed = true;
			}
		}

		if (connect_failed) {
			iface->sta.report_deauth = false;
			skw_sta_leave(wiphy, ndev, core->bss.bssid, 3, false);

			if (iface->sta.sme_external)
				skw_compat_assoc_failure(ndev, core->cbss, true);
			else
				skw_disconnected(ndev, 3, NULL, 0, true, GFP_KERNEL);
		} else {
			run_again = true;
		}

		break;

	default:
		break;
	}

	skw_chip_dbg(iface->skw->idx, "state: %s, connect failed: %d, run again: %d\n",
		skw_state_name(core->sm.state), connect_failed, run_again);

	if (run_again)
		skw_set_sta_timer(core, SKW_STEP_TIMEOUT);

	skw_wdev_unlock(&iface->wdev);
}

static void skw_sta_timer(struct timer_list *t)
{
	struct skw_iface *iface = skw_from_timer(iface, t, sta.core.timer);

	queue_work(iface->skw->event_wq, &iface->sta.work);
}

void skw_set_sta_timer(struct skw_sta_core *core, unsigned long timeout)
{
	if (!timer_pending(&core->timer))
		mod_timer(&core->timer, jiffies + timeout);
}

static int skw_mode_init(struct wiphy *wiphy, struct skw_iface *iface,
			enum nl80211_iftype type, int id)
{
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_sta_core *core = &iface->sta.core;

	switch (type) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		memset(&iface->sta, 0x0, sizeof(iface->sta));

		iface->sta.sme_external = true;
		core->bss.ctx_idx = SKW_INVALID_ID;

		mutex_init(&core->lock);
		core->pending.auth_cmd = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
		if (!core->pending.auth_cmd)
			return -ENOMEM;

		core->pending.assoc_cmd = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
		if (!core->pending.assoc_cmd) {
			SKW_KFREE(core->pending.auth_cmd);
			return -ENOMEM;
		}

		core->assoc_req_ie = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
		if (!core->assoc_req_ie) {
			SKW_KFREE(core->pending.assoc_cmd);
			SKW_KFREE(core->pending.auth_cmd);
			return -ENOMEM;
		}

		core->sm.inst = id;
		core->sm.iface_iftype = type;
		core->sm.state = SKW_STATE_NONE;
		core->sm.addr = core->bss.bssid;

		INIT_WORK(&iface->sta.work, skw_sta_work);
		skw_compat_setup_timer(&core->timer, skw_sta_timer);

		iface->sta.conn = NULL;
		spin_lock_init(&iface->sta.roam_data.lock);

		if (!(test_bit(SKW_FLAG_STA_SME_EXTERNAL, &skw->flags))) {
			iface->sta.sme_external = false;

			iface->sta.conn = SKW_ZALLOC(sizeof(*iface->sta.conn),
						  GFP_KERNEL);
			if (!iface->sta.conn) {
				iface->sta.conn = NULL;
				SKW_KFREE(core->pending.assoc_cmd);
				SKW_KFREE(core->pending.auth_cmd);
				SKW_KFREE(core->assoc_req_ie);

				return -ENOMEM;
			}

			mutex_init(&iface->sta.conn->lock);
			iface->sta.conn->channel = NULL;

			iface->sta.conn->assoc_ie = SKW_ZALLOC(SKW_2K_SIZE,
							GFP_KERNEL);
			if (!iface->sta.conn->assoc_ie) {
				SKW_KFREE(core->pending.assoc_cmd);
				SKW_KFREE(core->pending.auth_cmd);
				SKW_KFREE(core->assoc_req_ie);
				SKW_KFREE(iface->sta.conn);

				return -ENOMEM;
			}
		}

		break;

	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		memset(&iface->sap, 0x0, sizeof(iface->sap));

		skw_list_init(&iface->sap.mlme_client_list);
		iface->sap.max_sta_allowed = skw->fw.max_num_sta;

		skw_dfs_init(wiphy, iface->ndev);

		if (test_bit(SKW_FLAG_SAP_SME_EXTERNAL, &skw->flags))
			iface->sap.sme_external = true;

		iface->sap.probe_resp = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
		if (!iface->sap.probe_resp)
			return -ENOMEM;

		memset(&iface->buf_keys, 0, sizeof(iface->buf_keys));
		iface->buf_keys_idx = 0;
		SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_BUF_KEY);
		SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_AP_STARTED);

		break;

	default:
		break;
	}

	return 0;
}

static void skw_mode_deinit(struct wiphy *wiphy, struct skw_iface *iface,
			enum nl80211_iftype iftype)
{
	struct skw_core *skw = iface->skw;
	struct skw_peer_ctx *ctx;

	switch (iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		skw_dfs_deinit(wiphy, iface->ndev);

		SKW_KFREE(iface->sap.acl);
		SKW_KFREE(iface->sap.cfg.ht_cap);
		SKW_KFREE(iface->sap.cfg.vht_cap);
		SKW_KFREE(iface->sap.probe_resp);

		break;

	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		if (iface->flags & SKW_IFACE_FLAG_LEGACY_P2P_DEV)
			break;

		if (iface->sta.conn) {
			SKW_KFREE(iface->sta.conn->assoc_ie);
			SKW_KFREE(iface->sta.conn);
		}

		del_timer_sync(&iface->sta.core.timer);
		cancel_work_sync(&iface->sta.work);

		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
		SKW_KFREE(iface->sta.core.pending.auth_cmd);
		SKW_KFREE(iface->sta.core.pending.assoc_cmd);
		SKW_KFREE(iface->sta.core.assoc_req_ie);
		ctx = skw_get_ctx(skw, iface->lmac_id, iface->sta.core.bss.ctx_idx);
		skw_peer_ctx_bind(iface, ctx, NULL);
		memset(&iface->sta.core.bss, 0, sizeof(struct skw_bss_cfg));
		iface->sta.core.bss.ctx_idx = SKW_INVALID_ID;

		break;

	default:
		break;
	}
}

int skw_iface_setup(struct wiphy *wiphy, struct net_device *dev,
		    struct skw_iface *iface, const u8 *addr,
		    enum nl80211_iftype iftype, int id)
{
	int i, ret;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, addr: %pM\n", skw_iftype_name(iftype), addr);

	BUG_ON(!addr || !is_valid_ether_addr(addr));

	iface->ndev = dev;
	iface->wdev.wiphy = wiphy;
	iface->skw = wiphy_priv(wiphy);
	iface->default_multicast = -1;

	mutex_init(&iface->lock);
	atomic_set(&iface->peer_map, 0);
	atomic_set(&iface->actived_ctx, 0);

	INIT_LIST_HEAD(&iface->survey_list);

	mutex_init(&iface->key_conf.lock);
	for (i = 0; i < SKW_NUM_MAX_KEY; i++) {
		iface->key_conf.installed_bitmap = 0;
		RCU_INIT_POINTER(iface->key_conf.key[i], NULL);
	}

	skw_event_work_init(&iface->event_work, skw_iface_event_work);

	for (i = 0; i < SKW_MAX_DEFRAG_ENTRY; i++) {
		iface->frag[i].id = i;
		iface->frag[i].tid = SKW_INVALID_ID;
		skb_queue_head_init(&iface->frag[i].skb_list);
	}

	for (i = 0; i < SKW_WMM_AC_MAX + 1; i++) {
		skb_queue_head_init(&iface->txq[i]);
		skb_queue_head_init(&iface->tx_cache[i]);
	}

	ret = skw_mode_init(wiphy, iface, iftype, id);
	if (ret) {
		skw_chip_err(skw->idx, "init failed, iface: %d, iftype: %d, ret: %d\n",
			id, iftype, ret);

		return ret;
	}

	ret = skw_cmd_open_dev(wiphy, id, addr, iftype, 0);
	if (ret) {
		skw_chip_err(skw->idx,"open failed, iface: %d, iftype: %d, ret:%d\n",
			id, iftype, ret);
		goto iface_deinit;
	}

	switch (iftype) {
	case NL80211_IFTYPE_STATION:
		skw_config_set_mib(wiphy, id, &skw->config->mib.sta);
		break;

	case NL80211_IFTYPE_P2P_CLIENT:
		skw_config_set_mib(wiphy, id, &skw->config->mib.gc);
		break;

	case NL80211_IFTYPE_AP:
		skw_config_set_mib(wiphy, id, &skw->config->mib.sap);
		break;

	case NL80211_IFTYPE_P2P_GO:
		skw_config_set_mib(wiphy, id, &skw->config->mib.go);
		break;

	default:
		break;
	}

	spin_lock_bh(&skw->vif.lock);
	SKW_SET(iface->flags, SKW_IFACE_FLAG_OPENED);
	skw->vif.opened_dev++;
	spin_unlock_bh(&skw->vif.lock);

	iface->id = id;
	iface->wdev.iftype = iftype;
	skw_ether_copy(iface->addr, addr);
	iface->cpu_id = -1;

	return 0;

iface_deinit:
	skw_mode_deinit(wiphy, iface, iftype);

	return ret;
}

void skw_purge_key_conf(struct skw_key_conf *conf)
{
	int idx;
	struct skw_key *key;

	if (!conf)
		return;

	mutex_lock(&conf->lock);

	for (idx = 0; idx < SKW_NUM_MAX_KEY; idx++) {
		key = rcu_dereference_protected(conf->key[idx],
				lockdep_is_held(&conf->lock));

		RCU_INIT_POINTER(conf->key[idx], NULL);
		if (key)
			kfree_rcu(key, rcu);
	}

	conf->flags = 0;
	conf->installed_bitmap = 0;
	conf->skw_cipher = 0;

	mutex_unlock(&conf->lock);
}

int skw_iface_teardown(struct wiphy *wiphy, struct skw_iface *iface)
{
	int i, ret;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx,"iface id: %d\n", iface->id);

	skw_scan_done(skw, iface, true);
	skw_purge_survey_data(iface);

	for (i = SKW_WMM_AC_VO; i < SKW_WMM_AC_MAX + 1; i++) {
		skb_queue_purge(&iface->txq[i]);
		skb_queue_purge(&iface->tx_cache[i]);
	}

	for (i = 0; i < SKW_MAX_DEFRAG_ENTRY; i++) {
		skb_queue_purge(&iface->frag[i].skb_list);
		iface->frag[i].tid = SKW_INVALID_ID;
	}

	skw_event_work_deinit(&iface->event_work);

	skw_mode_deinit(wiphy, iface, iface->wdev.iftype);

	skw_purge_key_conf(&iface->key_conf);

	ret = skw_cmd_close_dev(wiphy, iface->id);
	if (ret < 0)
		return ret;

	spin_lock_bh(&skw->vif.lock);

	SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_OPENED);
	skw->vif.opened_dev--;
	if (!skw->vif.opened_dev) {
		for (i = 0; i < skw->hw.nr_lmac; i++) {
			atomic_set(&skw->hw.lmac[i].fw_credit, 0);
			skw->rx_packets = 0;
			skw->tx_packets = 0;
		}
	}

	spin_unlock_bh(&skw->vif.lock);

	return 0;
}

struct skw_iface *skw_add_iface(struct wiphy *wiphy, const char *name,
				enum nl80211_iftype iftype, u8 *mac,
				u8 id, bool need_ndev)
{
	u8 *addr;
	int priv_size, ret;
	struct skw_iface *iface;
	struct net_device *ndev = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);
	int inst = skw_alloc_inst(wiphy, id);

	skw_chip_info(skw->idx, "%s, inst: %d, mac: %pM, bitmap: 0x%x\n",
		 skw_iftype_name(iftype), inst, mac, skw->vif.bitmap);

	if (inst == SKW_INVALID_ID) {
		skw_chip_err(skw->idx, "invalid inst: %d, bitmap: 0x%x\n",
			inst, skw->vif.bitmap);

		return ERR_PTR(-EINVAL);
	}

	priv_size = sizeof(struct skw_iface);
	if (need_ndev) {
		ndev = alloc_netdev_mqs(priv_size, name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
					NET_NAME_ENUM,
#endif
					ether_setup, SKW_WMM_AC_MAX, 1);

		if (!ndev) {
			skw_chip_err(skw->idx, "alloc netdev failed, iftype: %d\n", iftype);
			skw_release_inst(wiphy, inst);

			return ERR_PTR(-ENOMEM);
		}

		iface = netdev_priv(ndev);
	} else {
		iface = SKW_ZALLOC(priv_size, GFP_KERNEL);
		if (!iface) {
			skw_release_inst(wiphy, inst);
			return ERR_PTR(-ENOMEM);
		}
	}

	if (mac && is_valid_ether_addr(mac))
		addr = mac;
	else
		addr = wiphy->addresses[inst].addr;

	ret = skw_iface_setup(wiphy, ndev, iface, addr, iftype, inst);
	if (ret) {
		skw_chip_err(skw->idx, "iface setup failed, iftype: %d, ret: %d\n",
			iftype, ret);

		goto free_iface;
	}

	skw_add_vif(wiphy, iface);

	if (ndev) {
		ret = skw_netdev_init(wiphy, ndev, addr);
		if (ret) {
			skw_chip_err(skw->idx, "init netdev failed\n");
			goto iface_teardown;
		}

		ret = skw_register_netdevice(ndev);
		if (ret) {
			skw_chip_err(skw->idx, "register netdev failed\n");
			// free_percpu(ndev->tstats);
			goto ndev_teardown;
		}

		iface->procfs = skw_procfs_file(SKW_WIPHY_PENTRY(wiphy),
						netdev_name(ndev), 0444,
						&skw_iface_fops, ndev);
	} else {
		skw_ether_copy(iface->wdev.address, addr);
	}

	return iface;

ndev_teardown:
	skw_netdev_deinit(iface->ndev);
iface_teardown:
	skw_iface_teardown(wiphy, iface);
	skw_del_vif(wiphy, iface);

free_iface:
	if (ndev)
		free_netdev(ndev);
	else
		SKW_KFREE(iface);

	skw_release_inst(wiphy, inst);

	return ERR_PTR(-EBUSY);
}

int skw_del_iface(struct wiphy *wiphy, struct skw_iface *iface)
{
	if (!iface || !iface->skw)
		return 0;

	ASSERT_RTNL();

	skw_chip_dbg(iface->skw->idx, "iftype = %d, iface id: %d\n", iface->wdev.iftype, iface->id);

	skw_iface_teardown(wiphy, iface);
	skw_del_vif(wiphy, iface);
	skw_release_inst(wiphy, iface->id);

	if (iface->ndev) {
		proc_remove(iface->procfs);
		skw_netdev_deinit(iface->ndev);
		skw_unregister_netdevice(iface->ndev);
	} else if (iface->wdev.iftype == NL80211_IFTYPE_P2P_DEVICE) {
		cfg80211_unregister_wdev(&iface->wdev);
		SKW_KFREE(iface);
	}

	return 0;
}

struct skw_peer *skw_peer_alloc(void)
{
	int len;

	len = ALIGN(sizeof(struct skw_peer), SKW_PEER_ALIGN);
	len += sizeof(struct skw_ctx_entry);

	return SKW_ZALLOC(len, GFP_KERNEL);
}

static void skw_peer_release(struct rcu_head *head)
{
	struct skw_ctx_entry *entry;

	entry = container_of(head, struct skw_ctx_entry, rcu);

	// NOTE: DO NOT USE SKW_FREE HERE
	kfree(entry->peer);
}

void skw_peer_free(struct skw_peer *peer)
{
	int i;
	struct skw_ctx_entry *entry;

	if (!peer)
		return;

	for (i = 0; i < SKW_NR_TID; i++)
		skw_del_tid_rx(peer, i);

	skw_purge_key_conf(&peer->ptk_conf);
	skw_purge_key_conf(&peer->gtk_conf);

	entry = skw_ctx_entry(peer);

#ifdef CONFIG_SWT6621S_GKI_DRV
	skw_call_rcu(peer->iface->skw, &entry->rcu, skw_peer_release);
#else
	call_rcu(&entry->rcu, skw_peer_release);
#endif
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
static void skw_reorder_timeout(struct timer_list *timer)
#else
static void skw_reorder_timeout(unsigned long timer)
#endif
{
	struct skw_reorder_rx *reorder;

	reorder = container_of((void *)timer, struct skw_reorder_rx, timer);

	skw_chip_dbg(reorder->skw->idx, "tid: %d, expired sn: %d\n", reorder->tid, reorder->expired.sn);

	if (atomic_read(&reorder->ref_cnt) != reorder->expired.ref_cnt)
		return;

	trace_skw_rx_reorder_timeout(reorder->inst, reorder->peer_idx,
				reorder->tid, reorder->expired.sn);

	spin_lock_bh(&reorder->todo.lock);

	if (!reorder->todo.actived) {
		reorder->todo.actived = true;
		reorder->todo.seq = reorder->expired.sn;
		reorder->todo.reason = SKW_RELEASE_EXPIRED;
		INIT_LIST_HEAD(&reorder->todo.list);

		skw_list_add(&reorder->skw->rx_todo_list, &reorder->todo.list);
	}

	spin_unlock_bh(&reorder->todo.lock);

	skw_wakeup_rx(reorder->skw);
}

void skw_peer_init(struct skw_peer *peer, const u8 *addr, int idx)
{
	int i;
	struct skw_ctx_entry *entry;

	if (WARN_ON(!peer))
		return;

	if (idx >= SKW_MAX_PEER_SUPPORT)
		peer->flags |= SKW_PEER_FLAG_BAD_ID;

	if (!addr)
		peer->flags |= SKW_PEER_FLAG_BAD_ADDR;

	atomic_set(&peer->rx_filter, 0);
	mutex_init(&peer->ptk_conf.lock);
	mutex_init(&peer->gtk_conf.lock);

	for (i = 0; i < SKW_NR_TID; i++) {
		atomic_set(&peer->reorder[i].ref_cnt, 0);
		skw_compat_setup_timer(&peer->reorder[i].timer, skw_reorder_timeout);
		INIT_LIST_HEAD(&peer->reorder[i].todo.list);
		spin_lock_init(&peer->reorder[i].todo.lock);
		spin_lock_init(&peer->reorder[i].lock);
	}

	peer->idx = idx;
	peer->iface = NULL;
	peer->sm.addr = peer->addr;
	peer->sm.state = SKW_STATE_NONE;

	entry = skw_ctx_entry(peer);
	entry->peer = peer;
	entry->idx = idx;

	if (addr) {
		skw_ether_copy(entry->addr, addr);
		skw_ether_copy(peer->addr, addr);
	}
}

void __skw_peer_ctx_transmit(struct skw_peer_ctx *ctx, bool enable)
{
	struct skw_ctx_entry *entry;

	if (WARN_ON(!ctx))
		return;

	lockdep_assert_held(&ctx->lock);

	if (enable) {
		if (WARN_ON(!ctx->peer || ctx->peer->flags))
			return;

		entry = skw_ctx_entry(ctx->peer);
		rcu_assign_pointer(ctx->entry, entry);
		atomic_inc(&ctx->peer->iface->actived_ctx);
		SKW_SET(ctx->peer->flags, SKW_PEER_FLAG_ACTIVE);

	} else {
		entry = rcu_dereference_protected(ctx->entry,
				lockdep_is_held(&ctx->lock));
		if (entry) {
			atomic_dec(&entry->peer->iface->actived_ctx);
			SKW_CLEAR(entry->peer->flags, SKW_PEER_FLAG_ACTIVE);
		}

		RCU_INIT_POINTER(ctx->entry, NULL);
	}
}

void skw_peer_ctx_transmit(struct skw_peer_ctx *ctx, bool enable)
{
	if (!ctx)
		return;

	skw_peer_ctx_lock(ctx);
	__skw_peer_ctx_transmit(ctx, enable);
	skw_peer_ctx_unlock(ctx);
}

int __skw_peer_ctx_bind(struct skw_iface *iface, struct skw_peer_ctx *ctx,
			struct skw_peer *peer)
{
	if (WARN_ON(!iface || !ctx))
		return -EINVAL;

	lockdep_assert_held(&ctx->lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	atomic_and(~BIT(ctx->idx), &iface->peer_map);
#else
	atomic_set(&iface->peer_map, atomic_read(&iface->peer_map) & (~BIT(ctx->idx)));
#endif

	skw_peer_free(ctx->peer);
	ctx->peer = NULL;

	if (peer) {
		peer->iface = iface;
		peer->sm.inst = iface->id;
		peer->sm.addr = peer->addr;
		peer->sm.iface_iftype = iface->wdev.iftype;
		ctx->peer = peer;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
		atomic_or(BIT(ctx->idx), &iface->peer_map);
#else
		atomic_set(&iface->peer_map, atomic_read(&iface->peer_map) | BIT(ctx->idx));
#endif
	}

	return 0;
}

int skw_peer_ctx_bind(struct skw_iface *iface, struct skw_peer_ctx *ctx,
			struct skw_peer *peer)
{
	int ret;

	if (!iface || !iface->skw || !ctx)
		return -EINVAL;

	skw_chip_dbg(iface->skw->idx, "ctx: %d, %s\n", ctx->idx, peer ? "bind" : "unbind");

	mutex_lock(&ctx->lock);
	ret = __skw_peer_ctx_bind(iface, ctx, peer);
	mutex_unlock(&ctx->lock);

	return ret;
}

struct skw_peer_ctx *skw_peer_ctx(struct skw_iface *iface, const u8 *mac)
{
	int idx;
	u32 peer_idx_map;
	struct skw_peer_ctx *ctx = NULL;
	struct skw_core *skw = iface->skw;

	peer_idx_map = atomic_read(&iface->peer_map);

	if (!peer_idx_map || !mac)
		return NULL;

	while (peer_idx_map) {

		idx = ffs(peer_idx_map) - 1;

		SKW_CLEAR(peer_idx_map, BIT(idx));

		ctx = &skw->hw.lmac[iface->lmac_id].peer_ctx[idx];
		if (!ctx)
			continue;

		mutex_lock(&ctx->lock);

		if (ctx->peer && ether_addr_equal(mac, ctx->peer->addr)) {
			mutex_unlock(&ctx->lock);
			return ctx;
		}

		mutex_unlock(&ctx->lock);
	}

	return NULL;
}

void skw_iface_set_wmm_capa(struct skw_iface *iface, const u8 *ies, size_t len)
{
	int i, j, tmp;
	struct skw_wmm *wmm;
	int ac[4] = {-1, -1, -1, -1};
	unsigned int oui = SKW_OUI(0x00, 0x50, 0xF2);

#define SKW_WMM_SUBTYPE     2
#define SKW_WMM_ACM         BIT(4)

	wmm = (void *)cfg80211_find_vendor_ie(oui, SKW_WMM_SUBTYPE, ies, len);
	if (!wmm)
		goto default_wmm;

	if (wmm->version != 1)
		goto default_wmm;

	iface->wmm.qos_enabled = true;

	for (i = 3; i >= 0; i--) {
		for (tmp = i, j = 0; j < 4; j++) {
			int id = ac[j];

			if (id < 0)
				break;

			if (wmm->ac[id].aifsn > wmm->ac[tmp].aifsn) {
				tmp = id;
				ac[j] = i;
			}
		}

		if (j < 4)
			ac[j] = tmp;
	}

	for (i = 0; i < 4; i++) {
		int aci = ac[i];

		switch (aci) {
		case 0:
			iface->wmm.ac[i].aci = SKW_WMM_AC_BE;
			if (wmm->ac[aci].acm)
				iface->wmm.acm |= BIT(SKW_WMM_AC_BE);

			iface->wmm.factor[SKW_WMM_AC_BE] = SKW_WMM_AC_BE - i;
			break;
		case 1:
			iface->wmm.ac[i].aci = SKW_WMM_AC_BK;
			if (wmm->ac[aci].acm)
				iface->wmm.acm |= BIT(SKW_WMM_AC_BK);

			iface->wmm.factor[SKW_WMM_AC_BK] = SKW_WMM_AC_BK - i;
			break;
		case 2:
			iface->wmm.ac[i].aci = SKW_WMM_AC_VI;
			if (wmm->ac[aci].acm)
				iface->wmm.acm |= BIT(SKW_WMM_AC_VI);

			iface->wmm.factor[SKW_WMM_AC_VI] = (SKW_WMM_AC_VI - i) << 2;
			break;
		case 3:
			iface->wmm.ac[i].aci = SKW_WMM_AC_VO;
			if (wmm->ac[aci].acm)
				iface->wmm.acm |= BIT(SKW_WMM_AC_VI);

			iface->wmm.factor[SKW_WMM_AC_VO] = (SKW_WMM_AC_VO - i) << 1;
			break;
		default:
			break;
		}

		iface->wmm.ac[i].aifsn = wmm->ac[aci].aifsn;
		iface->wmm.ac[i].txop_limit = le16_to_cpu(wmm->ac[aci].txop_limit);

		skw_chip_dbg(iface->skw->idx, "aci: %d, aifsn: %d, txop_limit: %d, factor: %d\n",
			iface->wmm.ac[i].aci, iface->wmm.ac[i].aifsn,
			iface->wmm.ac[i].txop_limit, iface->wmm.factor[i]);
	}

	for (i = 0; i < SKW_WMM_AC_MAX; i++)
		if (iface->wmm.factor[i] < 0)
			iface->wmm.factor[i] = 0;

	skw_chip_dbg(iface->skw->idx, "wmm_acm: 0x%x\n", iface->wmm.acm);
	return;

default_wmm:
	iface->wmm.acm = 0;

	iface->wmm.ac[0].aci = 0;
	iface->wmm.ac[0].aifsn = 2;
	iface->wmm.ac[0].txop_limit = 47;

	iface->wmm.ac[1].aci = 1;
	iface->wmm.ac[1].aifsn = 2;
	iface->wmm.ac[1].txop_limit = 94;

	iface->wmm.ac[2].aci = 2;
	iface->wmm.ac[2].aifsn = 3;
	iface->wmm.ac[2].txop_limit = 0;

	iface->wmm.ac[3].aci = 3;
	iface->wmm.ac[3].aifsn = 7;
	iface->wmm.ac[3].txop_limit = 0;
}
