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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <linux/ctype.h>
#include <net/tcp.h>
#include <net/arp.h>
#include <linux/platform_device.h>
#include <linux/if_tunnel.h>
#include <linux/firmware.h>
#include <generated/utsrelease.h>
#include <linux/suspend.h>

#ifdef CONFIG_PLATFORM_ROCKCHIP
#include <linux/rfkill-wlan.h>
#endif

#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_tx.h"
#include "skw_rx.h"
#include "skw_iw.h"
#include "skw_timer.h"
#include "skw_work.h"
#include "version.h"
#include "skw_calib.h"
#include "skw_vendor.h"
#include "skw_regd.h"
#include "skw_recovery.h"
#include "skw_config.h"
#include "skw_core_dbg.h"
#include "trace.h"

struct skw_global_config {
	atomic_t index;
	struct skw_config config;
};

static u8 skw_mac[ETH_ALEN];
static struct skw_global_config g_skw_config;

static unsigned long skw_chip_idx = 0;

static const int g_skw_up_to_ac[8] = {
	SKW_WMM_AC_BE,
	SKW_WMM_AC_BK,
	SKW_WMM_AC_BK,
	SKW_WMM_AC_BE,
	SKW_WMM_AC_VI,
	SKW_WMM_AC_VI,
	SKW_WMM_AC_VO,
	SKW_WMM_AC_VO
};

static int skw_add_chip_idx(void)
{
	u8 index = 0;

#define SKW_MAX_CHIP_IDX 	32

	for(index = 0; index < SKW_MAX_CHIP_IDX; index++) {
		if (!test_and_set_bit(index,&skw_chip_idx)) {
			break;
		}
	}

	return (index >= SKW_MAX_CHIP_IDX) ? 0 : index + 1;
}

static void skw_dec_chip_idx(u8 idx)
{
	clear_bit(idx,&skw_chip_idx);

	return;
}

static int skw_ndo_open(struct net_device *dev)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "dev: %s, type: %s\n", netdev_name(dev),
		skw_iftype_name(dev->ieee80211_ptr->iftype));

	if (test_bit(SKW_FLAG_FW_THERMAL, &skw->flags)) {
		skw_chip_warn(skw->idx, "disable TX for thermal warnning");
		netif_tx_stop_all_queues(dev);
	}

	netif_carrier_off(dev);

	if (dev->ieee80211_ptr->iftype == NL80211_IFTYPE_MONITOR) {
		dev->type = ARPHRD_IEEE80211_RADIOTAP;
		netif_tx_stop_all_queues(dev);
	} else
		dev->type = ARPHRD_ETHER;

	return 0;
}

static int skw_ndo_stop(struct net_device *dev)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "dev: %s, type: %s\n", netdev_name(dev),
		skw_iftype_name(dev->ieee80211_ptr->iftype));

	//netif_tx_stop_all_queues(dev);

	// fixme:
	// check if sched scan is going on current netdev
	skw_scan_done(skw, iface, true);

	switch (dev->ieee80211_ptr->iftype) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		// if (iface->sta.sm.state !=  SKW_STATE_NONE)
			//skw_disconnect(iface->wdev.wiphy, iface->ndev,
			//		WLAN_REASON_DEAUTH_LEAVING);
		break;

	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		// skw_flush_sta_info(iface);
		break;

	case NL80211_IFTYPE_MONITOR:
		skw_cmd_monitor(priv_to_wiphy(skw), NULL, SKW_MONITOR_CLOSE);
		break;

	default:
		break;
	}

	return 0;
}

static bool skw_udp_filter(struct net_device *ndev, struct sk_buff *skb)
{
	bool ret = false;
	struct udphdr *udp = udp_hdr(skb);
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = iface->skw;

#define DHCP_SERVER_PORT         67
#define DHCP_CLIENT_PORT         68
#define DHCPV6_SERVER_PORT       546
#define DHCPV6_CLIENT_PORT       547

	switch (ntohs(udp->dest)) {
	case DHCP_CLIENT_PORT:
	case DHCP_SERVER_PORT:
		if (ndev->priv_flags & IFF_BRIDGE_PORT) {
			/* set BOOTP flag to broadcast */
			*((u8 *)udp + 18) = 0x80;
			udp->check = 0;
		}

		skw_fallthrough;

	case DHCPV6_CLIENT_PORT:
	case DHCPV6_SERVER_PORT:
		ret = true;
		skw_chip_dbg(skw->idx, "DHCP, port: %d\n", ntohs(udp->dest));
		break;

	default:
		ret = false;
		break;
	}

	return ret;
}

static void skw_setup_txba(struct skw_core *skw, struct skw_iface *iface,
			   struct skw_peer *peer, int tid)
{
	int ret = 0;
	struct skw_ba_action tx_ba = {0};

	if (tid >= SKW_NR_TID) {
		skw_chip_warn(skw->idx, "tid: %d invalid\n", tid);
		return;
	}

	if ((peer->txba.bitmap | peer->txba.blacklist) & BIT(tid))
		return;

	tx_ba.tid = tid;
	tx_ba.win_size = 64;
	tx_ba.lmac_id = iface->lmac_id;
	tx_ba.peer_idx = peer->idx;
	tx_ba.action = SKW_ADD_TX_BA;

	ret = skw_queue_work(priv_to_wiphy(skw), iface, SKW_WORK_SETUP_TXBA,
				&tx_ba, sizeof(tx_ba));
	if (!ret)
		peer->txba.bitmap |= BIT(tid);
}

struct skw_ctx_entry *skw_get_ctx_entry(struct skw_core *skw, const u8 *addr)
{
	int i, j;
	struct skw_ctx_entry *entry;

	for (i = 0; i < skw->hw.nr_lmac; i++) {
		for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
			entry = rcu_dereference(skw->hw.lmac[i].peer_ctx[j].entry);
			if (entry && ether_addr_equal(entry->addr, addr))
				return entry;
		}
	}

	return NULL;
}

struct skw_peer_ctx *skw_get_ctx(struct skw_core *skw, u8 lmac_id, u8 idx)
{
	if (idx >= SKW_MAX_PEER_SUPPORT)
		return NULL;

	return &skw->hw.lmac[lmac_id].peer_ctx[idx];
}

static int skw_downgrade_ac(struct skw_iface *iface, int aci)
{
#if 1
	while (iface->wmm.acm & BIT(aci)) {
		if (aci == SKW_WMM_AC_BK)
			break;
		aci++;
	}
#else
	if (iface->wmm.acm & BIT(aci)) {
		int i;
		int acm = SKW_WMM_AC_BK;

		for (i = SKW_WMM_AC_BK; i >= 0; i--) {
			if (!(iface->wmm.acm & BIT(i))) {
				acm = i;
				break;
			}
		}

		aci = acm;
	}
#endif

	return aci;
}

void skw_udp_special_hdl(struct skw_core *skw, struct sk_buff *skb)
{
	const struct sock *sk = skb->sk;

	if (sk)
		skb_orphan(skb);
}

void skw_tcp_special_hdl(struct skw_core *skw, struct sk_buff *skb)
{
	const struct sock *sk = skb->sk;

	if (sk) {
		if (sk->sk_state == TCP_ESTABLISHED) {
			const struct tcp_sock *tp;

			tp = tcp_sk(sk);
			if (tp) {
				if (tp->snd_cwnd < 500)  //700Kbye cwnd
					skb_orphan(skb);
			}
		}
	}
}

static inline bool is_skw_tcp_pure_ack(struct sk_buff *skb)
{
	return tcp_hdr(skb)->ack &&
	       ntohs(ip_hdr(skb)->tot_len) == ip_hdrlen(skb) + tcp_hdrlen(skb);
}

static netdev_tx_t skw_ndo_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	int ret = -1;
	u8 tid;
	u8 fixed_rate = 0;
	u8 fixed_tid = SKW_INVALID_ID;
	u8 peer_index = SKW_INVALID_ID;
	u8 ac_idx = 0, padding = 0;
	u8 prot = 0, tcp_pkt = 0, do_csum = 0;
	u32 filter_map = 0, data_prot_map = 0;
	bool is_prot_filter = false;
	bool is_udp_filter = false;
	bool is_802_3_frame = false;
	bool is_tx_filter = false;
	bool pure_tcp_ack = false;
	bool need_desc_hdr = false;
	const u8 tid_map[] = {6, 4, 0, 1};
	int l4_hdr_offset = 0, reset_l4_offset = 0;
	int msdu_len, txq_len;
	int nhead, ntail, hdr_size;
	bool is_completed = true;

	struct netdev_queue *txq;
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface = netdev_priv(ndev);
	struct ethhdr *eth = eth_hdr(skb);
	struct skw_ctx_entry *entry = NULL;
	struct sk_buff_head *qlist;
	struct skw_tx_desc_hdr *desc_hdr;
	struct skw_tx_desc_conf *desc_conf;
	struct skw_core *skw = iface->skw;
#ifdef CONFIG_SWT6621S_SKB_RECYCLE
	struct sk_buff *skb_recycle;
