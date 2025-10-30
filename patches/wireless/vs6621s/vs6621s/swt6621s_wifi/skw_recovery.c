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

#include <linux/skbuff.h>

#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_msg.h"
#include "skw_iw.h"
#include "skw_calib.h"
#include "skw_recovery.h"
#include "skw_mlme.h"
#include "skw_rx.h"
#include "skw_tx.h"
#include "skw_regd.h"

static inline void
skw_recovery_sta_disconnect(struct net_device *ndev, u8 *addr)
{
	struct skw_iface *iface = netdev_priv(ndev);

	if (iface->sta.sme_external)
		skw_tx_mlme_mgmt(ndev, IEEE80211_STYPE_DEAUTH, addr, addr, 3);
	else
		skw_disconnected(ndev, 3, NULL, 0, true, GFP_KERNEL);
}

static int skw_recovery_sta(struct wiphy *wiphy, struct skw_recovery_data *rd,
				struct skw_iface *iface)
{
	int ret;
	u32 peer_map = rd->iface[iface->id].peer_map;
	struct skw_sta_core *core = &iface->sta.core;
	struct net_device *dev = iface->ndev;
#ifndef SKW_STATE_RECOVERY
	struct skw_core *skw = wiphy_priv(wiphy);
#endif

#ifdef SKW_STATE_RECOVERY
	// TODO:
	// recovery peer state

	struct cfg80211_bss *cbss;

	cbss = cfg80211_get_bss(wiphy, core->bss.channel, core->bss.bssid,
				core->bss.ssid, core->bss.ssid_len,
				IEEE80211_BSS_TYPE_ANY, IEEE80211_PRIVACY_ANY);
	if (!cbss) {
		if (!skw_cmd_unjoin(wiphy, dev, peer->addr, 3, true))
			peer->flags |= SKW_PEER_FLAG_DEAUTHED;

		skw_recovery_sta_disconnect(dev, core->bss.bssid);
		return 0;
	}

	ret = skw_join(wiphy, dev, cbss, false);
	if (ret) {
		if (!skw_cmd_unjoin(wiphy, dev, peer->addr, 3, true))
			peer->flags |= SKW_PEER_FLAG_DEAUTHED;

		skw_recovery_sta_disconnect(dev, core->bss.bssid);
		return 0;
	}

	// set key
	// set ip

	cfg80211_put_bss(wiphy, cbss);
#else
	while (peer_map) {
		u8 idx = ffs(peer_map) - 1;
		struct skw_peer *peer = rd->peer[iface->lmac_id][idx];

		SKW_CLEAR(peer_map, BIT(idx));

		if (!peer || peer->flags & SKW_PEER_FLAG_DEAUTHED)
			continue;

		if (ether_addr_equal(peer->addr, core->bss.bssid)) {
			del_timer_sync(&core->timer);
			cancel_work_sync(&iface->sta.work);

			ret = skw_cmd_unjoin(wiphy, dev, peer->addr,
					SKW_LEAVE, true);
			if (ret)
				skw_chip_warn(skw->idx, "failed, sta: %pM, ret: %d\n",
					 peer->addr, ret);

			skw_set_state(skw, &core->sm, SKW_STATE_NONE);
			memset(&core->bss, 0, sizeof(struct skw_bss_cfg));
			core->bss.ctx_idx = SKW_INVALID_ID;

			skw_recovery_sta_disconnect(dev, peer->addr);
			peer->flags |= SKW_PEER_FLAG_DEAUTHED;

		} else {
			/* TDLS */
			cfg80211_tdls_oper_request(dev, peer->addr,
					NL80211_TDLS_TEARDOWN,
					SKW_WLAN_REASON_TDLS_TEARDOWN_UNREACHABLE,
					GFP_KERNEL);

			peer->flags |= SKW_PEER_FLAG_DEAUTHED;
		}
	}

	atomic_set(&iface->actived_ctx, 0);
#endif

	return 0;
}

static void
skw_recovery_sap_flush_sta(struct wiphy *wiphy, struct skw_recovery_data *rd,
			struct skw_iface *iface, u8 subtype, u16 reason)
{
	int idx, ret;
	u8 addr[ETH_ALEN];
	struct skw_peer *peer;
	struct skw_core *skw = wiphy_priv(wiphy);
	u32 peer_map = rd->iface[iface->id].peer_map;
	u8 lmac_id = iface->lmac_id;

