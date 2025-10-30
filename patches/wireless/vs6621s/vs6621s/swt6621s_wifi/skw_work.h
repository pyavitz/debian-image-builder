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

#ifndef __SKW_WORK_MSG_H__
#define __SKW_WORK_MSG_H__

struct skw_sg_node {
	struct scatterlist *sg;
	int nents;
	int status;
	int lmac_id;
	void *data;
};

enum SKW_WORK_ID {
	SKW_WORK_BA_ACTION,
	SKW_WORK_SCAN_TIMEOUT,
	SKW_WORK_ACL_CHECK,
	SKW_WORK_SET_MC_ADDR,
	SKW_WORK_SET_IP,
	SKW_WORK_TX_FREE,
	SKW_WORK_SETUP_TXBA,
	SKW_WORK_TX_ETHER_DATA,
	SKW_WORK_RADAR_PULSE,
	SKW_WORK_RADAR_CAC,
	SKW_WORK_RADAR_CAC_END,
	SKW_WORK_IFACE_RPS_INIT,
	SKW_WORK_SPLIT_MGMT_TX_STATUS,
};

struct skw_work_cb {
	struct skw_iface *iface;
	const u8 *name;
	u32 id;
};

struct skw_event_work {
	atomic_t enabled;
	struct work_struct work;
	struct sk_buff_head qlist;
};

struct skw_mgmt_status {
	void *mgmt_status_data;
	int mgmt_status_data_len;
	u64 mgmt_status_cookie;
} __packed;

static inline struct skw_work_cb *SKW_WORK_CB(struct sk_buff *skb)
{
	return (struct skw_work_cb *)skb->cb;
}

static inline void skw_event_work_init(struct skw_event_work *work,
					work_func_t func)
{
	atomic_set(&work->enabled, 0);
	skb_queue_head_init(&work->qlist);
	INIT_WORK(&work->work, func);
	atomic_set(&work->enabled, 1);
}

static inline void skw_event_work_deinit(struct skw_event_work *work)
{
	atomic_set(&work->enabled, 0);
	cancel_work_sync(&work->work);
	skb_queue_purge(&work->qlist);
}

int __skw_queue_work(struct wiphy *wiphy, struct skw_iface *iface,
		     enum SKW_WORK_ID id, void *data,
		     int dat_len, const u8 *name);

#define skw_queue_work(wiphy, iface, id, data, len) \
	__skw_queue_work(wiphy, iface, id, data, len, #id)

int skw_queue_event_work(struct wiphy *wiphy, struct skw_event_work *work,
			 struct sk_buff *skb);

void skw_assert_schedule(struct wiphy *wiphy);
void skw_unlock_schedule(struct wiphy *wiphy);
void skw_work_init(struct wiphy *wiphy);
void skw_work_deinit(struct wiphy *wiphy);
int skw_init_rps_map(struct skw_iface *iface);
void skw_work_resume(struct wiphy *wiphy);

#ifdef CONFIG_SWT6621S_GKI_DRV
void skw_call_rcu(void *skw, struct rcu_head *head, rcu_callback_t func);
#endif

#endif