#endif
	s16 pkt_limit;
	struct sk_buff *pre_skb = NULL;

	__net_timestamp(skb);

	/* Mini frame size that HW support */
	if (unlikely(skb->len <= 16)) {
		skw_chip_dbg(skw->idx, "current: %s\n", current->comm);
		skw_hex_dump("short skb", skb->data, skb->len, true);

		if (skb->len != ETH_HLEN || eth->h_proto != htons(ETH_P_IP))
			SKW_BUG_ON(1);

		goto free;
	}

	if (skb_linearize(skb))
		goto free;

	msdu_len = skb->len;
	SKW_SKB_TXCB(skb)->skb_native_len = skb->len;
	SKW_SKB_TXCB(skb)->ret = 0;
	SKW_SKB_TXCB(skb)->recycle = 0;
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		l4_hdr_offset = skb_checksum_start_offset(skb);
		do_csum = 1;
	}

	switch (eth->h_proto) {
	case htons(ETH_P_IP):
		prot = ip_hdr(skb)->protocol;
		reset_l4_offset = ETH_HLEN + ip_hdrlen(skb);

		if (prot == IPPROTO_TCP) {
			pure_tcp_ack = is_skw_tcp_pure_ack(skb);
			skw_tcp_special_hdl(skw, skb);
		}

		break;

	case htons(ETH_P_IPV6):
		prot = ipv6_hdr(skb)->nexthdr;
		// fixme:
		// get tcp/udp head offset
		reset_l4_offset = ETH_HLEN + sizeof(struct ipv6hdr);
		break;

	case htons(ETH_P_ARP):
		if (test_bit(SKW_FLAG_FW_FILTER_ARP, &skw->flags))
			is_prot_filter = true;

		if (unlikely(test_bit(SKW_FLAG_REPEATER, &skw->flags)) &&
		    ndev->priv_flags & IFF_BRIDGE_PORT) {
			if (iface->wdev.iftype == NL80211_IFTYPE_STATION) {
				struct sk_buff *arp_skb;
				struct skw_ctx_entry *e;
				struct skw_arphdr *arp = skw_arp_hdr(skb);

				rcu_read_lock();
				e = skw_get_ctx_entry(iface->skw, arp->ar_sha);
				if (e)
					e->peer->ip_addr = arp->ar_sip;

				rcu_read_unlock();

				arp_skb = arp_create(ntohs(arp->ar_op),
						ETH_P_ARP, arp->ar_tip,
						iface->ndev,
						arp->ar_sip, eth->h_dest,
						iface->addr, arp->ar_tha);

				kfree_skb(skb);

				skb = arp_skb;
				if (!skb)
					return NETDEV_TX_OK;

				eth = skw_eth_hdr(skb);
			}
		}

		if (is_skw_sta_mode(iface))
			fixed_rate = 1;
		//fixed_tid = 4;
		break;

	case htons(ETH_P_PAE):
		data_prot_map |= (1 << SKW_MSDU_FILTER_EAP);
		is_prot_filter = true;
		break;

	case htons(SKW_ETH_P_WAPI):
		is_prot_filter = true;
		data_prot_map |= (1 << SKW_MSDU_FILTER_WAPI);
		break;

	case htons(ETH_P_TDLS):
		is_prot_filter = true;
		break;

	default:
		if (eth->h_proto < ETH_P_802_3_MIN)
			is_802_3_frame = true;

		break;
	}

	if (!do_csum && (prot == IPPROTO_UDP || prot == IPPROTO_TCP))
		skb_set_transport_header(skb, reset_l4_offset);

	// fixme:
	/* enable checksum for TCP & UDP frame, except framgment frame */
	switch (prot) {
	case IPPROTO_UDP:
		skw_udp_special_hdl(skw, skb);
		is_udp_filter = skw_udp_filter(ndev, skb);
		if (udp_hdr(skb)->check == 0)
			do_csum = 0;

		break;

	case IPPROTO_TCP:
		tcp_pkt = 1;

		break;

	default:
		break;
	}

	skw_tx_dump_tcp_data(iface, skb);

	ac_idx = skw_downgrade_ac(iface, g_skw_up_to_ac[skb->priority]);
	tid = (fixed_tid != SKW_INVALID_ID) ? fixed_tid : tid_map[ac_idx];

	rcu_read_lock();

	switch (iface->wdev.iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		if (is_unicast_ether_addr(eth->h_dest)) {
			entry = skw_get_ctx_entry(skw, eth->h_dest);
			peer_index = entry ? entry->idx : SKW_INVALID_ID;

			if (entry && entry->peer->sm.state != SKW_STATE_COMPLETED)
				is_completed = false;
		} else {
			fixed_rate = 1;
			peer_index = iface->default_multicast;
		}

		break;

	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_ADHOC:

		if (atomic_read(&iface->actived_ctx) > 1)
			entry = skw_get_ctx_entry(skw, eth->h_dest);

		if (!entry) {
			ctx = skw_get_ctx(skw, iface->lmac_id, iface->sta.core.bss.ctx_idx);
			if (ctx)
				entry = rcu_dereference(ctx->entry);
		}

		peer_index = entry ? entry->idx : SKW_INVALID_ID;

		if (peer_index != SKW_INVALID_ID &&
			is_multicast_ether_addr(eth->h_dest)) {
			peer_index = iface->default_multicast;

			if (iface->sta.core.sm.state != SKW_STATE_COMPLETED) {

				rcu_read_unlock();

				skw_chip_dbg(skw->idx, "%s drop dst: %pm, proto: 0x%x, state: %s\n",
					netdev_name(ndev), eth->h_dest, htons(eth->h_proto),
					skw_state_name(iface->sta.core.sm.state));

				goto free;
			}
		}

		if (iface->sta.core.sm.state != SKW_STATE_COMPLETED)
			is_completed = false;

		break;

	default:
		peer_index = SKW_INVALID_ID;
		break;
	}

	if (entry) {
		if(is_completed)
			skw_setup_txba(skw, iface, entry->peer, tid);

		if (entry->peer) {
			filter_map = atomic_read(&entry->peer->rx_filter);

			if (filter_map && !(filter_map & data_prot_map)) {

				rcu_read_unlock();

				skw_chip_dbg(skw->idx, "%s drop dst: %pm, proto: 0x%x, filter: 0x%x\n",
					netdev_name(ndev), eth->h_dest,
					htons(eth->h_proto), filter_map);

				goto free;
			}
		}
	}

	rcu_read_unlock();

	if (peer_index == SKW_INVALID_ID) {
		skw_chip_dbg(skw->idx, "%s drop dst: %pM, proto: 0x%x\n",
			netdev_name(ndev), eth->h_dest, htons(eth->h_proto));

		goto free;
	}

	is_tx_filter = (is_prot_filter || is_udp_filter || is_802_3_frame);

#ifdef CONFIG_SWT6621S_SKB_RECYCLE
	if (!is_prot_filter && !is_udp_filter && !is_802_3_frame && !is_multicast_ether_addr(eth->h_dest)) {
		skb_recycle = skw_recycle_skb_get(skw);
		if (skb_recycle) {
			if (!skw_recycle_skb_copy(skw, skb_recycle, skb)) {
				dev_kfree_skb_any(skb);
				skb = skb_recycle;
				eth = skw_eth_hdr(skb);
				SKW_SKB_TXCB(skb)->recycle = 1;
			} else {
				skw_chip_err(skw->idx, "skw_recycle_skb_copy failed\n");
				skw_recycle_skb_free(skw, skb_recycle);
			}
		} else {
			skb_recycle = skb_copy(skb, GFP_ATOMIC);
			if (skb_recycle) {
				dev_kfree_skb_any(skb);
				skb = skb_recycle;
				eth = skw_eth_hdr(skb);
			}
		}
	}
#endif

#define SKW_EXPAND_SIZE(x, y)  ((x) > (y) ? (x) - (y) : 0)
	hdr_size = sizeof(struct skw_tx_desc_conf);

	if (skw->hw.bus == SKW_BUS_PCIE) {
		SKW_SKB_TXCB(skb)->lmac_id = iface->lmac_id;
		SKW_SKB_TXCB(skb)->e.eth_type = eth->h_proto;
		SKW_SKB_TXCB(skb)->e.mac_id = iface->id;
		SKW_SKB_TXCB(skb)->e.tid = tid;
		SKW_SKB_TXCB(skb)->e.peer_idx = peer_index;
		SKW_SKB_TXCB(skb)->e.prot = SKW_ETHER_FRAME;
		SKW_SKB_TXCB(skb)->e.encry_dis = 0;
		SKW_SKB_TXCB(skb)->e.rate = fixed_rate;
		SKW_SKB_TXCB(skb)->e.msdu_len = msdu_len;

		padding = 0;
		ntail = 0;

		if (is_tx_filter) {
			need_desc_hdr = true;
			hdr_size += sizeof(struct skw_tx_desc_hdr);
		}

		nhead = SKW_EXPAND_SIZE(hdr_size, skb_headroom(skb));
	} else {
		int total_len;

		need_desc_hdr = true;

		hdr_size += sizeof(struct skw_tx_desc_hdr) + SKW_EXTER_HDR_SIZE;

		if (skw->config->global.dma_addr_align > 4) {
			unsigned char *ptr = NULL;
			struct sk_buff *pskb = NULL;

			total_len = skb->len + hdr_size + skw->config->global.dma_addr_align;
			pskb = netdev_alloc_skb(skb->dev, ALIGN(total_len, skw->hw.align));
			if (!pskb)
				goto free;

			ptr = PTR_ALIGN(pskb->data, skw->config->global.dma_addr_align);
			skb_reserve(pskb, ptr - pskb->data + hdr_size);
			skw_put_skb_data(pskb, skb->data, skb->len);
			eth = skw_eth_hdr(pskb);

			if(is_tx_filter) {
				pre_skb = skb;
			} else {
				dev_kfree_skb_any(skb);
			}

			skb = pskb;

			nhead = ntail = 0;
		} else {
			padding = ((long)(skb->data + hdr_size)) & SKW_DATA_ALIGN_MASK;
			nhead = SKW_EXPAND_SIZE(hdr_size + padding, skb_headroom(skb));

			total_len = hdr_size + skb->len + padding;
			ntail = ALIGN(total_len, skw->hw.align) - total_len;
			ntail = SKW_EXPAND_SIZE(ntail, skb_tailroom(skb));
		}
	}