	while (peer_map) {

		if (test_bit(SKW_FLAG_FW_ASSERT, &skw->flags))
			break;

		idx = ffs(peer_map) - 1;
		SKW_CLEAR(peer_map, BIT(idx));

		peer = rd->peer[lmac_id][idx];
		if (!peer || peer->flags & SKW_PEER_FLAG_DEAUTHED)
			continue;

		peer->flags |= SKW_PEER_FLAG_DEAUTHED;
		skw_mlme_ap_remove_client(iface, peer->addr);
		skw_del_sta_event(iface, peer->addr, SKW_LEAVE);
	}

	memset(addr, 0xff, ETH_ALEN);
	ret = skw_cmd_del_sta(wiphy, iface->ndev, addr, subtype, reason, true);
	if (ret)
		skw_chip_warn(skw->idx, "failed, sta: %pM, ret: %d\n", addr, ret);
}

static int skw_recovery_sap(struct wiphy *wiphy, struct skw_recovery_data *rd,
			struct skw_iface *iface)
{
	int ret, size;
	struct skw_startap_param *param;
	struct net_device *ndev = iface->ndev;
	struct skw_startap_resp resp = {};
	struct skw_core *skw = wiphy_priv(wiphy);

	param = rd->iface[iface->id].param;
	if (!param) {
		skw_chip_err(skw->idx, "invalid param\n");
		return -EINVAL;
	}

	ret = skw_sap_set_mib(wiphy, iface->ndev,
			(u8 *)param + param->beacon_tail_offset,
			param->beacon_tail_len);
	if (ret) {
		skw_chip_err(skw->idx, "set tlv failed, ret: %d\n", ret);
		return ret;
	}

	size = rd->iface[iface->id].size;

	ret = skw_send_msg(wiphy, ndev, SKW_CMD_START_AP, param, size, &resp, sizeof(resp));
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	skw_startap_resp_handler(iface->skw, iface, &resp);

	skw_dpd_set_coeff_params(wiphy, ndev, param->chan, param->center_chn1,
				 param->center_chn2, param->chan_width);

	skw_recovery_sap_flush_sta(wiphy, rd, iface, 12, SKW_LEAVE);

	return 0;
}

static int skw_recovery_ibss(struct wiphy *wiphy, struct skw_iface *iface)
{
	return 0;
}

static int skw_recovery_p2p_dev(struct wiphy *wiphy, struct skw_iface *iface)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "done\n");

	return 0;
}

static void
skw_recovery_prepare(struct skw_core *skw, struct skw_recovery_data *rd)
{
	int i, j;
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface;

	skw->cmd.seq = 0;
	skw->skw_event_sn = 0;

	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		atomic_set(&skw->hw.lmac[i].fw_credit, 0);

		WRITE_ONCE(skw->hw.lmac[i].iface_bitmap, 0);
	}

	if (test_and_set_bit(SKW_FLAG_FW_CHIP_RECOVERY, &skw->flags)) {
		skw_chip_info(skw->idx, "received recovery event during recovery");


		for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
			for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
				ctx = &skw->hw.lmac[i].peer_ctx[j];

				skw_peer_ctx_lock(ctx);

				rcu_assign_pointer(ctx->entry, NULL);
				ctx->peer = NULL;

				skw_peer_ctx_unlock(ctx);
			}
		}

		spin_lock_bh(&skw->vif.lock);

		for (i = 0; i < SKW_NR_IFACE; i++) {
			iface = skw->vif.iface[i];
			if (!iface)
				continue;

			for (j = 0; j <= SKW_WMM_AC_MAX; j++) {
				skb_queue_purge(&iface->txq[j]);
				skb_queue_purge(&iface->tx_cache[j]);
			}

			atomic_set(&iface->peer_map, 0);
		}

		spin_unlock_bh(&skw->vif.lock);

		return;
	}

	mutex_lock(&rd->lock);
	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
			ctx = &skw->hw.lmac[i].peer_ctx[j];

			skw_peer_ctx_lock(ctx);

			rcu_assign_pointer(ctx->entry, NULL);
			rd->peer[i][j] = ctx->peer;
			ctx->peer = NULL;

			skw_peer_ctx_unlock(ctx);
		}
	}

	spin_lock_bh(&skw->vif.lock);

	for (i = 0; i < SKW_NR_IFACE; i++) {
		iface = skw->vif.iface[i];
		if (!iface)
			continue;

		for (j = 0; j <= SKW_WMM_AC_MAX; j++) {
			skb_queue_purge(&iface->txq[j]);
			skb_queue_purge(&iface->tx_cache[j]);
		}

		rd->iface[i].peer_map = atomic_read(&iface->peer_map);
		atomic_set(&iface->peer_map, 0);
	}

	spin_unlock_bh(&skw->vif.lock);

	mutex_unlock(&rd->lock);

	skw_dpd_zero(&skw->dpd);
}

