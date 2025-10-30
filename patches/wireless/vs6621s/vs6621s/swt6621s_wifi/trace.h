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

#if !defined(__SKW_TRACE_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __SKW_TRACE_H__

#include <linux/tracepoint.h>
#include <linux/etherdevice.h>
#include <linux/version.h>

#include "skw_rx.h"
#include "skw_msg.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM skwifi

TRACE_EVENT(skw_tx_add_credit,
	    TP_PROTO(int mac, int cred),
	    TP_ARGS(mac, cred),

	    TP_STRUCT__entry(
	    __field(int, mac)
	    __field(int, cred)
	    ),

	    TP_fast_assign(
	    __entry->mac = mac;
	    __entry->cred = cred;
	    ),

	    TP_printk("mac: %d, credit: %d", __entry->mac, __entry->cred)
);

TRACE_EVENT(skw_tx_xmit,
	    TP_PROTO(u8 *mac, int peer_idx, int ip_prot, int fix_rate,
		     int do_csum, int tid, int qlen),
	    TP_ARGS(mac, peer_idx, ip_prot, fix_rate, do_csum, tid, qlen),
	    TP_STRUCT__entry(
	    __array(u8, dest, ETH_ALEN)
	    __field(int, peer_idx)
	    __field(int, ip_prot)
	    __field(int, fix_rate)
	    __field(int, do_csum)
	    __field(int, tid)
	    __field(int, qlen)
	    ),

	    TP_fast_assign(
	    memcpy(__entry->dest, mac, ETH_ALEN);
	    __entry->peer_idx = peer_idx;
	    __entry->ip_prot = ip_prot;
	    __entry->fix_rate = fix_rate;
	    __entry->do_csum = do_csum;
	    __entry->tid = tid;
	    __entry->qlen = qlen;
	    ),

	    TP_printk("dest: %pM, peer: %d, ip_prot: %d, fix rate: %d, csum: %d, queue[%d]: %d",
		      __entry->dest, __entry->peer_idx, __entry->ip_prot,
		      __entry->fix_rate, __entry->do_csum,
		      __entry->tid, __entry->qlen)
);

TRACE_EVENT(skw_tx_info,
	    TP_PROTO(int mac, int ac, int cred, int tx_count, int qlen, int pending_qlen),
	    TP_ARGS(mac, ac, cred, tx_count, qlen, pending_qlen),

	    TP_STRUCT__entry(
	    __field(int, mac)
	    __field(int, ac)
	    __field(int, cred)
	    __field(int, tx_count)
	    __field(int, qlen)
		__field(int, pending_qlen)
	    ),

	    TP_fast_assign(
	    __entry->mac = mac;
	    __entry->ac = ac;
	    __entry->cred = cred;
	    __entry->tx_count = tx_count;
	    __entry->qlen = qlen;
		__entry->pending_qlen = pending_qlen;
	    ),

	    TP_printk("mac: %d, ac: %d, credit: %d, qlen: %d, tx count: %d pending_qlen:%d",
		      __entry->mac, __entry->ac, __entry->cred,
		      __entry->qlen, __entry->tx_count, __entry->pending_qlen)
);

TRACE_EVENT(skw_tx_exit_check,
	TP_PROTO(int mac, int credit, int pending_qlen),
	TP_ARGS(mac, credit, pending_qlen),

	TP_STRUCT__entry(
	__field(int, mac)
	__field(int, credit)
	__field(int, pending_qlen)
	),

	TP_fast_assign(
	__entry->mac = mac;
	__entry->credit = credit;
	__entry->pending_qlen = pending_qlen;
	),

	TP_printk("mac: %d, credit: %d pending_qlen:%d",
		  __entry->mac, __entry->credit, __entry->pending_qlen)
);

TRACE_EVENT(skw_txlp_tx_list,
	    TP_PROTO(s8 *name, int id),
	    TP_ARGS(name, id),

	    TP_STRUCT__entry(
	    __field(s8 *, name)
		__field(int, id)
	    ),

	    TP_fast_assign(
	    __entry->name = name;
		__entry->id = id;
	    ),

	    TP_printk("name: %s, iface id:%d",
		      __entry->name, __entry->id)
);

TRACE_EVENT(skw_txlp_mac,
	    TP_PROTO(int mac, int cred, int current_qlen, int base),
	    TP_ARGS(mac, cred, current_qlen, base),

	    TP_STRUCT__entry(
		__field(int, mac)
		__field(int, cred)
		__field(int, current_qlen)
		__field(int, base)
	    ),

	    TP_fast_assign(
		__entry->mac = mac;
		__entry->cred = cred;
		__entry->current_qlen = current_qlen;
		__entry->base = base;
	    ),

	    TP_printk("mac:%d, cred:%d, current_qlen:%d, base:%d",
			  __entry->mac,
			  __entry->cred,
			  __entry->current_qlen,
			  __entry->base)
);