#undef SKW_EXPAND_SIZE

	if (nhead || ntail || skb_cloned(skb)) {
		if (unlikely(pskb_expand_head(skb, nhead, ntail, GFP_ATOMIC))) {
			skw_chip_dbg(skw->idx, "failed, nhead: %d, ntail: %d\n", nhead, ntail);
			goto free;
		}

		eth = skw_eth_hdr(skb);
	}

	desc_conf = (void *)skb_push(skb, sizeof(*desc_conf));
	desc_conf->csum = do_csum;
	desc_conf->ip_prot = tcp_pkt;
	desc_conf->l4_hdr_offset = l4_hdr_offset;

	if (padding)
		skb_push(skb, padding);

	if (need_desc_hdr) {
		desc_hdr = (void *)skb_push(skb, sizeof(*desc_hdr));

		desc_hdr->padding_gap = padding;
		desc_hdr->inst = iface->id & 0x3;
		desc_hdr->lmac_id = iface->lmac_id;
		desc_hdr->tid = tid;
		desc_hdr->peer_lut = peer_index;
		desc_hdr->frame_type = SKW_ETHER_FRAME;
		desc_hdr->encry_dis = 0;
		desc_hdr->msdu_len = msdu_len;
		desc_hdr->rate = fixed_rate;

		skw_set_tx_desc_eth_type(desc_hdr, eth->h_proto);
	}

	if (unlikely(is_tx_filter)) {
		skw_chip_dbg(skw->idx, "proto: 0x%x, udp filter: %d, 802.3 frame: %d\n",
			htons(eth->h_proto), is_udp_filter, is_802_3_frame);

		ret = skw_msg_try_send(skw, iface->id, SKW_CMD_TX_DATA_FRAME,
				       skb->data, skb->len, NULL, 0,
				       "SKW_CMD_TX_DATA_FRAME");
		if (ret < 0) {
			if (SKW_SKB_TXCB(skb)->tx_retry++ > 3) {
				skw_queue_work(priv_to_wiphy(skw), iface,
					       SKW_WORK_TX_ETHER_DATA,
					       skb->data, skb->len);
			} else {

				if (pre_skb && skb) {
					dev_kfree_skb_any(skb);
					skb = NULL;
				} else {
					skb_pull(skb, skb->len - msdu_len);
				}
				return NETDEV_TX_BUSY;
			}
		}

		goto free;
	}

	SKW_SKB_TXCB(skb)->ret = 0;
	SKW_SKB_TXCB(skb)->peer_idx = peer_index;

	if (pure_tcp_ack)
		ac_idx = SKW_ACK_TXQ;

	qlist = &iface->txq[ac_idx];

	skb_queue_tail(qlist, skb);

	if (skw->hw.bus == SKW_BUS_PCIE)
		pkt_limit = TX_BUF_ADDR_CNT / 2;
	else
		pkt_limit = 15;

	if (skw_get_hw_credit(skw, iface->lmac_id) == 0)
		goto _ok;

	if (prot == IPPROTO_UDP || prot == IPPROTO_TCP)
		pkt_limit = 15;
	else
		pkt_limit = 0;

	if (pkt_limit) {
		if (skb_queue_len(qlist) < pkt_limit) {
			if (!timer_pending(&skw->tx_worker.timer))
				skw_wakeup_tx(skw, msecs_to_jiffies(1));
		} else
			skw_wakeup_tx(skw, 0);
	} else
		skw_wakeup_tx(skw, 0);

_ok:
	trace_skw_tx_xmit(eth->h_dest, peer_index, prot, fixed_rate,
			  do_csum, ac_idx, skb_queue_len(qlist));

	txq_len = skb_queue_len(qlist) + skb_queue_len(&iface->tx_cache[ac_idx]);
	if (txq_len >= SKW_TXQ_HIGH_THRESHOLD) {
		txq = netdev_get_tx_queue(ndev, ac_idx);
		if (!netif_tx_queue_stopped(txq))
			netif_tx_stop_queue(txq);
	}

	if (pre_skb) {
		dev_kfree_skb_any(pre_skb);
		pre_skb = NULL;
	}
	return NETDEV_TX_OK;

free:
	if (ret != 0)
		ndev->stats.tx_dropped++;

	dev_kfree_skb_any(skb);

	if (pre_skb) {
		dev_kfree_skb_any(pre_skb);
		pre_skb = NULL;
	}
	return NETDEV_TX_OK;
}

static u16 skw_ndo_select(struct net_device *dev, struct sk_buff *skb
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
		, struct net_device *sb_dev
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
		, struct net_device *sb_dev,
		select_queue_fallback_t fallback
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		, void *accel_priv,
		select_queue_fallback_t fallback
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
		, void *accel_priv
#endif
		SKW_NULL)
{
	struct skw_iface *iface = netdev_priv(dev);

	if (!iface->wmm.qos_enabled) {
		skb->priority = 0;
		return SKW_WMM_AC_BE;
	}

	if(!skb->protocol && skb->len > ETH_HLEN) {
		skb->protocol = eth_type_trans(skb, iface->ndev);
		skb_push(skb, ETH_HLEN);
	}

	skb->priority = skw_compat_classify8021d(skb, iface->qos_map);

	return g_skw_up_to_ac[skb->priority];
}

static int skw_set_android_suspend(struct wiphy *wiphy, struct net_device *dev,
				char *suspend)
{
	int ret = 0;
	long val = 0;
	char *endp = NULL;

	skw_dbg("data: %s\n", suspend);

	val = simple_strtol(suspend, &endp, 0);
	if (*endp != '\0' || (val != 0 && val != 1)) {
		skw_err("endp: %d, val: %ld\n", *endp, val);

		return -EINVAL;
	}

#ifdef CONFIG_SWT6621S_PRESUSPEND_SUPPORT
	ret = skw_set_mib_bool(wiphy, SKW_NDEV_ID(dev),
			SKW_MIB_SET_SUSPEND_MODE, !!val);
#endif

	return ret;
}