static int skw_recovery_iface(struct work_struct *wk, u8 iface_idx)
{
	int ret = 0;
	struct skw_core *skw = container_of(wk, struct skw_core, recovery_work);
	struct wiphy *wiphy = priv_to_wiphy(skw);
	struct skw_recovery_data *rd = &skw->recovery_data;
	struct skw_iface *iface = skw->vif.iface[iface_idx];

	if (!iface)
		goto ret;

	if (test_bit(SKW_FLAG_FW_ASSERT, &skw->flags)) {
		ret = -EAGAIN;
		goto ret;
	}

	skw_chip_info(skw->idx, "%s: inst: %d\n",
			skw_iftype_name(iface->wdev.iftype), iface_idx);

	ret = skw_cmd_open_dev(wiphy, iface->id, iface->addr,
			iface->wdev.iftype, 0);
	if (ret) {
		skw_chip_err(skw->idx, "open %s failed, inst: %d, ret: %d\n",
			skw_iftype_name(iface->wdev.iftype),
			iface->id, ret);

		skw_hw_assert(skw, false);

		goto ret;
	}

	spin_lock_bh(&skw->vif.lock);
	SKW_SET(iface->flags, SKW_IFACE_FLAG_OPENED);
	skw->vif.opened_dev++;
	spin_unlock_bh(&skw->vif.lock);

	mutex_lock(&rd->lock);

	switch (iface->wdev.iftype) {
	case NL80211_IFTYPE_STATION:
		if (iface->flags & SKW_IFACE_FLAG_LEGACY_P2P_DEV) {
			skw_recovery_p2p_dev(wiphy, iface);
			break;
		}

		skw_fallthrough;

	case NL80211_IFTYPE_P2P_CLIENT:
		skw_recovery_sta(wiphy, rd, iface);
		break;

	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		skw_recovery_sap(wiphy, rd, iface);
		break;

	case NL80211_IFTYPE_ADHOC:
		skw_recovery_ibss(wiphy, iface);
		break;

	case NL80211_IFTYPE_P2P_DEVICE:
		skw_recovery_p2p_dev(wiphy, iface);
		break;

	default:
		break;
	}

	mutex_unlock(&rd->lock);

ret:
	return ret;
}