TRACE_EVENT(skw_txq_prepare,
	    TP_PROTO(s8 *name, int qlen),
	    TP_ARGS(name, qlen),

	    TP_STRUCT__entry(
	    __field(s8 *, name)
		__field(int, qlen)
	    ),

	    TP_fast_assign(
	    __entry->name = name;
		__entry->qlen = qlen;
	    ),

	    TP_printk("name: %s, qlen:%d",
		      __entry->name, __entry->qlen)
);

TRACE_EVENT(skw_get_credit,
	    TP_PROTO(int credit, int mac),
	    TP_ARGS(credit, mac),

	    TP_STRUCT__entry(
		__field(int, credit)
		__field(int, mac)
	    ),

	    TP_fast_assign(
	    __entry->credit = credit;
		__entry->mac = mac;
	    ),

	    TP_printk("credit: %d, mac:%d",
		      __entry->credit, __entry->mac)
);

#if 0
TRACE_EVENT(skw_tx_thread,
	    TP_PROTO(u32 cred, u32 nents, u32 tx_data_len, u32 tx_buff_len, u8 *tx_ac, u32 *cached),
	    TP_ARGS(cred, nents, tx_data_len, tx_buff_len, tx_ac, cached),

	    TP_STRUCT__entry(
	    __field(u32, cred)
	    __field(u32, nents)
	    __field(u32, tx_data_len)
	    __field(u32, tx_buff_len)
	    __array(u8, tx_ac, SKW_WMM_AC_MAX)
	    __array(u32, cached, SKW_WMM_AC_MAX)
	    ),

	    TP_fast_assign(
	    __entry->cred = cred;
	    __entry->nents = nents;
	    __entry->tx_data_len = tx_data_len;
	    __entry->tx_buff_len = tx_buff_len;
	    memcpy(__entry->tx_ac, tx_ac, sizeof(u32) * SKW_WMM_AC_MAX);
	    memcpy(__entry->cached, cached, sizeof(u32) * SKW_WMM_AC_MAX);
	    ),

	    TP_printk("creds: %d, tx: %d, len: %d/%d, VO[%d:%d] VI[%d:%d] BE[%d:%d] BK[%d:%d]",
		      __entry->cred, __entry->nents,
		      __entry->tx_data_len,
		      __entry->tx_buff_len,

		      __entry->tx_ac[SKW_WMM_AC_VO],
		      __entry->cached[SKW_WMM_AC_VO],

		      __entry->tx_ac[SKW_WMM_AC_VI],
		      __entry->cached[SKW_WMM_AC_VI],

		      __entry->tx_ac[SKW_WMM_AC_BE],
		      __entry->cached[SKW_WMM_AC_BE],

		      __entry->tx_ac[SKW_WMM_AC_BK],
		      __entry->cached[SKW_WMM_AC_BK])
);

TRACE_EVENT(skw_tx_thread_ret,
	    TP_PROTO(int ret, u32 pending_qlen, u32 ac_reset),
	    TP_ARGS(ret, pending_qlen, ac_reset),

	    TP_STRUCT__entry(
	    __field(int, ret)
	    __field(u32, pending_qlen)
	    __field(u32, ac_reset)
	    ),

	    TP_fast_assign(
	    __entry->ret = ret;
	    __entry->pending_qlen = pending_qlen;
	    __entry->ac_reset = ac_reset;
	    ),

	    TP_printk("ret: %d, pending: %d, ac_reset: 0x%x",
		      __entry->ret, __entry->pending_qlen, __entry->ac_reset)
);

#endif

TRACE_EVENT(skw_tx_runing,
	    TP_PROTO(int tx, int pending_qlen, long timeout, int keep_running),
	    TP_ARGS(tx, pending_qlen, timeout, keep_running),

	    TP_STRUCT__entry(
	    __field(int, tx)
	    __field(int, pending_qlen)
	    __field(long, timeout)
	    __field(int, keep_running)
	    ),

	    TP_fast_assign(
	    __entry->tx = tx;
	    __entry->pending_qlen = pending_qlen;
	    __entry->timeout = timeout;
	    __entry->keep_running = keep_running;
	    ),

	    TP_printk("tx: %d, pending_qlen: %d, timeout: %ld, pending_qlen: %d",
		      __entry->tx, __entry->pending_qlen,
		      __entry->timeout, __entry->keep_running)
);

TRACE_EVENT(skw_tx_pcie_edma_free,
	    TP_PROTO(u16 count),
	    TP_ARGS(count),

	    TP_STRUCT__entry(
	    __field(u16, count)
	    ),

	    TP_fast_assign(
	    __entry->count = count;
	    ),

	    TP_printk("count: %u",
		      __entry->count)
);