#define SKW_ANDROID_CMD_LEN    128
static int skw_android_cmd(struct net_device *dev, void __user *priv_data)
{
	int ret = 0;
	char command[SKW_ANDROID_CMD_LEN], *data;
	struct android_wifi_priv_cmd priv_cmd;
	bool compat_task = false;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	if (!priv_data)
		return  -EINVAL;

#ifdef CONFIG_COMPAT
	if (SKW_IS_COMPAT_TASK()) {
		struct compat_android_wifi_priv_cmd compat;

		if (copy_from_user(&compat, priv_data, sizeof(compat)))
			return -EFAULT;

		priv_cmd.buf = compat_ptr(compat.buf);
		priv_cmd.used_len = compat.used_len;
		priv_cmd.total_len = compat.total_len;

		compat_task = true;
	}
#endif

	if (!compat_task &&
	    copy_from_user(&priv_cmd, priv_data, sizeof(priv_cmd)))
		return -EFAULT;

	if (copy_from_user(command, priv_cmd.buf, sizeof(command)))
		return -EFAULT;

	command[SKW_ANDROID_CMD_LEN - 1] = 0;

	skw_chip_dbg(skw->idx, "%s: %s\n", netdev_name(dev), command);

#define IS_SKW_CMD(c, k)        \
	!strncasecmp(c, SKW_ANDROID_PRIV_##k, strlen(SKW_ANDROID_PRIV_##k))

#define SKW_CMD_DATA(c, k)     \
	(c + strlen(SKW_ANDROID_PRIV_##k) + 1)

#define SKW_CMD_DATA_LEN(c, k) \
	(sizeof(command) - strlen(SKW_ANDROID_PRIV_##k) - 1)

	if (IS_SKW_CMD(command, COUNTRY)) {
		data = SKW_CMD_DATA(command, COUNTRY);
		ret = skw_set_regdom(dev->ieee80211_ptr->wiphy, data);
		if (ret)
			skw_chip_err(skw->idx, "skw_set_regdom failed, ret: %d\n", ret);
	} else if (IS_SKW_CMD(command, BTCOEXSCAN_STOP)) {
	} else if (IS_SKW_CMD(command, RXFILTER_START)) {
	} else if (IS_SKW_CMD(command, RXFILTER_STOP)) {
	} else if (IS_SKW_CMD(command, RXFILTER_ADD)) {
	} else if (IS_SKW_CMD(command, RXFILTER_REMOVE)) {
	} else if (IS_SKW_CMD(command, SETSUSPENDMODE)) {
		data = SKW_CMD_DATA(command, SETSUSPENDMODE);
		ret = skw_set_android_suspend(dev->ieee80211_ptr->wiphy, dev, data);
		if (ret)
			skw_err("failed, command: %s, ret: %d\n", command, ret);
	} else if (IS_SKW_CMD(command, BTCOEXMODE)) {
	} else if (IS_SKW_CMD(command, SET_AP_WPS_P2P_IE)) {
	} else {
		skw_chip_info(skw->idx, "Unsupport cmd: %s - ignored\n", command);
	}

#undef IS_SKW_CMD

	return 0;
}

static int skw_ioctl(struct net_device *dev, void __user *user_data)
{
	int ret = -ENOTSUPP;
	char country[4] = {0};
	struct skw_ioctl_cmd cmd;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	if (copy_from_user(&cmd, user_data, sizeof(cmd)))
		return -ENOMEM;

	switch (cmd.id) {
	case SKW_IOCTL_SUBCMD_COUNTRY:

		if (cmd.len > sizeof(country))
			return -EINVAL;

		if (copy_from_user(country, user_data + sizeof(cmd), cmd.len))
			return -EINVAL;

		if (strlen(country) != 2)
			return -EINVAL;

		ret = skw_set_regdom(dev->ieee80211_ptr->wiphy, country);
		if (ret)
			skw_chip_err(skw->idx, "set regdom failed, ret: %d\n", ret);

		break;

	default:
		skw_chip_warn(skw->idx, "unsupport cmd: 0x%x, len: %d\n", cmd.id, cmd.len);
		break;
	}

	return ret;
}

static int skw_ndo_ioctl(struct net_device *dev, struct ifreq *ifr,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
			void __user *data,
#endif
			int cmd)
{
	int ret;
	void __user *priv = ifr->ifr_data;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	priv = data;
#endif

	switch (cmd) {
	case SKW_IOCTL_ANDROID_CMD:
		ret = skw_android_cmd(dev, priv);
		break;

	case SKW_IOCTL_CMD:
		ret = skw_ioctl(dev, priv);
		break;

	default:
		ret = -ENOTSUPP;
		skw_chip_warn(skw->idx, "%s, unsupport cmd: 0x%x\n", netdev_name(dev), cmd);

		break;
	}

	return ret;
}

static void skw_ndo_tx_timeout(struct net_device *dev
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
				, unsigned int txqueue
#endif
				SKW_NULL)
{
	struct skw_iface *iface = (struct skw_iface *)netdev_priv(dev);
	struct skw_core *skw;

	if (!iface) {
		skw_err("iface NULL\n");
		return;
	}

	skw = iface->skw;

	skw_chip_warn(skw->idx, "ndev:%s,flag:0x%lx\n", netdev_name(dev), skw->flags);
}

static void skw_ndo_set_rx_mode(struct net_device *dev)
{
	int count, total_len;
	struct skw_mc_list *mc;
	struct netdev_hw_addr *ha;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "%s, mc: %d, uc: %d\n", netdev_name(dev),
		netdev_mc_count(dev), netdev_uc_count(dev));

	count = netdev_mc_count(dev);
	if (!count)
		return;

	total_len = sizeof(*mc) + sizeof(struct mac_address) * count;
	mc = SKW_ZALLOC(total_len, GFP_ATOMIC);
	if (!mc) {
		skw_chip_err(skw->idx, "alloc failed, mc count: %d, total_len: %d\n",
			count, total_len);
		return;
	}

	mc->count = count;

	count = 0;
	netdev_for_each_mc_addr(ha, dev)
		skw_ether_copy(mc->mac[count++].addr, ha->addr);

	skw_queue_work(dev->ieee80211_ptr->wiphy, netdev_priv(dev),
		SKW_WORK_SET_MC_ADDR, mc, total_len);

	SKW_KFREE(mc);
}
#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
static struct rtnl_link_stats64 *
#else
static void
#endif
skw_ndo_get_stats64(struct net_device *dev,
		    struct rtnl_link_stats64 *stats)
{
	int i;

	for_each_possible_cpu(i) {
		const struct pcpu_sw_netstats *tstats;
		u64 rx_packets, rx_bytes, tx_packets, tx_bytes;
		unsigned int start;

		tstats = per_cpu_ptr(dev->tstats, i);

		do {
			start = u64_stats_fetch_begin_irq(&tstats->syncp);
			rx_packets = tstats->rx_packets;
			tx_packets = tstats->tx_packets;
			rx_bytes = tstats->rx_bytes;
			tx_bytes = tstats->tx_bytes;
		} while (u64_stats_fetch_retry_irq(&tstats->syncp, start));

		stats->rx_packets += rx_packets;
		stats->tx_packets += tx_packets;
		stats->rx_bytes   += rx_bytes;
		stats->tx_bytes   += tx_bytes;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	return stats;
#endif
}
#endif

static int skw_ndo_set_mac_address(struct net_device *dev, void *addr)
{
	struct skw_iface *iface = (struct skw_iface *)netdev_priv(dev);
	struct sockaddr *sa = addr;
	int ret = 0, err = 0;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "mac: %pM\n", sa->sa_data);

	ret = eth_mac_addr(dev, sa);
	if (ret) {
		skw_chip_err(skw->idx, "failed, addr: %pM, ret: %d\n", sa->sa_data, ret);
		return ret;
	}

	ret = skw_send_msg(iface->wdev.wiphy, dev, SKW_CMD_RANDOM_MAC,
			sa->sa_data, ETH_ALEN, NULL, 0);
	if (ret) {
		skw_chip_err(skw->idx, "set mac: %pM failed, ret: %d\n",
			sa->sa_data, ret);

		err = eth_mac_addr(dev, iface->addr);
		if (err)
			skw_chip_warn(skw->idx, "set eth macaddr failed\n");

		return ret;
	}

	skw_ether_copy(iface->addr, sa->sa_data);

	return 0;
}

#if 0
static void skw_ndo_uninit(struct net_device *ndev)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct wiphy *wiphy = ndev->ieee80211_ptr->wiphy;

	skw_dbg("%s\n", netdev_name(ndev));

	free_percpu(ndev->tstats);

	skw_del_vif(wiphy, iface);
	skw_iface_teardown(wiphy, iface);
	skw_release_inst(wiphy, iface->id);
}
#endif

static const struct net_device_ops skw_netdev_ops = {
	// .ndo_uninit = skw_ndo_uninit,
	.ndo_open = skw_ndo_open,
	.ndo_stop = skw_ndo_stop,
	.ndo_start_xmit = skw_ndo_xmit,
	.ndo_select_queue = skw_ndo_select,
	.ndo_tx_timeout = skw_ndo_tx_timeout,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	.ndo_siocdevprivate = skw_ndo_ioctl,
#else
	.ndo_do_ioctl = skw_ndo_ioctl,
#endif
	//.ndo_get_stats64 = skw_ndo_get_stats64,
	.ndo_set_rx_mode = skw_ndo_set_rx_mode,
	.ndo_set_mac_address = skw_ndo_set_mac_address,
};

int skw_netdev_init(struct wiphy *wiphy, struct net_device *ndev, u8 *addr)
{
	struct skw_core *skw;
	struct skw_iface *iface;

	if (!ndev)
		return -EINVAL;

	skw = wiphy_priv(wiphy);
	iface = netdev_priv(ndev);
	SET_NETDEV_DEV(ndev, wiphy_dev(wiphy));

	ndev->features = NETIF_F_GRO |
			 NETIF_F_IP_CSUM |
			 NETIF_F_IPV6_CSUM;

	ndev->ieee80211_ptr = &iface->wdev;
	ndev->netdev_ops = &skw_netdev_ops;
	ndev->watchdog_timeo = 3 * HZ;
	ndev->needed_headroom =	skw->skb_headroom;
	ndev->needed_tailroom = skw->hw_pdata->align_value;
	// ndev->priv_destructor = skw_netdev_deinit;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	ndev->destructor = free_netdev;
#else
	ndev->needs_free_netdev = true;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	eth_hw_addr_set(ndev, addr);
#else
	skw_ether_copy(ndev->dev_addr, addr);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	ndev->tstats = netdev_alloc_pcpu_stats(struct pcpu_tstats);
#else
	ndev->tstats = netdev_alloc_pcpu_stats(struct pcpu_sw_netstats);
#endif
	if (!ndev->tstats)
		return -ENOMEM;

#ifdef CONFIG_WIRELESS_EXT
	ndev->wireless_handlers = skw_iw_handlers();
#endif

#ifdef CONFIG_RPS
	iface->cpu_id = skw->isr_cpu_id;
	skw_init_rps_map(iface);
#endif

	return 0;
}

void skw_netdev_deinit(struct net_device *ndev)
{
	free_percpu(ndev->tstats);
}

#if 0
void skw_netdev_deinit(struct net_device *dev)
{
	struct skw_iface *iface = netdev_priv(dev);

	skw_dbg("%s\n", netdev_name(dev));

#if 0 //move these actions to ndo_uninit
	free_percpu(dev->tstats);
	skw_cmd_close_dev(iface->wdev.wiphy, iface->id);
	skw_del_vif(iface->skw, iface);
	skw_iface_teardown(iface);
#endif
}
#endif

int skw_sync_cmd_event_version(struct wiphy *wiphy)
{
	int i, ret = 0;
	struct skw_version_info *skw_fw_ver;
	struct skw_core *skw = wiphy_priv(wiphy);

#define SKW_CMD_VER(id, ver)   .cmd[id] = ver
#define SKW_EVENT_VER(id, ver) .event[id] = ver
	const struct skw_version_info skw_drv_ver = {
		SKW_CMD_VER(SKW_CMD_DOWNLOAD_INI, V1),
		SKW_CMD_VER(SKW_CMD_GET_INFO, V1),
		SKW_CMD_VER(SKW_CMD_SYN_VERSION, V1),
		SKW_CMD_VER(SKW_CMD_OPEN_DEV, V1),
		SKW_CMD_VER(SKW_CMD_CLOSE_DEV, V1),
		SKW_CMD_VER(SKW_CMD_START_SCAN, V1),
		SKW_CMD_VER(SKW_CMD_STOP_SCAN, V1),
		SKW_CMD_VER(SKW_CMD_START_SCHED_SCAN, V1),
		SKW_CMD_VER(SKW_CMD_STOP_SCHED_SCAN, V1),
		SKW_CMD_VER(SKW_CMD_JOIN, V1),
		SKW_CMD_VER(SKW_CMD_AUTH, V1),
		SKW_CMD_VER(SKW_CMD_ASSOC, V1),
		SKW_CMD_VER(SKW_CMD_ADD_KEY, V1),
		SKW_CMD_VER(SKW_CMD_DEL_KEY, V1),
		SKW_CMD_VER(SKW_CMD_TX_MGMT, V1),
		SKW_CMD_VER(SKW_CMD_TX_DATA_FRAME, V1),
		SKW_CMD_VER(SKW_CMD_SET_IP, V1),
		SKW_CMD_VER(SKW_CMD_DISCONNECT, V1),
		SKW_CMD_VER(SKW_CMD_RPM_REQ, V1),
		SKW_CMD_VER(SKW_CMD_START_AP, V1),
		SKW_CMD_VER(SKW_CMD_STOP_AP, V1),
		SKW_CMD_VER(SKW_CMD_ADD_STA, V1),
		SKW_CMD_VER(SKW_CMD_DEL_STA, V1),
		SKW_CMD_VER(SKW_CMD_GET_STA, V1),
		SKW_CMD_VER(SKW_CMD_RANDOM_MAC, V1),
		SKW_CMD_VER(SKW_CMD_GET_LLSTAT, V1),
		SKW_CMD_VER(SKW_CMD_SET_MC_ADDR, V1),
		SKW_CMD_VER(SKW_CMD_RESUME, V1),
		SKW_CMD_VER(SKW_CMD_SUSPEND, V1),
		SKW_CMD_VER(SKW_CMD_REMAIN_ON_CHANNEL, V1),
		SKW_CMD_VER(SKW_CMD_BA_ACTION, V1),
		SKW_CMD_VER(SKW_CMD_TDLS_MGMT, V1),
		SKW_CMD_VER(SKW_CMD_TDLS_OPER, V1),
		SKW_CMD_VER(SKW_CMD_TDLS_CHANNEL_SWITCH, V1),
		SKW_CMD_VER(SKW_CMD_SET_CQM_RSSI, V1),
		SKW_CMD_VER(SKW_CMD_NPI_MSG, V1),
		SKW_CMD_VER(SKW_CMD_IBSS_JOIN, V1),
		SKW_CMD_VER(SKW_CMD_SET_IBSS_ATTR, V1),
		SKW_CMD_VER(SKW_CMD_RSSI_MONITOR, V1),
		SKW_CMD_VER(SKW_CMD_SET_IE, V1),
		SKW_CMD_VER(SKW_CMD_SET_MIB, V1),
		SKW_CMD_VER(SKW_CMD_REGISTER_FRAME, V1),
		SKW_CMD_VER(SKW_CMD_ADD_TX_TS, V1),
		SKW_CMD_VER(SKW_CMD_DEL_TX_TS, V1),
		SKW_CMD_VER(SKW_CMD_REQ_CHAN_SWITCH, V1),
		SKW_CMD_VER(SKW_CMD_CHANGE_BEACON, V1),
		SKW_CMD_VER(SKW_CMD_DPD_ILC_GEAR_PARAM, V1),
		SKW_CMD_VER(SKW_CMD_DPD_ILC_MARTIX_PARAM, V1),
		SKW_CMD_VER(SKW_CMD_DPD_ILC_COEFF_PARAM, V1),
		SKW_CMD_VER(SKW_CMD_WIFI_RECOVER, V1),
		SKW_CMD_VER(SKW_CMD_PHY_BB_CFG, V1),
		SKW_CMD_VER(SKW_CMD_SET_REGD, V1),
		SKW_CMD_VER(SKW_CMD_SET_EFUSE, V1),
		SKW_CMD_VER(SKW_CMD_SET_PROBEREQ_FILTER, V1),
		SKW_CMD_VER(SKW_CMD_CFG_ANT, V1),
		SKW_CMD_VER(SKW_CMD_RTT, V1),
		SKW_CMD_VER(SKW_CMD_GSCAN, V1),
		SKW_CMD_VER(SKW_CMD_DFS, V1),
		SKW_CMD_VER(SKW_CMD_SET_SPD_ACTION, V1),
		SKW_CMD_VER(SKW_CMD_SET_DPD_RESULT, V1),
		SKW_CMD_VER(SKW_CMD_SET_MONITOR_PARAM, V1),

		/* event */
	};

	skw_fw_ver = SKW_ZALLOC(sizeof(struct skw_version_info), GFP_KERNEL);
	if (!skw_fw_ver)
		return -ENOMEM;

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SYN_VERSION,
			   NULL, 0, skw_fw_ver,
			   sizeof(struct skw_version_info));
	if (ret) {
		skw_chip_err(skw->idx, "ret: %d\n", ret);
		SKW_KFREE(skw_fw_ver);

		return ret;
	}

	for (i = 0; i < SKW_MAX_MSG_ID; i++) {
		if (skw_fw_ver->cmd[i] == 0 || skw_drv_ver.cmd[i] == 0)
			continue;

		if (skw_drv_ver.cmd[i] != skw_fw_ver->cmd[i]) {
			skw_chip_warn(skw->idx, "cmd: %d, drv ver: %d, fw ver: %d\n",
				 i, skw_drv_ver.cmd[i], skw_fw_ver->cmd[i]);

			ret = -EINVAL;
		}
	}

	SKW_KFREE(skw_fw_ver);

	return ret;

}

static int skw_set_capa(struct skw_core *skw, int capa)
{
	int idx, bit;
	int size = sizeof(skw->ext_capa);

	idx = capa / BITS_PER_BYTE;
	bit = capa % BITS_PER_BYTE;

	BUG_ON(idx >= size);

	skw->ext_capa[idx] |= BIT(bit);

	return 0;
}

static int skw_set_ext_capa(struct skw_core *skw, struct skw_chip_info *chip)
{
	skw_set_capa(skw, SKW_EXT_CAPA_BSS_TRANSITION);
	skw_set_capa(skw, SKW_EXT_CAPA_MBSSID);
	skw_set_capa(skw, SKW_EXT_CAPA_TDLS_SUPPORT);
	skw_set_capa(skw, SKW_EXT_CAPA_TWT_REQ_SUPPORT);

	return 0;
}

static const char *skw_get_chip_id(struct skw_core *skw, u32 chip_type)
{
	switch (chip_type) {
	case 0x100:
		return "EA6621Q";

	case 0x101:
		return "EA6521QF";

	case 0x102:
		return "EA6621QT";

	case 0x103:
		return "EA6521QT";

	case 0x200:
		return "EA6316";

	case 0x300:
		return "SWT6621S";

	default:
		skw_chip_err(skw->idx, "Unsupport chip type: 0x%x\n", chip_type);
		break;
	}

	return NULL;
}

void skw_init_mib(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_set_mib_u8(wiphy, 0, SKW_MIB_SET_LINK_LOSS_THOLD,
			skw->config->fw.beacon_timeout);

	skw_set_mib_u8(wiphy, 0, SKW_MIB_SET_BAND_2G,
			skw->config->band.bw_2ghz);

	skw_set_mib_u8(wiphy, 0, SKW_MIB_SET_WAKEUP_HOST_ENABLE, 1);

	skw_config_set_mib(wiphy, 0, &skw->config->mib.init);
}

int skw_sync_chip_info(struct wiphy *wiphy, struct skw_chip_info *chip)
{
	int ret;
	const char *chipid;
	int vendor, revision;
	u64 ts = local_clock();
	struct skw_core *skw = wiphy_priv(wiphy);

	skw->fw.host_timestamp = ts;
	skw->fw.host_seconds = skw_get_seconds();

	do_div(ts, 1000000);
	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_GET_INFO, &ts, sizeof(ts),
			   chip, sizeof(*chip));
	if (ret) {
		skw_chip_err(skw->idx, "ret: %d\n", ret);
		return ret;
	}

	if (chip->priv_filter_arp)
		set_bit(SKW_FLAG_FW_FILTER_ARP, &skw->flags);

	if (chip->priv_ignore_cred)
		set_bit(SKW_FLAG_FW_IGNORE_CRED, &skw->flags);

	if (chip->priv_p2p_common_port)
		set_bit(SKW_FLAG_LEGACY_P2P_COMMON_PORT, &skw->flags);

	if (chip->priv_dfs_master_enabled)
		skw->dfs.fw_enabled = true;

	if (chip->nr_hw_mac)
		skw->hw.nr_lmac = chip->nr_hw_mac;
	else
		skw->hw.nr_lmac = 1;

	BUG_ON(skw->hw.nr_lmac > SKW_MAX_LMAC_SUPPORT);

	memcpy(skw->fw.build_time, chip->fw_build_time,
	       sizeof(skw->fw.build_time));
	memcpy(skw->fw.plat_ver, chip->fw_plat_ver, sizeof(skw->fw.plat_ver));
	memcpy(skw->fw.wifi_ver, chip->fw_wifi_ver, sizeof(skw->fw.wifi_ver));

	skw->fw.timestamp = chip->fw_timestamp;
	skw->fw.max_num_sta = chip->max_sta_allowed;
	skw->fw.fw_bw_capa = chip->fw_bw_capa;

	chipid = skw_get_chip_id(skw, chip->fw_chip_type);
	if (!chipid) {
		skw_chip_err(skw->idx, "chip id 0x%x not support\n", chip->fw_chip_type);
		return -ENOTSUPP;
	}

	/* BIT[0:1] Reserved
	 * BIT[2:3] BUS Type
	 * BIT[4:7] Vendor ID
	 */
	vendor = 0;

	if (skw_config_append_bus_name(skw->config))
		vendor |= ((skw->hw.bus & 0x3) << 2);

	if (skw_config_append_module_id(skw->config)) {
		if (chip->calib_module_id) {
			int i;
			int vdata = chip->calib_module_id;

			for (i = 0; i < 4; i++) {
				if ((vdata & 0xf) == 0xf)
					vendor |= BIT(4 + i);

				vdata >>= 4;
			}
		}
	}

	revision = skw->hw_pdata->chipid[15];

	if (strlen(skw->config->calib.chip_alias_name))
		chipid = skw->config->calib.chip_alias_name;

	snprintf(skw->fw.calib_file, sizeof(skw->fw.calib_file),
		"%s_%s_R%02X%03X.bin", chipid, skw->config->calib.project,
		vendor & 0xff, revision);

	if (chip->priv_pn_reuse)
		set_bit(SKW_FLAG_FW_PN_REUSE, &skw->flags);

	skw_set_ext_capa(skw, chip);
	/* HT capa */

	skw_chip_dbg(skw->idx, "efuse mac: %pM, %s\n", chip->mac,
		is_valid_ether_addr(chip->mac) ? "valid" : "invalid");

	return 0;
}

static void skw_setup_mac_address(struct wiphy *wiphy, u8 *user_mac, u8 *hw_mac)
{
	int i;
	u8 addr[ETH_ALEN] = {0};
	struct skw_core *skw = wiphy_priv(wiphy);

	if (user_mac && is_valid_ether_addr(user_mac)) {
		skw_ether_copy(addr, user_mac);
	} else if (hw_mac && is_valid_ether_addr(hw_mac)) {
		skw_ether_copy(addr, hw_mac);
	} else {
		eth_random_addr(addr);
		addr[0] = 0xFE;
		addr[1] = 0xFD;
		addr[2] = 0xFC;
	}

	for (i = 0; i < SKW_NR_IFACE; i++) {
		skw_ether_copy(skw->address[i].addr, addr);

		if (i != 0) {
			skw->address[i].addr[0] |= BIT(1);
			skw->address[i].addr[3] ^= BIT(i);
		}

		skw_chip_dbg(skw->idx, "addr[%d]: %pM\n", i, skw->address[i].addr);
	}
}

static int skw_buffer_init(struct skw_core *skw)
{
	int ret, size;

	if (skw->hw.bus == SKW_BUS_PCIE) {
		ret = skw_edma_init(priv_to_wiphy(skw));
		if (ret < 0) {
			skw_chip_err(skw->idx, "edma init failed, ret: %d\n", ret);
			return ret;
		}

		skw->cmd.data = skw->edma.cmd_chn.current_node->buffer;
	} else {
		size = sizeof(struct scatterlist);

		skw->sgl_dat = kcalloc(SKW_NR_SGL_DAT, size, GFP_KERNEL);
		if (!skw->sgl_dat) {
			skw_chip_err(skw->idx, "sg list malloc failed, sg length: %d\n",
				SKW_NR_SGL_DAT);

			return -ENOMEM;
		}

		skw->sgl_cmd = kcalloc(SKW_NR_SGL_CMD, size, GFP_KERNEL);
		if (!skw->sgl_cmd) {
			skw_chip_err(skw->idx, "sg list malloc failed, sg length: %d\n",
				SKW_NR_SGL_CMD);

			SKW_KFREE(skw->sgl_dat);
			return -ENOMEM;
		}

		skw->cmd.data = SKW_ZALLOC(SKW_MSG_BUFFER_LEN, GFP_KERNEL);
		if (!skw->cmd.data) {
			skw_chip_err(skw->idx, "mallc data buffer failed\n");
			SKW_KFREE(skw->sgl_dat);
			SKW_KFREE(skw->sgl_cmd);
			return -ENOMEM;
		}
	}

	return 0;
}

static void skw_buffer_deinit(struct skw_core *skw)
{
	if (skw->hw.bus == SKW_BUS_PCIE) {
		skw_edma_deinit(priv_to_wiphy(skw));
	} else {
		SKW_KFREE(skw->sgl_dat);
		SKW_KFREE(skw->sgl_cmd);
		SKW_KFREE(skw->cmd.data);

		//TODO : recycle txqlen_pending if BSP support API
		if (unlikely(atomic_read(&skw->txqlen_pending)))
			skw_chip_err(skw->idx, "txqlen_pending:%d remind\n", atomic_read(&skw->txqlen_pending));
	}
}

static struct wakeup_source *skw_wakeup_source_init(const char *name)
{
	struct wakeup_source *ws;

	ws = wakeup_source_create(name);
	if (ws)
		wakeup_source_add(ws);

	return ws;
}

static void skw_wakeup_source_deinit(struct wakeup_source *ws)
{
	if (ws) {
		wakeup_source_remove(ws);
		wakeup_source_destroy(ws);
	}
}

static void skw_hw_hal_init(struct skw_core *skw, struct sv6160_platform_data *pdata)
{
	int i, j;
	struct {
		u8 dat_port;
		u8 logic_port;
		u8 flags;
		u8 resv;
	} skw_port[SKW_MAX_LMAC_SUPPORT] = {0};

	atomic_set(&skw->hw.credit, 0);
	skw->hw.align = pdata->align_value;
	skw->hw.cmd_port = pdata->cmd_port;
	skw->hw.pkt_limit = pdata->max_buffer_size / SKW_TX_PACK_SIZE;
	skw->hw.dma = (pdata->bus_type >> 3) & 0x3;

	if (skw->hw.pkt_limit >= SKW_NR_SGL_DAT) {
		WARN_ONCE(1, "pkt limit(%d) larger than buffer", skw->hw.pkt_limit);
		skw->hw.pkt_limit = SKW_NR_SGL_DAT - 1;
	}

	skw->hw.wow.enabled = false;
	set_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags);

	switch (pdata->bus_type & SKW_BUS_TYPE_MASK) {
	case SKW_BUS_SDIO2:
		set_bit(SKW_HW_FLAG_SDIO_V2, &skw->hw.flags);

		/* fall through */
		skw_fallthrough;

	case  SKW_BUS_SDIO:
		skw->hw.bus = SKW_BUS_SDIO;

		skw->hw.bus_dat_xmit = skw_sdio_xmit;
		skw->hw.bus_cmd_xmit = skw_sdio_cmd_xmit;

		set_bit(SKW_HW_FLAG_EXTRA_HDR, &skw->hw.flags);
		skw->hw.extra.hdr_len = SKW_EXTER_HDR_SIZE;
		skw->hw.extra.eof_offset = 23;
		skw->hw.extra.chn_offset = 24;
		skw->hw.extra.len_offset = 7;

		if(test_bit(SKW_HW_FLAG_SDIO_V2, &skw->hw.flags))
			skw->hw.extra.len_offset = 0;

		skw->hw.rx_desc.hdr_offset = SKW_SDIO_RX_DESC_HDR_OFFSET;
		skw->hw.rx_desc.msdu_offset = SKW_SDIO_RX_DESC_MSDU_OFFSET;

		skw_port[0].dat_port = pdata->data_port & 0xf;
		skw_port[0].logic_port = skw_port[0].dat_port;
		skw_port[0].flags = SKW_LMAC_FLAG_INIT |
				    SKW_LMAC_FLAG_RXCB;

		skw_port[1].dat_port = (pdata->data_port >> 4) & 0xf;
		skw_port[1].logic_port = skw_port[1].dat_port;
		if (skw_port[1].logic_port) {
			skw_port[1].flags = SKW_LMAC_FLAG_INIT |
					    SKW_LMAC_FLAG_RXCB;

		}

		break;

	case SKW_BUS_USB2:
		set_bit(SKW_HW_FLAG_USB_V2, &skw->hw.flags);

		/* fall through */
		skw_fallthrough;

	case SKW_BUS_USB:
		skw->hw.bus = SKW_BUS_USB;

		/* ignore PM callback from cfg80211 */
		clear_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags);

		skw->hw.bus_dat_xmit = skw_usb_xmit;
		skw->hw.bus_cmd_xmit = skw_usb_cmd_xmit;

		set_bit(SKW_HW_FLAG_EXTRA_HDR, &skw->hw.flags);
		skw->hw.extra.hdr_len = SKW_EXTER_HDR_SIZE;
		skw->hw.extra.eof_offset = 23;
		skw->hw.extra.chn_offset = 24;
		skw->hw.extra.len_offset = 7;

		if(test_bit(SKW_HW_FLAG_USB_V2, &skw->hw.flags))
			skw->hw.extra.len_offset = 0;

		skw->hw.rx_desc.hdr_offset = SKW_USB_RX_DESC_HDR_OFFSET;
		skw->hw.rx_desc.msdu_offset = SKW_USB_RX_DESC_MSDU_OFFSET;

		skw_port[0].dat_port = pdata->data_port;
		skw_port[0].logic_port = 0;
		skw_port[0].flags = SKW_LMAC_FLAG_INIT |
				    SKW_LMAC_FLAG_RXCB |
				    SKW_LMAC_FLAG_TXCB;

		skw_port[1].dat_port = pdata->data_port;
		skw_port[1].logic_port = 1;
		skw_port[1].flags = SKW_LMAC_FLAG_INIT;

		break;

	case SKW_BUS_PCIE:
		skw->hw.bus = SKW_BUS_PCIE;

		/* ignore PM callback from cfg80211 */
		clear_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags);

		skw->hw.bus_dat_xmit = skw_pcie_xmit;
		skw->hw.bus_cmd_xmit = skw_pcie_cmd_xmit;
		skw->hw.dma = SKW_ASYNC_EDMA_TX;

		skw->hw.rx_desc.hdr_offset = SKW_PCIE_RX_DESC_HDR_OFFSET;
		skw->hw.rx_desc.msdu_offset = SKW_PCIE_RX_DESC_MSDU_OFFSET;

		skw->hw.cmd_port = -1;

		skw->hw.pkt_limit = SKW_EDMA_TX_CHN_NODE_NUM * TX_BUF_ADDR_CNT;

		skw_port[0].dat_port = 0;
		skw_port[0].logic_port = 0;
		skw_port[0].flags = SKW_LMAC_FLAG_INIT;

		skw_port[1].logic_port = 0;
		skw_port[1].dat_port = 0;
		skw_port[1].flags = SKW_LMAC_FLAG_INIT;

		break;

	default:
		break;
	}

	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		skw->hw.lmac[i].id = i;
		skw->hw.lmac[i].lport = skw_port[i].logic_port;
		skw->hw.lmac[i].dport = skw_port[i].dat_port;
		skw->hw.lmac[i].flags = skw_port[i].flags;
		skw->hw.lmac[i].iface_bitmap = 0;

		skw->hw.lmac[i].skw = skw;
		skb_queue_head_init(&skw->hw.lmac[i].rx_dat_q);
		skb_queue_head_init(&skw->hw.lmac[i].avail_skb);
		skb_queue_head_init(&skw->hw.lmac[i].edma_free_list);

		atomic_set(&skw->hw.lmac[i].fw_credit, 0);
		atomic_set(&skw->hw.lmac[i].avail_skb_num, 0);

		for (j = 0; j < SKW_MAX_PEER_SUPPORT; j++) {
			skw->hw.lmac[i].peer_ctx[j].idx = j;
			mutex_init(&skw->hw.lmac[i].peer_ctx[j].lock);
		}
	}
}

