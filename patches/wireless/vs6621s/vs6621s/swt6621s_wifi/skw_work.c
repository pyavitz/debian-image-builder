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
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
#include <net/rps.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <net/netdev_rx_queue.h>
#endif

#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_mlme.h"
#include "skw_msg.h"
#include "skw_work.h"
#include "skw_timer.h"
#include "skw_recovery.h"
#include "skw_tx.h"
#include "skw_dfs.h"

#define SKW_WORK_FLAG_ASSERT        0
#define SKW_WORK_FLAG_RCU_FREE      1
#define SKW_WORK_FLAG_UNLOCK        2
#define SKW_WORK_FLAG_RESUME        3

static void skw_ap_acl_check(struct wiphy *wiphy, struct skw_iface *iface)
{
	int idx;
	struct skw_peer_ctx *ctx = NULL;
	u32 peer_idx_map = atomic_read(&iface->peer_map);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (peer_idx_map == 0)
		return;

	while (peer_idx_map) {
		idx = ffs(peer_idx_map) - 1;

		SKW_CLEAR(peer_idx_map, BIT(idx));

		ctx = &skw->hw.lmac[iface->lmac_id].peer_ctx[idx];
		if (!ctx)
			continue;

		if (ctx->peer && !skw_acl_allowed(iface, ctx->peer->addr))
			skw_mlme_ap_del_sta(wiphy, iface->ndev,
					ctx->peer->addr, false);
	}
}

static void skw_work_async_adma_tx_free(struct skw_core *skw,
				struct scatterlist *sglist, int nents)
{
	int idx;
	struct scatterlist *sg;
	struct sk_buff *skb;
	unsigned long *skb_addr, *sg_addr;

	for_each_sg(sglist, sg, nents, idx) {
		sg_addr = (unsigned long *)sg_virt(sg);

		skb_addr = sg_addr - 1;
		skb = (struct sk_buff *)*skb_addr;
		if (!virt_addr_valid(skb)) {
			/* Invalid skb pointer */
			skw_chip_dbg(skw->idx, "wrong address p_data:0x%lx from FW\n", (unsigned long)sg_addr);
			continue;
		}

		skb->dev->stats.tx_packets++;
		skb->dev->stats.tx_bytes += SKW_SKB_TXCB(skb)->skb_native_len;
		//kfree_skb(skb);
		skw_skb_kfree(skw, skb);
		atomic_dec(&skw->txqlen_pending);
	}

	SKW_KFREE(sglist);
}

#ifdef CONFIG_RPS
int skw_init_rps_map(struct skw_iface *iface)
{
	int i, cpu;
	struct rps_map *map, *old_map;
	static DEFINE_SPINLOCK(rps_map_lock);
	struct netdev_rx_queue *queue = iface->ndev->_rx;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
	map = kzalloc(max_t(unsigned int,
			    XPS_MAP_SIZE(cpumask_weight(cpu_online_mask)), L1_CACHE_BYTES),
		      GFP_KERNEL);
#else
	map = kzalloc(max_t(unsigned int,
			    RPS_MAP_SIZE(cpumask_weight(cpu_online_mask)), L1_CACHE_BYTES),
		      GFP_KERNEL);
#endif
    if (!map)
		return -ENOMEM;

	i = 0;
	for_each_cpu(cpu, cpu_online_mask)
		if (cpu != iface->cpu_id)
			map->cpus[i++] = cpu;

	if (i) {
		map->len = i;
	} else {
		kfree(map);
		map = NULL;
	}

	spin_lock(&rps_map_lock);
	old_map = rcu_dereference_protected(queue->rps_map,
					    lockdep_is_held(&rps_map_lock));
	rcu_assign_pointer(queue->rps_map, map);
	spin_unlock(&rps_map_lock);

	if (map) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
		static_key_slow_inc(&rps_needed.key);
#else
		static_key_slow_inc(&rps_needed);
#endif
	}

	if (old_map) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
		static_key_slow_dec(&rps_needed.key);
#else
		static_key_slow_dec(&rps_needed);
#endif
		kfree_rcu(old_map, rcu);
    }

	return 0;
}
#endif

