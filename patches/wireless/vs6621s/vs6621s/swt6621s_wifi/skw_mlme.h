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

#ifndef __SKW_MLME_H__
#define __SKW_MLME_H__

#include "skw_iface.h"

struct skw_client {
	struct list_head list;
	struct skw_iface *iface;
	enum SKW_STATES state;
	u32 capa;

	u16 aid;
	u8 addr[ETH_ALEN];

	u8 *challenge;
	u64 cookie;
	unsigned long idle;
	u8 *assoc_req_ie;
	u16 assoc_req_ie_len;
	u16 last_seq_ctrl;
};

struct skw_element_info {
	struct {
		int len;
		u8 data[32];
	} ssid;
	const struct skw_element *support_rate;
	const struct skw_element *ext_rate;
	const struct skw_element *ht_capa;
	const struct skw_element *ht_oper;
	const struct skw_element *vht_capa;
	const struct skw_element *vht_oper;
	const struct skw_element *ext_capa;
	const struct skw_element *vendor_vht;
};

int skw_mgmt_frame_with_reason(struct skw_iface *iface, u8 *da, u64 *cookie, u8 *bssid,
			struct ieee80211_channel *ch, u16 stype, u16 reason, bool switchover);

int skw_ap_simple_reply(struct skw_iface *iface, struct skw_client *client,
			u16 stype, u16 reason);

static inline int skw_ap_send_deauth(struct skw_iface *iface,
				     struct skw_client *client, u16 reason)
{
	return skw_ap_simple_reply(iface, client,
					IEEE80211_STYPE_DEAUTH, reason);
}


static inline int skw_ap_send_disassoc(struct skw_iface *iface,
				       struct skw_client *client, u16 code)
{
	return skw_ap_simple_reply(iface, client,
					IEEE80211_STYPE_DISASSOC, code);
}

void skw_mlme_sta_tx_status(struct skw_iface *iface, u64 cookie,
			   const u8 *frame, int frame_len, u16 ack);
int skw_mlme_sta_rx_mgmt(struct skw_iface *iface, int freq, int signal,
			void *frame, int frame_len);
int skw_process_auth_response(struct skw_iface *iface, int freq,
			int signal, void *frame, int frame_len);
int skw_ap_mgmt_handler(struct skw_iface *iface, void *frame, int frame_len);
void skw_mlme_ap_del_sta(struct wiphy *wiphy, struct net_device *ndev,
			 const u8 *addr, u8 force);
int skw_mlme_ap_rx_mgmt(struct skw_iface *iface, u16 fc, int freq,
			int signal, void *frame, int frame_len);
void skw_mlme_ap_remove_client(struct skw_iface *iface, const u8 *addr);
void skw_mlme_ap_tx_status(struct skw_iface *iface, u64 cookie,
			   const u8 *frame, int frame_len, u16 ack);
int skw_mlme_sta_rx_auth(struct skw_iface *iface, int freq, int signal,
			 void *buf, int len);

int skw_mlme_sta_rx_assoc(struct skw_iface *iface, struct cfg80211_bss *bss,
			  void *frame, int len, void *req_ie, int req_ie_len);
#endif