static int skw_core_init(struct skw_core *skw, struct platform_device *pdev, int idx)
{
	int ret;
	char name[32] = {0};

	skw->idx = idx;

	skw->hw_pdata = dev_get_platdata(&pdev->dev);
	if (!skw->hw_pdata) {
		skw_chip_err(skw->idx, "get drv data failed\n");
		return -ENODEV;
	}

	skw->config = &g_skw_config.config;

	skw_hw_hal_init(skw, skw->hw_pdata);

	snprintf(name, sizeof(name), "chip%d.%s", idx, skw_bus_name(skw->hw.bus));
	skw->dentry = skw_debugfs_subdir(name, NULL);
	skw->pentry = skw_procfs_subdir(name, NULL);

	skw_chip_info(skw->idx, " name=%s\n",name);

	mutex_init(&skw->lock);

#ifdef CONFIG_SWT6621S_SAP_SME_EXT
	set_bit(SKW_FLAG_SAP_SME_EXTERNAL, &skw->flags);
#endif

#ifdef CONFIG_SWT6621S_STA_SME_EXT
	set_bit(SKW_FLAG_STA_SME_EXTERNAL, &skw->flags);
#endif

#ifdef CONFIG_SWT6621S_REPEATER_MODE
	set_bit(SKW_FLAG_REPEATER, &skw->flags);
#endif
	skw->regd = NULL;
	skw->country[0] = '0';
	skw->country[1] = '0';

	atomic_set(&skw->exit, 0);
	atomic_set(&skw->tx_wake, 0);
	atomic_set(&skw->rx_wake, 0);

	init_waitqueue_head(&skw->tx_wait_q);
	init_waitqueue_head(&skw->rx_wait_q);

	skb_queue_head_init(&skw->rx_dat_q);
	atomic_set(&skw->txqlen_pending, 0);

	spin_lock_init(&skw->dfs.skw_pool_lock);
	INIT_LIST_HEAD(&skw->dfs.skw_pulse_pool);
	INIT_LIST_HEAD(&skw->dfs.skw_pseq_pool);

	sema_init(&skw->cmd.lock, 1);
	init_waitqueue_head(&skw->cmd.wq);
	skw->cmd.ws = skw_wakeup_source_init("skwifi_cmd");
	sema_init(&skw->cmd.mgmt_cmd_lock, 1);

	spin_lock_init(&skw->vif.lock);

	skw_event_work_init(&skw->event_work, skw_default_event_work);
	skw->event_wq = alloc_workqueue("skw_evt_wq.%d", WQ_UNBOUND | WQ_MEM_RECLAIM, 1, idx);
	if (!skw->event_wq) {
		ret = -ENOMEM;
		skw_chip_err(skw->idx, "alloc event wq failed, ret: %d\n", ret);

		goto deinit_ws;
	}

	ret = skw_dpd_init(&skw->dpd);
	if (ret < 0)
		goto deinit_wq;

	ret = skw_buffer_init(skw);
	if (ret < 0)
		goto deinit_dpd;

	ret = skw_rx_init(skw);
	if (ret < 0) {
		skw_chip_err(skw->idx, "rx init failed, ret: %d\n", ret);
		goto deinit_buff;
	}

	ret = skw_tx_init(skw);
	if (ret < 0) {
		skw_chip_err(skw->idx, "tx init failed, ret: %d\n", ret);
		goto deinit_rx;
	}

	skw->dbg.nr_cmd = SKW_DBG_NR_CMD;
	skw->dbg.nr_dat = SKW_DBG_NR_DAT;
	atomic_set(&skw->dbg.loop, 0);
	skw->isr_cpu_id = -1;

	return 0;

deinit_rx:
	skw_rx_deinit(skw);

deinit_buff:
	skw_buffer_deinit(skw);

deinit_dpd:
	skw_dpd_deinit(&skw->dpd);

deinit_wq:
	destroy_workqueue(skw->event_wq);
	skw_event_work_deinit(&skw->event_work);

deinit_ws:
	skw_wakeup_source_deinit(skw->cmd.ws);

	debugfs_remove_recursive(skw->dentry);
	proc_remove(skw->pentry);

	return ret;
}