TRACE_EVENT(skw_rx_set_reorder_timer,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 seq, unsigned long rx_time, unsigned long timeout),
	    TP_ARGS(inst, pid, tid, seq, rx_time, timeout),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, seq)
	    __field(unsigned long, rx_time)
	    __field(unsigned long, timeout)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->seq = seq;
	    __entry->rx_time = rx_time;
	    __entry->timeout = timeout;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, seq: %d, rx_time: %ld, timeout: %ld",
		      __entry->inst, __entry->pid,
		      __entry->tid, __entry->seq,
		      __entry->rx_time, __entry->timeout)
);

TRACE_EVENT(skw_rx_reorder_timeout,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 expired_sn),
	    TP_ARGS(inst, pid, tid, expired_sn),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, expired_sn)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->expired_sn = expired_sn;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, expired_sn: %d",
		    __entry->inst, __entry->pid,
		    __entry->tid, __entry->expired_sn)
);

TRACE_EVENT(skw_rx_expired_release,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 expired_sn),
	    TP_ARGS(inst, pid, tid, expired_sn),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, expired_sn)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->expired_sn = expired_sn;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, expired_sn: %d",
		    __entry->inst, __entry->pid,
		    __entry->tid, __entry->expired_sn)
);

TRACE_EVENT(skw_rx_data,
	    TP_PROTO(u8 inst, u8 pid, u8 tid, u8 filter, u16 seq, u8 qos,
		     u8 retry, u8 amsdu, u8 idx, u8 first, u8 last, bool fake_ack),
	    TP_ARGS(inst, pid, tid, filter, seq, qos, retry, amsdu,
		    idx, first, last, fake_ack),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u8, tid)
	    __field(u8, filter)
	    __field(u16, seq)
	    __field(u8, qos)
	    __field(u8, retry)
	    __field(u8, amsdu)
	    __field(u8, idx)
	    __field(u8, first)
	    __field(u8, last)
	    __field(bool, fake_ack)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->filter = filter;
	    __entry->seq = seq;
	    __entry->qos = qos;
	    __entry->retry = retry;
	    __entry->amsdu = amsdu;
	    __entry->idx = idx;
	    __entry->first = first;
	    __entry->last = last;
	    __entry->fake_ack = fake_ack;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, filter: %d, seq: %d, qos: %d, "
		      "retry: %d, amsdu: %d, idx: %d(F: %d, L: %d), fake ack: %d",
		      __entry->inst, __entry->pid,
		      __entry->tid, __entry->filter,
		      __entry->seq, __entry->qos,
		      __entry->retry, __entry->amsdu,
		      __entry->idx, __entry->first,
		      __entry->last, __entry->fake_ack)
);

TRACE_EVENT(skw_rx_reorder,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 sn, u8 is_amsdu, u8 amsdu_idx,
		     u16 win_size, u16 win_start, u32 stored_num,
		     bool release, bool drop),
	    TP_ARGS(inst, pid, tid, sn, is_amsdu, amsdu_idx, win_size,
		    win_start, stored_num, release, drop),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, sn)
	    __field(u8, is_amsdu)
	    __field(u8, amsdu_idx)
	    __field(u16, win_size)
	    __field(u16, win_start)
	    __field(u32, stored_num)
	    __field(bool, release)
	    __field(bool, drop)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->sn = sn;
	    __entry->is_amsdu = is_amsdu;
	    __entry->amsdu_idx = amsdu_idx;
	    __entry->win_size = win_size;
	    __entry->win_start = win_start;
	    __entry->stored_num = stored_num;
	    __entry->release = release;
	    __entry->drop = drop;
	    ),

	    TP_printk("ssn: %d, sn: %d (%d, %d), release: %d, drop: %d, stored: %d, "
		      "I: %d, P: %d, T: %d, win_sz: %d, amsdu: %d, idx: %d",
		      __entry->win_start, __entry->sn,
		      __entry->win_start % __entry->win_size,
		      __entry->sn % __entry->win_size,
		      __entry->release, __entry->drop,
		      __entry->stored_num,
		      __entry->inst, __entry->pid,
		      __entry->tid, __entry->win_size,
		      __entry->is_amsdu, __entry->amsdu_idx)
);

TRACE_EVENT(skw_rx_reorder_release,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 win_start, u16 seq, u16 index, u16 ssn, u16 left),
	    TP_ARGS(inst, pid, tid, win_start, seq, index, ssn, left),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, win_start)
	    __field(u16, seq)
	    __field(u16, index)
	    __field(u16, ssn)
	    __field(u16, left)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->win_start = win_start;
	    __entry->seq = seq;
	    __entry->index = index;
	    __entry->ssn = ssn;
	    __entry->left = left;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, win start: %d, seq: %d (index: %d), ssn: %d, left: %d",
		      __entry->inst, __entry->pid, __entry->tid, __entry->win_start,
		      __entry->seq, __entry->index, __entry->ssn, __entry->left)
);