static int skw_work_process(struct wiphy *wiphy, struct skw_iface *iface,
			int work_id, void *data, int data_len, const u8 *name)
{
	int ret = 0;
	struct skw_sg_node *node;
	struct skw_ba_action *ba;
	struct skw_core *skw = wiphy_priv(wiphy);
//	int *umask;
	struct skw_mgmt_status *status;

	if (!iface) {
		ret = -ENOENT;
		goto exit;
	}

	skw_chip_log(skw->idx, SKW_WORK, "[%s]: iface: %d, %s (id: %d)\n",
		SKW_TAG_WORK, iface ? iface->id : -1, name, work_id);

	switch (work_id) {
	case SKW_WORK_BA_ACTION:
		ret = skw_send_msg(wiphy, iface->ndev, SKW_CMD_BA_ACTION,
				data, data_len, NULL, 0);
		break;

	case SKW_WORK_SCAN_TIMEOUT:
		skw_scan_done(skw, iface, true);
		break;

	case SKW_WORK_ACL_CHECK:
		skw_ap_acl_check(wiphy, iface);
		break;

	case SKW_WORK_SET_MC_ADDR:
		ret = skw_send_msg(wiphy, iface->ndev, SKW_CMD_SET_MC_ADDR,
				data, data_len, NULL, 0);
		break;

	case SKW_WORK_SET_IP:
		ret = skw_send_msg(wiphy, iface->ndev, SKW_CMD_SET_IP,
				data, data_len, NULL, 0);
		break;

	case SKW_WORK_TX_FREE:
		node = data;
		skw_work_async_adma_tx_free(skw, node->sg, node->nents);

		break;

	case SKW_WORK_SETUP_TXBA:
		ba = data;

		skw_chip_dbg(skw->idx, "%s, iface: %d, peer: %d, tid: %d\n",
			name, iface->id, ba->peer_idx, ba->tid);

		ret = skw_send_msg(wiphy, iface->ndev, SKW_CMD_BA_ACTION,
				data, data_len, NULL, 0);
		if (ret) {
			struct skw_peer_ctx *ctx;

			skw_chip_err(skw->idx, "setup TXBA failed, ret: %d\n", ret);

			ctx = skw_get_ctx(skw, iface->lmac_id, ba->peer_idx);

			skw_peer_ctx_lock(ctx);

			if (ctx->peer)
				SKW_CLEAR(ctx->peer->txba.bitmap, BIT(ba->tid));

			skw_peer_ctx_unlock(ctx);
		}

		break;

	case SKW_WORK_TX_ETHER_DATA:
		ret = skw_send_msg(wiphy, iface->ndev, SKW_CMD_TX_DATA_FRAME,
				data, data_len, NULL, 0);
		break;
#if 0
	case SKW_WORK_RADAR_PULSE:
		skw_dfs_radar_pulse_event(wiphy, iface, data, data_len);
		break;
#endif

	case SKW_WORK_IFACE_RPS_INIT:
#if 0
#ifdef CONFIG_RPS
		umask = (int *)data;
		skw_init_rps_map(iface, *umask);
#endif
#endif
		break;

	case SKW_WORK_SPLIT_MGMT_TX_STATUS:
		status = data;
		skw_mgmt_tx_status(iface, status->mgmt_status_cookie,
			status->mgmt_status_data, status->mgmt_status_data_len, true);

		skw_chip_dbg(skw->idx, "split ack cookie:0x%llx\n", status->mgmt_status_cookie);

		SKW_KFREE(status->mgmt_status_data);
		break;

	default:
		skw_chip_info(skw->idx, "invalid work: %d\n", work_id);
		break;
	}

exit:
	return ret;
}

//workaround for bug3384
static void skw_work_unlock(struct work_struct *work)
{
	struct skw_core *skw = container_of(work, struct skw_core, work_unlock);

	skw_del_timer_work(skw, skw->cmd.data);
	__pm_relax(skw->cmd.ws);
	up(&skw->cmd.lock);
}