static void skw_core_deinit(struct skw_core *skw)
{
	skw_tx_deinit(skw);

	skw_rx_deinit(skw);

	skw_buffer_deinit(skw);

	skw_dpd_deinit(&skw->dpd);

	destroy_workqueue(skw->event_wq);

	skw_event_work_deinit(&skw->event_work);

	skw_wakeup_source_deinit(skw->cmd.ws);

	debugfs_remove_recursive(skw->dentry);
	proc_remove(skw->pentry);
}

int skw_set_ip(struct wiphy *wiphy, struct net_device *ndev,
		struct skw_setip_param *setip_param, int size)
{
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);
	//int size = 0;

	//size = sizeof(struct skw_setip_param);
	ret = skw_queue_work(wiphy, netdev_priv(ndev),
			SKW_WORK_SET_IP, setip_param, size);
	if (ret)
		skw_chip_err(skw->idx, "Set IP failed\n");

	return ret;
}

/*
 * Get the IP address for both IPV4 and IPV6, set it
 * to firmware while they are valid.
 */
void skw_set_ip_to_fw(struct wiphy *wiphy, struct net_device *ndev)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;

	struct inet6_dev *idev;
	struct inet6_ifaddr *ifp;

	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_setip_param setip_param[SKW_FW_IPV6_COUNT_LIMIT+1];
	int ip_count = 0, ipv6_count = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	in_dev = __in_dev_get_rtnl(ndev);
	if (!in_dev)
		goto ipv6;

	ifa = in_dev->ifa_list;
	if (!ifa || !ifa->ifa_local)
		goto ipv6;

	skw_chip_info(skw->idx, "ip addr: %pI4\n", &ifa->ifa_local);
	setip_param[ip_count].ip_type = SKW_IP_IPV4;
	setip_param[ip_count].ipv4 = ifa->ifa_local;

	if(is_skw_sta_mode(iface))
		iface->sta.ipv4 = ifa->ifa_local;

	ip_count++;