TRACE_EVENT(skw_rx_force_release,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 index, u16 ssn, u16 target, u16 left, int reason),
	    TP_ARGS(inst, pid, tid, index, ssn, target, left, reason),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, index)
	    __field(u16, ssn)
	    __field(u16, target)
	    __field(u16, left)
	    __field(int, reason)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->index = index;
	    __entry->ssn = ssn;
	    __entry->target = target;
	    __entry->left = left;
	    __entry->reason = reason;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, seq: %d(index: %d), target: %d, left: %d, reason: %d",
		    __entry->inst, __entry->pid, __entry->tid,
		    __entry->ssn, __entry->index, __entry->target,
		    __entry->left, __entry->reason)
);

TRACE_EVENT(skw_rx_handler_seq,
	    TP_PROTO(u16 sn, u8 msdu_filter),
	    TP_ARGS(sn, msdu_filter),

	    TP_STRUCT__entry(
	    __field(u16, sn)
	    __field(u8, msdu_filter)
	    ),

	    TP_fast_assign(
	    __entry->sn = sn;
	    __entry->msdu_filter = msdu_filter;
	    ),

	    TP_printk("seq: %d msdu_filter: %u",
		    __entry->sn, __entry->msdu_filter)
);

TRACE_EVENT(skw_rx_add_ba,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 ssn, u16 buf_size),
	    TP_ARGS(inst, pid, tid, ssn, buf_size),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, ssn)
	    __field(u16, buf_size)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->ssn = ssn;
	    __entry->buf_size = buf_size;
	    ),

	    TP_printk("I: %d, P: %d, T: %d,  ssn: %d, buf_size: %d",
		      __entry->inst, __entry->pid, __entry->tid,
		      __entry->ssn, __entry->buf_size)
);

TRACE_EVENT(skw_rx_update_ba,
	    TP_PROTO(u8 inst, u8 pid, u16 tid, u16 ssn),
	    TP_ARGS(inst, pid, tid, ssn),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, pid)
	    __field(u16, tid)
	    __field(u16, ssn)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->pid = pid;
	    __entry->tid = tid;
	    __entry->ssn = ssn;
	    ),

	    TP_printk("I: %d, P: %d, T: %d, ssn: %d",
		      __entry->inst, __entry->pid,
		      __entry->tid, __entry->ssn)
);

TRACE_EVENT(skw_rx_del_ba,
	    TP_PROTO(u16 tid),
	    TP_ARGS(tid),

	    TP_STRUCT__entry(
	    __field(u16, tid)
	    ),

	    TP_fast_assign(
	    __entry->tid = tid;
	    ),

	    TP_printk("del ba, tid: %d", __entry->tid)
);

TRACE_EVENT(skw_rx_irq,
	    TP_PROTO(int nents, int idx,  int port, int len),
	    TP_ARGS(nents, idx, port, len),

	    TP_STRUCT__entry(
	    __field(int, nents)
	    __field(int, idx)
	    __field(int, port)
	    __field(int, len)
	    ),

	    TP_fast_assign(
	    __entry->nents = nents;
	    __entry->idx = idx;
	    __entry->port = port;
	    __entry->len = len;
	    ),

	    TP_printk("nents: %d, idx: %d, port: %d, len: %d",
		      __entry->nents, __entry->idx,
		      __entry->port, __entry->len)
);

TRACE_EVENT(skw_msg_rx,
	    TP_PROTO(u8 inst, u8 type, u16 id, u16 seq, u16 len),
	    TP_ARGS(inst, type, id, seq, len),

	    TP_STRUCT__entry(
	    __field(u8, inst)
	    __field(u8, type)
	    __field(u16, id)
	    __field(u16, seq)
	    __field(u16, len)
	    ),

	    TP_fast_assign(
	    __entry->inst = inst;
	    __entry->type = type;
	    __entry->id = id;
	    __entry->seq = seq;
	    __entry->len = len;
	    ),

	    TP_printk("inst: %d, type: %d, id: %d, seq: %d, len: %d",
		      __entry->inst, __entry->type, __entry->id,
		      __entry->seq, __entry->len)
);

TRACE_EVENT(skw_hw_adma_tx_done,
	TP_PROTO(u8 num),
	TP_ARGS(num),

	TP_STRUCT__entry(__field(u8, num)),

	TP_fast_assign(__entry->num = num;),

	TP_printk("num:%d", __entry->num)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