static void skw_recovery_work(struct work_struct *wk)
{
	int i, j, ret;
	struct skw_chip_info chip;
	struct skw_core *skw = container_of(wk, struct skw_core, recovery_work);
	struct wiphy *wiphy = priv_to_wiphy(skw);
	struct skw_recovery_data *rd = &skw->recovery_data;
	struct skw_iface *iface;

	skw_chip_info(skw->idx, "start\n");

	skw_recovery_prepare(skw, rd);

	spin_lock_bh(&skw->vif.lock);
	skw->vif.opened_dev = 0;
	for (i = 0; i < SKW_NR_IFACE; i++) {
		iface = skw->vif.iface[i];

		if (iface)
			SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_OPENED);
	}
	spin_unlock_bh(&skw->vif.lock);

	skw_wifi_enable(skw->hw_pdata);

	ret = skw_register_rx_callback(skw, skw_rx_cb, skw, skw_rx_cb, skw);
	if (ret < 0)
		skw_chip_err(skw->idx, "register rx callback failed, ret: %d\n", ret);

	skw_hw_xmit_init(skw, skw->hw.dma);

	clear_bit(SKW_FLAG_FW_ASSERT, &skw->flags);
	clear_bit(SKW_FLAG_BLOCK_TX, &skw->flags);
	clear_bit(SKW_FLAG_FW_MAC_RECOVERY, &skw->flags);
	clear_bit(SKW_FLAG_FW_THERMAL, &skw->flags);

	skw_sync_cmd_event_version(wiphy);

	ret = skw_sync_chip_info(wiphy, &chip);
	if (ret) {
		skw_chip_err(skw->idx, "sync chip info failed, ret: %d\n", ret);
		goto ret;
	}

	ret = skw_calib_download(wiphy, skw->fw.calib_file);
	if (ret) {
		skw_chip_err(skw->idx, "calib download failed, ret: %d\n", ret);
		goto ret;
	}

	skw_init_mib(wiphy);

	rtnl_lock();

	skw_set_wiphy_regd(wiphy, skw->country);

	if (skw->config->regd.country[0] != '0' &&
	    skw->config->regd.country[1] != '0')
		skw_set_regdom(wiphy, skw->country);

	rtnl_unlock();

	for (i = 0; i < SKW_NR_IFACE; i++) {
		struct skw_iface *iface = skw->vif.iface[i];

		if (!iface)
			continue;

		if (iface->wdev.iftype == NL80211_IFTYPE_STATION ||
			iface->wdev.iftype == NL80211_IFTYPE_P2P_DEVICE) {
			ret = skw_recovery_iface(wk, i);
			if (ret)
				goto ret;
		}
	}

	for (i = 0; i < SKW_NR_IFACE; i++) {
		struct skw_iface *iface = skw->vif.iface[i];

		if (!iface)
			continue;

		if (!(iface->wdev.iftype == NL80211_IFTYPE_STATION ||
			iface->wdev.iftype == NL80211_IFTYPE_P2P_DEVICE)) {
			ret = skw_recovery_iface(wk, i);
			if (ret)
				goto ret;
		}
	}

	if (!ret) {
		for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
			for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
				skw_peer_free(rd->peer[i][j]);
				rd->peer[i][j] = NULL;
			}
		}

		clear_bit(SKW_FLAG_FW_CHIP_RECOVERY, &skw->flags);

#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
		if (test_bit(SKW_FLAG_SWITCHING_USB_MODE, &skw->flags)) {
			skw_chip_dbg(skw->idx, "usb switch done");
			complete(&skw->usb_switch_done);
		}
#endif
	}

ret:
	skw_chip_dbg(skw->idx, "end %d\n", ret);
}

void skw_recovery_del_peer(struct skw_iface *iface, u8 peer_idx)
{
	struct skw_recovery_data *rd = &iface->skw->recovery_data;
	u8 lmac_id = iface->lmac_id;

	if (!test_bit(SKW_FLAG_FW_CHIP_RECOVERY, &iface->skw->flags))
		return;

	mutex_lock(&rd->lock);

	if (rd->peer[lmac_id])
		rd->peer[lmac_id][peer_idx]->flags |= SKW_PEER_FLAG_DEAUTHED;

	mutex_unlock(&rd->lock);
}

int skw_recovery_data_update(struct skw_iface *iface, void *param, int len)
{
	void *data;
	struct skw_recovery_data *rd = &iface->skw->recovery_data;

	if (!param)
		return 0;

	data = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memcpy(data, param, len);

	mutex_lock(&rd->lock);

	SKW_KFREE(rd->iface[iface->id].param);

	rd->iface[iface->id].param = data;
	rd->iface[iface->id].size = len;

	mutex_unlock(&rd->lock);

	return 0;
}

void skw_recovery_data_clear(struct skw_iface *iface)
{
	struct skw_recovery_data *rd = &iface->skw->recovery_data;

	mutex_lock(&rd->lock);

	rd->iface[iface->id].size = 0;
	rd->iface[iface->id].peer_map = 0;
	SKW_KFREE(rd->iface[iface->id].param);

	mutex_unlock(&rd->lock);
}

int skw_recovery_init(struct skw_core *skw)
{
	mutex_init(&skw->recovery_data.lock);
	INIT_WORK(&skw->recovery_work, skw_recovery_work);
#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
	init_completion(&skw->usb_switch_done);
#endif

	return 0;
}

void skw_recovery_deinit(struct skw_core *skw)
{
	int i, j;
	struct skw_recovery_data *rd = &skw->recovery_data;

	mutex_lock(&rd->lock);

	cancel_work_sync(&skw->recovery_work);

	for (i = 0; i < SKW_NR_IFACE; i++)
		SKW_KFREE(rd->iface[i].param);

	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
			skw_peer_free(rd->peer[i][j]);
			rd->peer[i][j] = NULL;
		}
	}

	mutex_unlock(&rd->lock);
}