ipv6:
	idev = __in6_dev_get(ndev);
	if (!idev)
		goto ip_apply;

	read_lock_bh(&idev->lock);
	list_for_each_entry_reverse(ifp, &idev->addr_list, if_list) {
		skw_chip_info(skw->idx, "ip addr: %pI6\n", &ifp->addr);
		if (++ipv6_count > SKW_FW_IPV6_COUNT_LIMIT) {
			skw_chip_warn(skw->idx, "%s set first %d of %d ipv6 address\n",
				ndev->name, SKW_FW_IPV6_COUNT_LIMIT, ipv6_count);

			read_unlock_bh(&idev->lock);
			goto ip_apply;
		}

		setip_param[ip_count].ip_type = SKW_IP_IPV6;
		memcpy(&setip_param[ip_count].ipv6, &ifp->addr, 16);
		ip_count++;
	}
	read_unlock_bh(&idev->lock);

ip_apply:
	if (ip_count)
		skw_set_ip(wiphy, ndev, setip_param, ip_count*sizeof(struct skw_setip_param));

}

#ifdef CONFIG_INET
static int skw_ifa4_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct in_ifaddr *ifa = data;
	struct net_device *ndev;
	struct wireless_dev *wdev;
	struct skw_core *skw = container_of(nb, struct skw_core, ifa4_nf);

	skw_chip_dbg(skw->idx, "action: %ld\n", action);

	if (!ifa->ifa_dev)
		return NOTIFY_DONE;

	ndev = ifa->ifa_dev->dev;
	wdev = ndev->ieee80211_ptr;

	if (!wdev || wdev->wiphy != priv_to_wiphy(skw))
		return NOTIFY_DONE;

	if (ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_STATION &&
	    ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_P2P_CLIENT)
		return NOTIFY_DONE;

	switch (action) {
	case NETDEV_UP:
//	case NETDEV_DOWN:
		skw_set_ip_to_fw(wdev->wiphy, ndev);

		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

#endif

#ifdef CONFIG_IPV6
static int skw_ifa6_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct inet6_ifaddr *ifa6 = data;
	struct skw_core *skw = container_of(nb, struct skw_core, ifa6_nf);

	struct net_device *ndev;
	struct wireless_dev *wdev;

	skw_chip_dbg(skw->idx, "action: %ld\n", action);

	if (!ifa6->idev || !ifa6->idev->dev)
		return NOTIFY_DONE;

	ndev = ifa6->idev->dev;
	wdev = ndev->ieee80211_ptr;

	if (!wdev || wdev->wiphy != priv_to_wiphy(skw))
		return NOTIFY_DONE;

	switch (action) {
	case NETDEV_UP:
	// case NETDEV_DOWN:

		skw_set_ip_to_fw(wdev->wiphy, ndev);

		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}
#endif

static int skw_bsp_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct skw_core *skw = container_of(nb, struct skw_core, bsp_nf);
	struct wiphy *wiphy = priv_to_wiphy(skw);
	struct cfg80211_wowlan wow, *pwow = NULL;

	skw_chip_info(skw->idx, "action: %ld, skw flags: 0x%lx, nf_flags: 0x%lx\n",
		action, skw->flags, skw->nf_flags);

	switch (action) {
	case SKW_BSP_NF_ASSERT:
	case SKW_BSP_NF_BLOCKED:
	case SKW_BSP_NF_FW_REBOOT:
		set_bit(SKW_FLAG_FW_ASSERT, &skw->flags);

		WRITE_ONCE(skw->nf_flags, 0);
		set_bit(SKW_BSP_NF_ASSERT, &skw->nf_flags);

		skw_abort_cmd(skw);
		cancel_work_sync(&skw->recovery_work);
		skw_wifi_disable(skw->hw_pdata);

		break;

	case SKW_BSP_NF_READY:
		if (test_and_clear_bit(SKW_BSP_NF_ASSERT, &skw->nf_flags))
			schedule_work(&skw->recovery_work);

		break;

	case SKW_BSP_NF_SUSPEND:
		if (!test_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags)) {
			if (skw->hw.wow.enabled)  {
				memset(&wow, 0x0, sizeof(struct cfg80211_wowlan));

				if (skw->hw.wow.flags & SKW_WOW_DISCONNECT)
					wow.disconnect = true;

				if (skw->hw.wow.flags & SKW_WOW_MAGIC_PKT)
					wow.magic_pkt = true;

				if (skw->hw.wow.flags & SKW_WOW_GTK_REKEY_FAIL)
					wow.gtk_rekey_failure = true;

				if (skw->hw.wow.flags & SKW_WOW_EAP_IDENTITY_REQ)
					wow.eap_identity_req = true;

				if (skw->hw.wow.flags & SKW_WOW_FOUR_WAY_HANDSHAKE)
					wow.four_way_handshake = true;

				if (skw->hw.wow.flags & SKW_WOW_RFKILL_RELEASE)
					wow.rfkill_release = true;

				pwow = &wow;
			}

			set_bit(SKW_BSP_NF_SUSPEND, &skw->nf_flags);

			skw_suspend(wiphy, pwow);
		}

		break;

	case SKW_BSP_NF_RESUME:
		if (test_and_clear_bit(SKW_BSP_NF_SUSPEND, &skw->nf_flags))
			skw_work_resume(wiphy);

		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static int skw_pm_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct skw_core *skw = container_of(nb, struct skw_core, pm_nf);

	skw_chip_dbg(skw->idx, "action: %ld\n", action);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		set_bit(SKW_FLAG_SUSPEND_PREPARE, &skw->flags);
		break;

	case PM_POST_SUSPEND:
		clear_bit(SKW_FLAG_SUSPEND_PREPARE, &skw->flags);
		break;
	}

	return 0;
}

static void skw_ifa_notifier_register(struct skw_core *skw)
{
#ifdef CONFIG_INET
	skw->ifa4_nf.notifier_call = skw_ifa4_notifier;
	register_inetaddr_notifier(&skw->ifa4_nf);
#endif

#ifdef CONFIG_IPV6
	skw->ifa6_nf.notifier_call = skw_ifa6_notifier;
	register_inet6addr_notifier(&skw->ifa6_nf);
#endif

	skw->bsp_nf.notifier_call = skw_bsp_notifier;
	skw_register_bsp_notifier(skw, &skw->bsp_nf);

	skw->pm_nf.notifier_call = skw_pm_notifier;
	register_pm_notifier(&skw->pm_nf);
}

static void skw_ifa_notifier_unregister(struct skw_core *skw)
{
#ifdef CONFIG_INET
	unregister_inetaddr_notifier(&skw->ifa4_nf);
#endif

#ifdef CONFIG_IPV6
	unregister_inet6addr_notifier(&skw->ifa6_nf);
#endif

	skw_unregister_bsp_notifier(skw, &skw->bsp_nf);

	unregister_pm_notifier(&skw->pm_nf);
}

int skw_lmac_bind_iface(struct skw_core *skw, struct skw_iface *iface, int lmac_id)
{
	struct skw_lmac *lmac;

	if (lmac_id >= skw->hw.nr_lmac) {
		skw_chip_err(skw->idx, "invalid lmac id: %d\n", skw->hw.nr_lmac);
		return -ENOTSUPP;
	}

	iface->lmac_id = lmac_id;
	lmac = &skw->hw.lmac[lmac_id];
	BUG_ON(!(lmac->flags & SKW_LMAC_FLAG_INIT));

	if (skw->hw.bus == SKW_BUS_PCIE) {
		// TODO:
		// register edma channel
		// skw_edma_enable_channel(&lmac->edma_tx_chn, isr);
	}

	SKW_SET(lmac->iface_bitmap, BIT(iface->id));
	SKW_SET(lmac->flags, SKW_LMAC_FLAG_ACTIVED);

	return 0;
}

int skw_lmac_unbind_iface(struct skw_core *skw, int lmac_id, int iface_id)
{
	struct skw_lmac *lmac;

	if (lmac_id >= skw->hw.nr_lmac) {
		skw_chip_err(skw->idx, "invalid lmac id: %d\n", skw->hw.nr_lmac);
		return 0;
	}

	lmac = &skw->hw.lmac[lmac_id];

	SKW_CLEAR(lmac->iface_bitmap, BIT(iface_id));

	if (lmac->iface_bitmap)
		return 0;

	// TODO:
	// unregister edma channel

	SKW_CLEAR(lmac->flags, SKW_LMAC_FLAG_ACTIVED);

	skw_chip_dbg(skw->idx, "reset fw credit for mac:%d", lmac_id);
	atomic_set(&skw->hw.lmac[lmac_id].fw_credit, 0);

	return 0;
}