static void skw_work(struct work_struct *work)
{
	struct sk_buff *skb;
	struct skw_work_cb *cb;
	struct skw_core *skw = container_of(work, struct skw_core, work);
	struct wiphy *wiphy = priv_to_wiphy(skw);

	while (READ_ONCE(skw->work_data.flags) || skb_queue_len(&skw->work_data.work_list)) {

		if (test_bit(SKW_WORK_FLAG_RCU_FREE, &skw->work_data.flags)) {
			struct rcu_head *head;

			spin_lock_bh(&skw->work_data.rcu_lock);

			head = skw->work_data.rcu_hdr;
			if (head)
				skw->work_data.rcu_hdr = head->next;

			spin_unlock_bh(&skw->work_data.rcu_lock);

			if (head) {
				synchronize_rcu();
				head->func(head);
			} else {
				skw->work_data.rcu_tail = &skw->work_data.rcu_hdr;
				clear_bit(SKW_WORK_FLAG_RCU_FREE, &skw->work_data.flags);
			}
		}

		if (test_and_clear_bit(SKW_WORK_FLAG_ASSERT, &skw->work_data.flags))
			skw_hw_assert(skw, false);

		if (test_and_clear_bit(SKW_WORK_FLAG_RESUME, &skw->work_data.flags))
			skw_resume(wiphy);

		if (!skb_queue_len(&skw->work_data.work_list))
			continue;

		skb = skb_dequeue(&skw->work_data.work_list);
		cb = SKW_WORK_CB(skb);
		if (skb && cb)
			skw_work_process(wiphy, cb->iface, cb->id,
				skb->data, skb->len, cb->name);

		kfree_skb(skb);
	}
}

void skw_assert_schedule(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	set_bit(SKW_WORK_FLAG_ASSERT, &skw->work_data.flags);
	schedule_work(&skw->work);
}

void skw_work_resume(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	set_bit(SKW_WORK_FLAG_RESUME, &skw->work_data.flags);
	schedule_work(&skw->work);
}

void skw_unlock_schedule(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	//set_bit(SKW_WORK_FLAG_UNLOCK, &skw_work_flags);
	schedule_work(&skw->work_unlock);
	//schedule_work(&skw->work);
}

#ifdef CONFIG_SWT6621S_GKI_DRV
void skw_call_rcu(void *core, struct rcu_head *head, rcu_callback_t func)
{
	struct skw_core *skw = core;

	spin_lock_bh(&skw->work_data.rcu_lock);

	head->func = func;
	head->next = NULL;

	*skw->work_data.rcu_tail = head;
	skw->work_data.rcu_tail = &head->next;

	spin_unlock_bh(&skw->work_data.rcu_lock);

	set_bit(SKW_WORK_FLAG_RCU_FREE, &skw->work_data.flags);

	schedule_work(&skw->work);
}
#endif

int __skw_queue_work(struct wiphy *wiphy, struct skw_iface *iface,
		     enum SKW_WORK_ID id, void *data,
		     int dat_len, const u8 *name)
{
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_work_cb *wcb;
	struct sk_buff *skb;

	skb = dev_alloc_skb(dat_len);
	if (!skb)
		return -ENOMEM;

	if (data)
		skw_put_skb_data(skb, data, dat_len);

	wcb = SKW_WORK_CB(skb);
	wcb->iface = iface;
	wcb->id = id;
	wcb->name = name;

	skb_queue_tail(&skw->work_data.work_list, skb);
	schedule_work(&skw->work);

	return 0;
}

int skw_queue_event_work(struct wiphy *wiphy, struct skw_event_work *work,
			 struct sk_buff *skb)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!atomic_read(&work->enabled))
		return -EINVAL;

	skb_queue_tail(&work->qlist, skb);

	if (!work_pending(&work->work))
		queue_work(skw->event_wq, &work->work);

	return 0;
}

void skw_work_init(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw->work_data.rcu_hdr = NULL;
	skw->work_data.rcu_tail = &skw->work_data.rcu_hdr;

	spin_lock_init(&skw->work_data.rcu_lock);
	skb_queue_head_init(&skw->work_data.work_list);
	INIT_WORK(&skw->work, skw_work);
	INIT_WORK(&skw->work_unlock, skw_work_unlock);
}

void skw_work_deinit(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	flush_work(&skw->work_unlock);
	flush_work(&skw->work);
	skb_queue_purge(&skw->work_data.work_list);
}