void skw_add_credit(struct skw_core *skw, int lmac_id, int cred)
{
	trace_skw_tx_add_credit(lmac_id, cred);

	atomic_add(cred, &skw->hw.lmac[lmac_id].fw_credit);
	smp_wmb();

	skw_wakeup_tx(skw, 0);
	skw_chip_detail(skw->idx, "lmac_id:%d cred:%d", lmac_id, cred);

	if (skw->hw.bus == SKW_BUS_SDIO || skw->hw.bus == SKW_BUS_SDIO2)
		queue_work(skw->tx_dy, &skw->hw.lmac[lmac_id].dy_work);

}

static int skw_add_default_iface(struct wiphy *wiphy)
{
	int ret = 0;
	struct skw_iface *iface = NULL;
#ifdef CONFIG_SWT6621S_LEGACY_P2P
	struct skw_iface *p2p = NULL;
#endif
	struct skw_core *skw = wiphy_priv(wiphy);

	rtnl_lock();

	iface = skw_add_iface(wiphy, "wlan%d", NL80211_IFTYPE_STATION,
				skw_mac, SKW_INVALID_ID, true);
	if (IS_ERR(iface)) {
		ret = PTR_ERR(iface);
		skw_chip_err(skw->idx, "add iface failed, ret: %d\n", ret);

		goto unlock;
	}

#ifdef CONFIG_SWT6621S_LEGACY_P2P
	if (test_bit(SKW_FLAG_LEGACY_P2P_COMMON_PORT, &skw->flags))
		p2p = skw_add_iface(wiphy, "p2p%d", NL80211_IFTYPE_STATION,
				NULL, SKW_LAST_IFACE_ID, true);
	else
		p2p = skw_add_iface(wiphy, "p2p%d", NL80211_IFTYPE_P2P_DEVICE,
				NULL, SKW_LAST_IFACE_ID, true);

	if (IS_ERR(p2p)) {
		ret = PTR_ERR(p2p);
		skw_chip_err(skw->idx, "add p2p legacy interface failed, ret: %d\n", ret);

		skw_del_iface(wiphy, iface);

		goto unlock;
	}

	if (!test_bit(SKW_FLAG_LEGACY_P2P_COMMON_PORT, &skw->flags))
		p2p->flags |= SKW_IFACE_FLAG_LEGACY_P2P_DEV;

#endif

unlock:
	rtnl_unlock();

	return ret;
}

static int skw_calib_bin_download(struct wiphy *wiphy, const u8 *data, u32 size)
{
	int i = 0, ret = 0;
	int buf_size, remain = size;
	struct skw_calib_param calib;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "file size: %d\n", size);

	buf_size = sizeof(calib.data);

	while (remain > 0) {
		calib.len = (remain < buf_size) ? remain : buf_size;
		calib.seq = i;
		remain -= calib.len;

		memcpy(calib.data, data, calib.len);

		if (!remain)
			calib.end = 1;
		else
			calib.end = 0;

		skw_chip_dbg(skw->idx, "bb_file remain: %d, seq: %d len: %d end: %d\n",
			remain, calib.seq, calib.len, calib.end);

		ret = skw_msg_xmit_timeout(wiphy, 0, SKW_CMD_PHY_BB_CFG,
			&calib, sizeof(calib), NULL, 0,
			"SKW_CMD_PHY_BB_CFG", msecs_to_jiffies(5000), 0);
		if (ret) {
			skw_chip_err(skw->idx, "failed, ret: %d,  seq: %d\n", ret, i);
			break;
		}

		i++;
		data += calib.len;
	}

	return ret;
}

int skw_calib_download(struct wiphy *wiphy, const char *fname)
{
	int ret = 0;
	const struct firmware *fw;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "fw file: %s\n", fname);

	ret = request_firmware(&fw, fname, &wiphy->dev);
	if (ret) {
		skw_chip_err(skw->idx, "load %s failed, ret: %d\n", fname, ret);
		return ret;
	}

	ret = skw_calib_bin_download(wiphy, fw->data, fw->size);
	if (ret != 0)
		skw_chip_err(skw->idx, "bb_file cali msg fail\n");

	release_firmware(fw);

	return ret;
}

static int skw_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct skw_chip_info chip;
	struct wiphy *wiphy = NULL;
	struct skw_core *skw = NULL;
	int idx = skw_add_chip_idx();

	skw_chip_info(idx, "chip: %d, MAC: %pM\n", idx, skw_mac);

	wiphy = skw_alloc_wiphy(sizeof(struct skw_core));
	if (!wiphy) {
		ret = -ENOMEM;
		skw_chip_err(idx, "malloc wiphy failed\n");

		goto failed;
	}

	set_wiphy_dev(wiphy, &pdev->dev);

	skw = wiphy_priv(wiphy);

	skw_chip_info(idx, "chip_idx: %d, wiphy: %s\n", idx, wiphy->dev.kobj.name);

	ret = skw_core_init(skw, pdev, idx);
	if (ret) {
		skw_chip_err(skw->idx, "core init failed, ret: %d\n", ret);
		goto free_wiphy;
	}

	skw_regd_init(wiphy);
	skw_vendor_init(wiphy);
	skw_work_init(wiphy);
	skw_timer_init(skw);
	skw_recovery_init(skw);

	skw_wifi_enable(dev_get_platdata(&pdev->dev));

	if (skw_sync_cmd_event_version(wiphy) ||
	    skw_sync_chip_info(wiphy, &chip))
		goto core_deinit;

#ifdef CONFIG_PLATFORM_ROCKCHIP
	if (!is_valid_ether_addr(skw_mac))
		rockchip_wifi_mac_addr(skw_mac);
#endif

	skw_setup_mac_address(wiphy, skw_mac, chip.mac);

	if (skw_calib_download(wiphy, skw->fw.calib_file) < 0)
		goto core_deinit;

	ret = skw_setup_wiphy(wiphy, &chip);
	if (ret) {
		skw_chip_err(skw->idx, "setup wiphy failed, ret: %d\n", ret);
		goto core_deinit;
	}

	/* make sure mib init after calib download */
	skw_init_mib(wiphy);

	rtnl_lock();

	skw_set_wiphy_regd(wiphy, skw->country);

	if (skw->config->regd.country[0] != '0' &&
	    skw->config->regd.country[1] != '0')
		skw_set_regdom(wiphy, skw->config->regd.country);

	rtnl_unlock();

	ret = skw_add_default_iface(wiphy);
	if (ret)
		goto unregister_wiphy;

	skw_ifa_notifier_register(skw);

	platform_set_drvdata(pdev, skw);

	skw_core_dbg_init(wiphy);

	return 0;

unregister_wiphy:
	wiphy_unregister(wiphy);

core_deinit:
	skw_wifi_disable(dev_get_platdata(&pdev->dev));
	skw_recovery_deinit(skw);
	skw_timer_deinit(skw);
	skw_work_deinit(wiphy);
	skw_vendor_deinit(wiphy);
	skw_core_deinit(skw);

free_wiphy:
	wiphy_free(wiphy);

failed:
	skw_dec_chip_idx(idx);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
static void skw_drv_remove(struct platform_device *pdev)
#else
static int skw_drv_remove(struct platform_device *pdev)
#endif
{
	int i;
	struct wiphy *wiphy;
	struct skw_core *skw = platform_get_drvdata(pdev);

	skw_chip_info(skw->idx, "%s\n", pdev->name);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
	if (!skw)
		return;
#else
	if (!skw)
		return 0;
#endif


	wiphy = priv_to_wiphy(skw);

	skw_chip_info(skw->idx, "%s, name=chip%d.%s, wiphy: %s\n",
		pdev->name, skw->idx, skw_bus_name(skw->hw.bus), wiphy->dev.kobj.name);

	skw_ifa_notifier_unregister(skw);

	rtnl_lock();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	cfg80211_shutdown_all_interfaces(wiphy);
#endif

	for (i = 0; i < SKW_NR_IFACE; i++)
		skw_del_iface(wiphy, skw->vif.iface[i]);

	rtnl_unlock();

	wiphy_unregister(wiphy);

	skw_wifi_disable(dev_get_platdata(&pdev->dev));

	skw_recovery_deinit(skw);

	skw_timer_deinit(skw);

	skw_vendor_deinit(wiphy);

	skw_core_deinit(skw);

	skw_work_deinit(wiphy);

	wiphy_free(wiphy);

	skw_dec_chip_idx(skw->idx);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
    return;
#else
    return 0;
#endif
}

static struct platform_driver skw_drv = {
	.probe = skw_drv_probe,
	.remove = skw_drv_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sv6621s_wireless1",
	},
};

static void skw_global_config_init(struct skw_global_config *cfg)
{
	memset(cfg, 0x0, sizeof(struct skw_global_config));

	atomic_set(&cfg->index, 0);
	skw_config_init(&cfg->config);
}

static void skw_global_config_deinit(struct skw_global_config *cfg)
{
	skw_config_deinit(&cfg->config);
}

static int __init skw_module_init(void)
{
	int ret = 0;

	pr_info("[%s] VERSION: %s (%s)\n",
		SKW_TAG_INFO, SKW_VERSION, UTS_RELEASE);

	skw_dentry_init();
	skw_log_level_init();

	skw_power_on_chip();

	skw_global_config_init(&g_skw_config);

	ret = platform_driver_register(&skw_drv);
	if (ret) {
		skw_err("register %s failed, ret: %d\n",
			skw_drv.driver.name, ret);

		skw_power_off_chip();

		skw_log_level_deinit();
		skw_dentry_deinit();
	}

	return ret;
}

static void __exit skw_module_exit(void)
{
	skw_info("unload\n");

	skw_del_dbg_iface();
	platform_driver_unregister(&skw_drv);

	skw_global_config_deinit(&g_skw_config);

	skw_power_off_chip();

	skw_log_level_deinit();
	skw_dentry_deinit();
}

module_init(skw_module_init);
module_exit(skw_module_exit);

module_param_array_named(mac, skw_mac, byte, NULL, 0444);
MODULE_PARM_DESC(mac, "config mac address");

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("seekwavetech");
