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
#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>

#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_timer.h"
#include "skw_msg.h"
#include "skw_mlme.h"
#include "skw_work.h"

#define SKW_AP_AUTH_TIMEOUT     5000
#define SKW_IEEE80211_HDR_LEN   24

static void skw_mlme_ap_del_client(struct skw_iface *iface,
				struct skw_client *client)
{
	struct skw_core *skw = iface->skw;

	if (!client)
		return;

	skw_chip_dbg(skw->idx, "client: %pM\n", client->addr);

	skw_del_timer_work(iface->skw, client);

	skw_list_del(&iface->sap.mlme_client_list, &client->list);

	if (client->aid)
		clear_bit(client->aid, iface->sap.aid_map);

	SKW_KFREE(client->assoc_req_ie);
	SKW_KFREE(client->challenge);
	SKW_KFREE(client);
}

static struct skw_client *
skw_mlme_ap_get_client(struct skw_iface *iface, const u8 *addr)
{
	struct skw_client *client = NULL, *tmp;

	if (!iface)
		return NULL;

	spin_lock_bh(&iface->sap.mlme_client_list.lock);

	list_for_each_entry(tmp, &iface->sap.mlme_client_list.list, list) {
		if (ether_addr_equal(addr, tmp->addr)) {
			client = tmp;
			break;
		}
	}

	spin_unlock_bh(&iface->sap.mlme_client_list.lock);

	return client;
}

static struct skw_client *
skw_mlme_ap_add_client(struct skw_iface *iface, const u8 *addr)
{
	struct skw_client *client = NULL;
	struct skw_core *skw = iface->skw;

	client = SKW_ZALLOC(sizeof(*client), GFP_KERNEL);
	if (client) {
		INIT_LIST_HEAD(&client->list);
		client->iface = iface;
		client->state = SKW_STATE_NONE;
		client->last_seq_ctrl = 0xFFFF;
		client->idle = jiffies;
		client->challenge = NULL;
		client->assoc_req_ie = NULL;
		client->aid = 0;
		skw_ether_copy(client->addr, addr);
		skw_chip_dbg(skw->idx, "%pM\n", client->addr);

		skw_list_add(&iface->sap.mlme_client_list, &client->list);
	}

	return client;
}

void skw_mlme_ap_remove_client(struct skw_iface *iface, const u8 *addr)
{
	struct skw_client *client;

	if (!iface->sap.sme_external) {
		client = skw_mlme_ap_get_client(iface, addr);
		skw_mlme_ap_del_client(iface, client);
	}
}

void skw_mlme_ap_del_sta(struct wiphy *wiphy, struct net_device *ndev,
			 const u8 *addr, u8 force)
{
	int ret = -1;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "sta: %pM\n", addr);

	ret = skw_del_station(wiphy, ndev, addr, 12, 3);
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
		return;
	}

}

static void skw_mlme_ap_auth_timeout(void *data)
{
	unsigned long timeout;
	struct skw_client *client = data;

	if (!client)
		return;

	skw_chip_dbg(client->iface->skw->idx, "client: %pM\n", client->addr);

	if (client->state == SKW_STATE_ASSOCED)
		return;

	timeout = client->idle + msecs_to_jiffies(SKW_AP_AUTH_TIMEOUT);
	if (time_after(jiffies, timeout)) {
		skw_queue_local_event(priv_to_wiphy(client->iface->skw),
				client->iface, SKW_EVENT_LOCAL_AP_AUTH_TIMEOUT,
				client, sizeof(*client));
		return;
	}
}

#if 0
void skw_flush_sta_info(struct skw_iface *iface)
{
	LIST_HEAD(flush_list);
	struct skw_client *sta;

	spin_lock_bh(&iface->sap.sta_lock);
	list_replace_init(&iface->sap.mlme_client_list, &flush_list);
	spin_unlock_bh(&iface->sap.sta_lock);

	// fixme:
	// deauth all sta
	while (!list_empty(&flush_list)) {
		sta = list_first_entry(&flush_list, struct skw_client, list);
		list_del(&sta->list);
		skw_dbg("sta: %pM, state: %d\n", sta->addr, sta->state);
		// skw_mlle_ap_state_event(sta, SKW_STATE_NONE);
		SKW_KFREE(sta);
	}
}
#endif

int skw_mgmt_frame_with_reason(struct skw_iface *iface, u8 *da, u64 *cookie, u8 *bssid,
			struct ieee80211_channel *ch, u16 stype, u16 reason, bool switchover)
{
	struct wiphy *wiphy = priv_to_wiphy(iface->skw);
	struct ieee80211_mgmt reply;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "stype: %d, reason: %d\n", stype, reason);

	reply.frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | stype);
	reply.duration = 0;
	reply.seq_ctrl = 0;
	skw_ether_copy(reply.da, da);
	skw_ether_copy(reply.sa, iface->addr);
	skw_ether_copy(reply.bssid, bssid);

	reply.u.deauth.reason_code = cpu_to_le16(reason);

	return skw_mgmt_tx(wiphy, iface, ch, 0,
			   cookie, false, &reply,
			   SKW_DEAUTH_FRAME_LEN, SKW_DEAUTH_FRAME_LEN, &reply, switchover);
}


int skw_ap_simple_reply(struct skw_iface *iface, struct skw_client *client,
			u16 stype, u16 reason)
{
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "stype: %d, reason: %d\n", stype, reason);

	return skw_mgmt_frame_with_reason(iface, client->addr, &client->cookie,
			iface->sap.cfg.bssid, iface->sap.cfg.channel, stype, reason, true);
}

static void skw_mlme_ap_auth_cb(struct skw_iface *iface,
				struct skw_client *client,
				struct ieee80211_mgmt *mgmt,
				int mgmt_len, bool ack)
{
	u16 status;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "client: %pM, ack: %d\n", client->addr, ack);
	if (!client)
		return;

	status = le16_to_cpu(mgmt->u.auth.status_code);

	if (ack && status == WLAN_STATUS_SUCCESS) {
		skw_del_timer_work(iface->skw, client);
		skw_add_timer_work(iface->skw, "auth_timeout",
				   skw_mlme_ap_auth_timeout,
				   client, SKW_AP_AUTH_TIMEOUT,
				   client, GFP_KERNEL);
	} else {
		skw_chip_warn(skw->idx, "failed\n");
		client->state = SKW_STATE_NONE;
		skw_mlme_ap_del_sta(iface->wdev.wiphy,
				iface->ndev, client->addr, false);
	}
}

static void skw_mlme_ap_assoc_cb(struct skw_iface *iface,
				 struct skw_client *client,
				 struct ieee80211_mgmt *mgmt,
				 int frame_len, bool ack, int reassoc)
{
	u16 status_code;
	struct station_info info;
	struct station_parameters params;
	struct skw_core *skw = iface->skw;

	if (!client)
		return;

	if (reassoc)
		status_code = le16_to_cpu(mgmt->u.reassoc_resp.status_code);
	else
		status_code = le16_to_cpu(mgmt->u.assoc_resp.status_code);

	skw_chip_dbg(skw->idx, "client: %pM, ack: %d, status code: %d\n",
		client->addr, ack, status_code);

	if (ack && status_code == WLAN_STATUS_SUCCESS) {
		skw_del_timer_work(iface->skw, client);

		params.sta_flags_set = 0;
		params.sta_flags_set |= BIT(NL80211_STA_FLAG_ASSOCIATED);
		skw_change_station(iface->wdev.wiphy, iface->ndev,
				client->addr, &params);

		memset(&info, 0x0, sizeof(info));
		if (client->assoc_req_ie) {
			info.assoc_req_ies = client->assoc_req_ie;
			info.assoc_req_ies_len = client->assoc_req_ie_len;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0))
			info.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
		}

		cfg80211_new_sta(iface->ndev, client->addr,
				 &info, GFP_KERNEL);
		SKW_KFREE(client->assoc_req_ie);
		client->assoc_req_ie = NULL;
		client->assoc_req_ie_len = 0;

		client->state = SKW_STATE_ASSOCED;
	} else {
		skw_chip_err(skw->idx, "failed, ack: %d, status_code: %d\n", ack, status_code);

		client->state = SKW_STATE_NONE;
		skw_mlme_ap_del_sta(iface->wdev.wiphy,
				iface->ndev, client->addr, false);
	}
}

void skw_mlme_ap_tx_status(struct skw_iface *iface, u64 cookie,
			   const u8 *frame, int frame_len, u16 ack)
{
	u16 fc;
	int reassoc = 0;
	struct skw_client *client;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)frame;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "iface: %d, da: %pM, ack: %d, cookie: %lld\n",
		iface->id, mgmt->da, ack, cookie);

	if (!iface->sap.sme_external) {
		client = skw_mlme_ap_get_client(iface, mgmt->da);
		if (!client || client->cookie != cookie) {
			skw_chip_dbg(skw->idx, "cfg80211 tx status, cookie: %lld\n", cookie);
			goto report;
		}

		fc = SKW_MGMT_SFC(mgmt->frame_control);

		switch (fc) {
		case IEEE80211_STYPE_AUTH:
			skw_mlme_ap_auth_cb(iface, client, mgmt, frame_len, !!ack);
			break;

		case IEEE80211_STYPE_REASSOC_RESP:
			reassoc = 1;
			/* fall through */
			skw_fallthrough;
		case IEEE80211_STYPE_ASSOC_RESP:
			skw_mlme_ap_assoc_cb(iface, client, mgmt, frame_len,
						!!ack, reassoc);
			break;

		default:
			break;
		}

		return;
	}

report:
	cfg80211_mgmt_tx_status(&iface->wdev, cookie, frame, frame_len,
				ack, GFP_KERNEL);
}

static int skw_mlme_ap_auth_reply(struct skw_iface *iface,
		struct skw_client *client, const u8 *bssid,
		u16 auth_type, u16 transaction, u16 status,
		u8 *ie, int ie_len)
{
	int ret;
	int frame_len;
	struct wiphy *wiphy;
	struct ieee80211_mgmt *reply;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "da: %pM, bssid: %pM, transaction: %d, status: %d, ie: %d\n",
		client->addr, bssid, transaction, status, ie_len);

	wiphy = priv_to_wiphy(iface->skw);
	frame_len = SKW_IEEE80211_HDR_LEN +
		    sizeof(reply->u.auth) +
		    ie_len;

	reply = SKW_ZALLOC(frame_len, GFP_KERNEL);

	if (!reply)
		return -ENOMEM;

	reply->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					   IEEE80211_STYPE_AUTH);
	skw_ether_copy(reply->da, client->addr);
	skw_ether_copy(reply->sa, iface->addr);
	skw_ether_copy(reply->bssid, bssid);

	reply->u.auth.auth_alg = cpu_to_le16(auth_type);
	reply->u.auth.auth_transaction = cpu_to_le16(transaction);
	reply->u.auth.status_code = cpu_to_le16(status);

	if (ie && ie_len)
		memcpy(reply->u.auth.variable, ie, ie_len);

	// skw_hex_dump("auth_reply", reply, frame_len, false);
	ret = skw_mgmt_tx(wiphy, iface, iface->sap.cfg.channel,
			  0, &client->cookie, false, reply, frame_len,
			  frame_len, reply, true);

	SKW_KFREE(reply);

	return ret;
}

#if 0
static int skw_ap_auth_shared_key(struct skw_client *sta, u16 trans_action)
{
	u16 status;
	u8 *challenge;

	switch (trans_action) {
	case 1:
		sta->challenge = SKW_ZALLOC(WLAN_AUTH_CHALLENGE_LEN,
				GFP_KERNEL);
		if (IS_ERR(sta->challenge)) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto reply;
		}

		/* Generate challenge text */
		get_random_bytes(sta->challenge,
				WLAN_AUTH_CHALLENGE_LEN);

		status = WLAN_STATUS_SUCCESS;
		break;

	case 3:
		challenge = &mgmt->u.auth.variable[2];

		if (memcmp(sta->challenge, challenge,
			   WLAN_AUTH_CHALLENGE_LEN) == 0) {
			status = WLAN_STATUS_SUCCESS;
			SKW_KFREE(sta->challenge);
		} else {
			status = WLAN_STATUS_CHALLENGE_FAIL;
		}

		break;

	default:
		status = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
		break;
	}

	return status;
}
#endif

static int skw_mlme_ap_auth_handler(struct skw_iface *iface, int freq,
				int signal, void *frame, int frame_len)
{
	u8 *ies = NULL, *challenge;
	int ies_len = 0, ret = 0;
	struct skw_client *client = NULL;
	struct station_parameters sta_params;
	u8 challenge_ies[WLAN_AUTH_CHALLENGE_LEN + 2];
	struct wiphy *wiphy = priv_to_wiphy(iface->skw);
	struct ieee80211_mgmt *mgmt = frame;
	u16 auth_alg, status_code, trans_action;
	u16 status = WLAN_STATUS_SUCCESS;
	u16 seq_ctrl;
	struct skw_core *skw = iface->skw;

	auth_alg = le16_to_cpu(mgmt->u.auth.auth_alg);
	trans_action = le16_to_cpu(mgmt->u.auth.auth_transaction);
	status_code = le16_to_cpu(mgmt->u.auth.status_code);
	seq_ctrl = le16_to_cpu(mgmt->seq_ctrl);

	skw_chip_dbg(skw->idx, "auth alg: %d, trans action: %d, status: %d, seq: %u\n",
		auth_alg, trans_action, status_code, seq_ctrl);

	client = skw_mlme_ap_get_client(iface, mgmt->sa);
	if (client) {
		skw_chip_dbg(skw->idx, "flush peer status\n");
	} else {
		client = skw_mlme_ap_add_client(iface, mgmt->sa);
		if (!client) {
			skw_chip_err(skw->idx, "add client: %pM failed\n", mgmt->sa);
			return 0;
		}

		memset(&sta_params, 0x0, sizeof(sta_params));
		skw_add_station(wiphy, iface->ndev, mgmt->sa, &sta_params);
	}

	if (ieee80211_has_retry(mgmt->frame_control) &&
	    client->last_seq_ctrl == seq_ctrl) {
		skw_chip_dbg(skw->idx, "drop repeated auth(seq: %d)\n", seq_ctrl);
		return 0;
	}

	client->last_seq_ctrl = seq_ctrl;
	client->state = SKW_STATE_AUTHED;

	if (!ether_addr_equal(mgmt->bssid, iface->sap.cfg.bssid)) {
		skw_chip_warn(skw->idx, "failed, ap bssid: %pM, rx bssid: %pM\n",
			 iface->sap.cfg.bssid, mgmt->bssid);
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto reply;
	}

	// TODO:
	// transation check

#if 0
	if (auth_alg != iface->sap.auth_type) {
		skw_chip_err(skw->idx, "auth type not match (client: %d, ap: %d)\n",
			auth_alg, iface->sap.auth_type);
		status = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto reply;
	}

	if (client->state !=  SKW_STATE_NONE) {
		skw_chip_warn(skw->idx, "current state: %s\n", sm_str[client->state]);
		return 0;
	}

#endif

	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		if (trans_action != 1) {
			status = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
			goto reply;
		}

		if (status_code != WLAN_STATUS_SUCCESS)
			return 0;

		status = WLAN_STATUS_SUCCESS;
		client->last_seq_ctrl = seq_ctrl;

		break;

	case WLAN_AUTH_SAE:
		if (!skw_compat_cfg80211_rx_mgmt(&iface->wdev, freq, signal,
				      frame, frame_len, 0, GFP_ATOMIC)) {
			skw_chip_warn(skw->idx, "cfg80211_rx_mgmt failed\n");
		}

		return 0;

	case WLAN_AUTH_SHARED_KEY:
		switch (trans_action) {
		case 1:
			client->challenge = SKW_ZALLOC(WLAN_AUTH_CHALLENGE_LEN, GFP_KERNEL);
			if (!client->challenge) {
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}

			/* Generate challenge text */
			get_random_bytes(client->challenge,
					WLAN_AUTH_CHALLENGE_LEN);

			challenge_ies[0] = WLAN_EID_CHALLENGE;
			challenge_ies[1] = WLAN_AUTH_CHALLENGE_LEN;
			memcpy(challenge_ies + 2, client->challenge,
					WLAN_AUTH_CHALLENGE_LEN);
			ies_len = 2 + WLAN_AUTH_CHALLENGE_LEN;

			ies = challenge_ies;
			status = WLAN_STATUS_SUCCESS;
			break;

		case 3:
			challenge = &mgmt->u.auth.variable[2];

			if (client->challenge &&
				(memcmp(client->challenge, challenge,
				   WLAN_AUTH_CHALLENGE_LEN) == 0))
				status = WLAN_STATUS_SUCCESS;
			else
				status = WLAN_STATUS_CHALLENGE_FAIL;

			break;

		default:
			status = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
			break;
		}


		break;

	default:
		status = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		skw_chip_warn(skw->idx, "unsupport auth alg: %d\n", auth_alg);
		break;
	}

reply:
	ret = skw_mlme_ap_auth_reply(iface, client, mgmt->bssid, auth_alg,
				trans_action + 1, status, ies, ies_len);
	if (ret || status != WLAN_STATUS_SUCCESS) {
		skw_chip_warn(skw->idx, "failed, ret = %d, status: %d\n", ret, status);
		client->state = SKW_STATE_NONE;
		skw_mlme_ap_del_sta(wiphy, iface->ndev, mgmt->sa, false);
	}

	return 0;
}

#if 0
static int skw_ap_parse_element(struct skw_80211_element *element,
				const u8 *ies, u32 ie_len)
{
	const struct element *elem;

	return 0;

	for_each_element(elem, ies, ie_len) {
		switch (elem->id) {
		case WLAN_EID_SSID:
			// element->ssid = elem->data;
			// element->ssid_len = elem->data;
			break;

		case WLAN_EID_SUPP_RATES:
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			break;
		case WLAN_EID_RSN:
			break;
		case WLAN_EID_PWR_CAPABILITY:
			break;
		case WLAN_EID_SUPPORTED_CHANNELS:
			break;
		case WLAN_EID_HT_CAPABILITY:
			break;
		case WLAN_EID_HT_OPERATION:
			break;
		case WLAN_EID_VHT_CAPABILITY:
			break;
		case WLAN_EID_VHT_OPERATION:
			break;
		case WLAN_EID_EXT_CAPABILITY:
			break;
		case WLAN_EID_MIC:
			break;
		case WLAN_EID_SUPPORTED_REGULATORY_CLASSES:
			break;
		default:
			break;
		}
	}

	return 0;
}

static u16 skw_ap_check_ssid(struct skw_iface *iface,
			     const u8 *ssid, int ssid_len)
{
	if (!ssid ||
	    ssid_len != iface->sap.ssid_len ||
	    memcmp(ssid, iface->sap.ssid, iface->sap.ssid_len) != 0)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	return WLAN_STATUS_SUCCESS;
}

static u16 skw_ap_check_wmm(struct skw_client *sta, const u8 *wmm_ie, int len)
{
#define SKW_WMM_IE_LEN  24
	struct skw_wmm_info {
		u8 oui[3];
		u8 oui_type;
		u8 oui_subtype;
		u8 version;
		u8 qos_info;
	} __packed;

	struct skw_wmm_info *wmm = (struct skw_wmm_info *)wmm_ie;

	if (len != SKW_WMM_IE_LEN ||
	    wmm->oui_subtype != 0 ||
	    wmm->version != 1)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	return WLAN_STATUS_SUCCESS;
}
#endif
static u16 skw_mlme_ap_check_assoc_ie(struct skw_iface *iface,
				 struct skw_client *client,
				 const u8 *ie, int ie_len)
{
	// skw_hex_dump("rx assoc ie", ie, ie_len, false);
	// struct skw_80211_element e;

	//memset(&ie, 0x0, sizeof(e));
	// skw_ap_parse_element(&e, ie, ie_len);

#if 0
	/* check ssid */
	if (skw_ap_check_ssid(iface, e.ssid, e.ssid_len))
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* check wmm */
	if (skw_ap_check_wmm(sta, e.wmm, e.wmm_len))
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* check ext capa */
	/* check support rate */
#endif
	return 0;
}

static u16 skw_mlme_ap_new_aid(struct skw_iface *iface)
{
	u16 aid = 0;

	for (aid = 1; aid < 64; aid++)
		if (!test_and_set_bit(aid, iface->sap.aid_map))
			break;

	return aid;
}

/* add basic rate & ext support rate */
static u8 *skw_mlme_ap_add_rate(struct wiphy *wiphy,
			   struct skw_iface *iface, u8 *ies)
{
	int i, nr;
	u8 *pos = ies, *ext_rate_count;
	struct ieee80211_rate *rate;
	struct ieee80211_supported_band *sband;

	/* basic rate */
	sband = wiphy->bands[iface->sap.cfg.channel->band];
	rate = sband->bitrates;
#if 0
	enum ieee80211_rate_flags mandatory;

	if (sband->band == NL80211_BAND_2GHZ) {
		if (scan_width == NL80211_BSS_CHAN_WIDTH_5 ||
		    scan_width == NL80211_BSS_CHAN_WIDTH_10)
			mandatory = IEEE80211_RATE_MANDATORY_G;
		else
			mandatory = IEEE80211_RATE_MANDATORY_B;
	} else {
		mandatory = IEEE80211_RATE_MANDATORY_A;
	}
#endif
	*pos++ = WLAN_EID_SUPP_RATES;
	*pos++ = SKW_BASIC_RATE_COUNT;
	for (i = 0; i < SKW_BASIC_RATE_COUNT; i++)
		*pos++ = rate[i].bitrate / 5;

	/* ext support rate */
	*pos++ = WLAN_EID_EXT_SUPP_RATES;
	ext_rate_count = pos++;

	for (i = SKW_BASIC_RATE_COUNT; i < sband->n_bitrates; i++)
		*pos++ = rate[i].bitrate / 5;

	nr = sband->n_bitrates - SKW_BASIC_RATE_COUNT;
	if (iface->sap.ht_required) {
		*pos++ = 0x80 | SKW_BSS_MEMBERSHIP_SELECTOR_HT_PHY;
		nr++;
	}

	if (iface->sap.vht_required) {
		*pos++ = 0x80 | SKW_BSS_MEMBERSHIP_SELECTOR_VHT_PHY;
		nr++;
	}

	*ext_rate_count = nr;

	return pos;
}

static u8 *skw_mlme_ap_add_ht_cap(struct skw_iface *iface, u8 *ies)
{
	u8 *pos = ies;
	int len = sizeof(struct ieee80211_ht_cap);

	if(iface->sap.cfg.ht_cap) {
		*pos++ = WLAN_EID_HT_CAPABILITY;
		*pos++ = len;
		memcpy(pos, iface->sap.cfg.ht_cap, len);
		return pos + len;
	} else {
		return pos;
	}
}

static u8 *skw_mlme_ap_add_ht_oper(struct skw_iface *iface, u8 *ies)
{
	u8 *pos = ies;
	struct ieee80211_ht_operation *oper;

	*pos++ = WLAN_EID_HT_OPERATION;
	*pos++ = sizeof(*oper);

	oper = (struct ieee80211_ht_operation *)pos;
	memset(oper, 0x0, sizeof(*oper));

	oper->primary_chan = iface->sap.cfg.channel->hw_value;

	return pos + sizeof(*oper);
}

static void skw_mlme_ap_parse_ies(struct skw_core *skw, const u8 *beacon, int beacon_len,
				 struct skw_element_info *e)
{
	const struct skw_element *element;

	if (!beacon || beacon_len == 0)
		return;

	skw_foreach_element(element, beacon, beacon_len) {
		switch (element->id) {
		case WLAN_EID_SSID:
			e->ssid.len = element->datalen;
			memcpy(e->ssid.data, element->data, element->datalen);
			break;

		case WLAN_EID_SUPP_RATES:
			e->support_rate = element;
			break;

		case WLAN_EID_EXT_SUPP_RATES:
			e->ext_rate = element;
			break;

		case WLAN_EID_HT_CAPABILITY:
			e->ht_capa = element;
			break;

		case WLAN_EID_HT_OPERATION:
			e->ht_oper = element;
			break;

		case WLAN_EID_VHT_CAPABILITY:
			e->vht_capa = element;
			break;

		case WLAN_EID_VHT_OPERATION:
			e->vht_oper = element;
			break;

		case WLAN_EID_EXT_CAPABILITY:
			e->ext_capa = element;
			break;

		case WLAN_EID_VENDOR_SPECIFIC:
			e->vendor_vht = element;
			break;

		default:
			skw_chip_dbg(skw->idx, "unused element: %d, len: %d\n",
				element->id, element->datalen);
			break;
		}
	}
}

static int skw_mlme_ap_assoc_reply(struct skw_iface *iface,
		struct skw_client *client, u16 status, bool reassoc)
{
	u16 fc, elen;
	u8 *ies, *pos;
	int ret, frame_len, len;
	struct wiphy *wiphy = priv_to_wiphy(iface->skw);
	struct ieee80211_mgmt *reply;
	struct skw_element_info e;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "client addr: %pM, reassoc: %d, aid: %d, status code: %d\n",
		client->addr, reassoc, client->aid, status);

	len = sizeof(struct ieee80211_mgmt) + 1024;

	reply = SKW_ZALLOC(len, GFP_KERNEL);
	if (!reply)
		return -ENOMEM;

	memset(&e, 0x0, sizeof(e));

	fc = reassoc ? IEEE80211_STYPE_REASSOC_RESP :
		IEEE80211_STYPE_ASSOC_RESP;

	reply->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | fc);
	reply->duration = 0;
	skw_ether_copy(reply->da, client->addr);
	skw_ether_copy(reply->sa, iface->addr);
	skw_ether_copy(reply->bssid, iface->sap.cfg.bssid);
	reply->seq_ctrl = 0;

	reply->u.assoc_resp.capab_info = client->capa;
	reply->u.assoc_resp.status_code = status;
	reply->u.assoc_resp.aid = client->aid;

	frame_len = SKW_IEEE80211_HDR_LEN +
		    sizeof(reply->u.assoc_resp);

	pos = ies = reply->u.assoc_resp.variable;

	len = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	skw_mlme_ap_parse_ies(skw, iface->sap.probe_resp + len,
			      iface->sap.probe_resp_len - len, &e);

	/* support rate & ext rate */

	if (e.support_rate) {
		elen = e.support_rate->datalen + 2;
		memcpy(pos, e.support_rate, elen);
		pos += elen;

		if (e.ext_rate) {
			elen = e.ext_rate->datalen + 2;
			memcpy(pos, e.ext_rate, elen);
			pos += elen;
		}
	} else {
		pos = skw_mlme_ap_add_rate(wiphy, iface, pos);
	}

#if 1
	/* 80211n capa & oper */
	if (e.ht_capa && e.ht_oper) {
		elen = e.ht_capa->datalen + 2;
		memcpy(pos, e.ht_capa, elen);
		pos += elen;

		elen = e.ht_oper->datalen + 2;
		memcpy(pos, e.ht_oper, elen);
		pos += elen;
	} else {
		pos = skw_mlme_ap_add_ht_cap(iface, pos);
		pos = skw_mlme_ap_add_ht_oper(iface, pos);
	}

	/* 11ac capa */

	/* vendor vht */
	if (e.vendor_vht) {
		elen = e.vendor_vht->datalen + 2;
		memcpy(pos, e.vendor_vht, elen);
		pos += elen;
	}
#endif

	frame_len += pos - ies;

	ret = skw_mgmt_tx(wiphy, iface, iface->sap.cfg.channel,
			  0, &client->cookie, false, reply, frame_len,
			  frame_len, reply, true);

	SKW_KFREE(reply);

	return ret;
}

static int skw_mlme_ap_assoc_handler(struct skw_iface *iface, void *frame,
				int frame_len, int reassoc)
{
	u8 *ie;
	int ie_len = 0, ret;
	u16 capab_info, status;
	struct skw_client *client;
	struct ieee80211_mgmt *mgmt = frame;
	u16 seq_ctrl;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "iface: %d, sa: %pM, reassoc: %d\n",
		iface->id, mgmt->sa, reassoc);

	seq_ctrl = le16_to_cpu(mgmt->seq_ctrl);

	client = skw_mlme_ap_get_client(iface, mgmt->sa);
	if (!client) {
		skw_chip_warn(skw->idx, "client: %pM not exist\n", mgmt->sa);
		return 0;
	}

	if (client->state == SKW_STATE_NONE) {
		status = WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA;
		skw_chip_dbg(skw->idx, "client->state: %d\n", client->state);
		skw_ap_send_disassoc(iface, client, status);
		return 0;
	}

	if (ieee80211_has_retry(mgmt->frame_control) &&
	    client->last_seq_ctrl == seq_ctrl) {
		skw_chip_dbg(skw->idx, "drop repeated assoc frame(seq: %d)\n", seq_ctrl);
		return 0;
	}

	client->last_seq_ctrl = seq_ctrl;

	ie_len = frame_len - sizeof(struct ieee80211_hdr_3addr);
	if (reassoc) {
		capab_info = le16_to_cpu(mgmt->u.reassoc_req.capab_info);
		ie = mgmt->u.reassoc_req.variable;
		ie_len -= sizeof(mgmt->u.reassoc_req);
	} else {
		capab_info = le16_to_cpu(mgmt->u.assoc_req.capab_info);
		ie = mgmt->u.assoc_req.variable;
		ie_len -= sizeof(mgmt->u.assoc_req);
	}

	client->capa = capab_info;

	// check assoc ies
	status = skw_mlme_ap_check_assoc_ie(iface, client, ie, ie_len);
	if (status != WLAN_STATUS_SUCCESS) {
		skw_ap_send_disassoc(iface, client, status);
		return 0;
	}

	// assign aid
	client->aid = skw_mlme_ap_new_aid(iface);
	if (!client->aid) {
		status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto reply;
	}

	// 11W
reply:
	ret = skw_mlme_ap_assoc_reply(iface, client, status, reassoc);
	if (ret || status != WLAN_STATUS_SUCCESS) {
		skw_chip_err(skw->idx, "ret: %d, status: %d\n", ret, status);
		return ret;
	}

	if (!client->assoc_req_ie) {
		client->assoc_req_ie = SKW_ZALLOC(ie_len, GFP_KERNEL);
		if (client->assoc_req_ie) {
			memcpy(client->assoc_req_ie, ie, ie_len);
			client->assoc_req_ie_len = ie_len;
		}
	}

	return ret;
}

int skw_mlme_ap_rx_mgmt(struct skw_iface *iface, u16 fc, int freq,
			int signal, void *frame, int frame_len)
{
	int reassoc = 0;
	struct skw_client *client;
	struct ieee80211_mgmt *mgmt = frame;
	struct skw_core *skw = iface->skw;

	// address check
	switch (fc) {
	case IEEE80211_STYPE_AUTH:
		skw_mlme_ap_auth_handler(iface, freq, signal, frame, frame_len);
		break;

	case IEEE80211_STYPE_DEAUTH:
	case IEEE80211_STYPE_DISASSOC:
		client = skw_mlme_ap_get_client(iface, mgmt->sa);
		if (!client) {
			skw_chip_warn(skw->idx, "can't find sta:%pM\n", mgmt->sa);
			return 0;
		}

		if (client->state >= SKW_STATE_ASSOCED) {
			//notify hostapd to update state and delete sta
			cfg80211_del_sta(iface->ndev, client->addr, GFP_KERNEL);
		} else if (client->state >= SKW_STATE_AUTHED) {
			//just delete local sta info
			skw_mlme_ap_del_sta(iface->wdev.wiphy,
					    iface->ndev, client->addr, false);
		}
#if 0
		skw_add_timer_work("idle_release", skw_mlme_ap_auth_timeout,
				   client, SKW_AP_IDLE_TIMEOUT,
				   client, GFP_KERNEL);
#endif
		break;

	case IEEE80211_STYPE_REASSOC_REQ:
		reassoc = 1;
		/* fall through */
		skw_fallthrough;
	case IEEE80211_STYPE_ASSOC_REQ:
		skw_mlme_ap_assoc_handler(iface, frame, frame_len, reassoc);
		break;

	case IEEE80211_STYPE_PROBE_REQ:
		skw_fallthrough;
	case IEEE80211_STYPE_PROBE_RESP:
		skw_fallthrough;
	case IEEE80211_STYPE_ACTION:
		if (!skw_compat_cfg80211_rx_mgmt(&iface->wdev, freq, signal,
						frame, frame_len, 0, GFP_ATOMIC)) {
			skw_chip_warn(skw->idx, "mlme_ap_rx failed\n");
		}
		break;

	default:
		skw_chip_warn(skw->idx, "unsupport fc type: 0x%x\n", fc);
		break;
	}

	return 0;
}

#if 0
int skw_send_deauth_frame(struct wiphy *wiphy, struct net_device *netdev,
			int reason_code)
{
	int ret;
	int size;
	char *buff = NULL;
	struct skw_core *skw;
	struct skw_disconnect_param *deauth_param = NULL;

	skw = wiphy_priv(wiphy);

	size = sizeof(struct skw_disconnect_param);
	buff = SKW_ZALLOC(size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(buff)) {
		skw_chip_err(skw->idx, "Malloc disconnect param for deauth failed\n");
		return -ENOMEM;
	}

	deauth_param = (struct skw_disconnect_param *)buff;
	deauth_param->type = SKW_DISCONNECT_SEND_DEAUTH;
	deauth_param->local_state_change = true;
	deauth_param->reason_code = reason_code;
	deauth_param->ie_len = 0;

	ret = skw_send_msg(wiphy, netdev, SKW_CMD_DISCONNECT, buff,
			size, NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "Deauth failed ret:%d\n", ret);

	SKW_KFREE(buff);

	return ret;
}
#endif

static int skw_mlme_sta_ft_event(struct skw_iface *iface, void *buf, int len)
{
	int ie_len;
	struct cfg80211_ft_event_params ft_event;
	struct ieee80211_mgmt *mgmt = buf;

	ie_len = len - offsetof(struct ieee80211_mgmt, u.auth.variable);

	ft_event.ies = mgmt->u.auth.variable;
	ft_event.ies_len = ie_len;
	ft_event.target_ap = mgmt->bssid;
	ft_event.ric_ies = NULL;
	ft_event.ric_ies_len = 0;

	cfg80211_ft_event(iface->ndev, &ft_event);

	return 0;
}

int skw_mlme_sta_rx_auth(struct skw_iface *iface, int freq, int signal,
			 void *buf, int len)
{
	u16 status_code;
	struct ieee80211_mgmt *mgmt = buf;
	struct wiphy *wiphy = iface->wdev.wiphy;
	struct skw_connect_param *conn = iface->sta.conn;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "auth_type: %d, flags: 0x%x\n",
		conn->auth_type, conn->flags);

	conn->state = SKW_STATE_AUTHED;

	if (conn->auth_type == NL80211_AUTHTYPE_SAE)
		return skw_compat_cfg80211_rx_mgmt(&iface->wdev, freq, signal,
						buf, len, 0, GFP_ATOMIC);

	if (conn->auth_type == NL80211_AUTHTYPE_FT)
		return skw_mlme_sta_ft_event(iface, mgmt, len);

	status_code = le16_to_cpu(mgmt->u.auth.status_code);
	if (status_code == WLAN_STATUS_SUCCESS)
		return skw_connect_assoc(wiphy, iface->ndev, conn);

	if (SKW_TEST(conn->flags, SKW_CONN_FLAG_AUTH_AUTO) &&
	    status_code == WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG) {

		mutex_lock(&iface->sta.conn->lock);

		switch (conn->auth_type) {
		case NL80211_AUTHTYPE_OPEN_SYSTEM:
			if (conn->key_len)
				conn->auth_type = NL80211_AUTHTYPE_SHARED_KEY;
			else
				conn->auth_type = NL80211_AUTHTYPE_FT;
			break;

		case NL80211_AUTHTYPE_SHARED_KEY:
			conn->auth_type = NL80211_AUTHTYPE_FT;
			break;

		default:
			SKW_CLEAR(conn->flags, SKW_CONN_FLAG_AUTH_AUTO);
			break;
		}

		mutex_unlock(&iface->sta.conn->lock);

		if (conn->flags & SKW_CONN_FLAG_AUTH_AUTO) {
			return skw_queue_local_event(wiphy, iface,
					SKW_EVENT_LOCAL_STA_CONNECT,
					NULL, 0);
		}
	}

	/* status code != WLAN_STATUS_SUCCESS */
	conn->state = SKW_STATE_NONE;

	return 0;
}

int skw_mlme_sta_rx_assoc(struct skw_iface *iface, struct cfg80211_bss *bss,
			  void *frame, int len, void *req_ie, int req_ie_len)
{
	u16 status;
	int resp_ie_len;
	struct ieee80211_mgmt *mgmt = frame;
	struct skw_connect_param *conn = iface->sta.conn;
	struct skw_core *skw = iface->skw;

	skw_chip_dbg(skw->idx, "bssid: %pM\n", mgmt->bssid);

	resp_ie_len = offsetof(struct ieee80211_mgmt, u.assoc_resp.variable);
	resp_ie_len = len - resp_ie_len;

	status = le16_to_cpu(mgmt->u.assoc_resp.status_code);
	if (status == WLAN_STATUS_SUCCESS) {
		mutex_lock(&conn->lock);
		conn->state = SKW_STATE_ASSOCED;
		skw_connected(iface->ndev, conn, req_ie, req_ie_len,
			      mgmt->u.assoc_resp.variable, resp_ie_len,
			      status, GFP_KERNEL);
		mutex_unlock(&conn->lock);
	} else {
		conn->state = SKW_STATE_NONE;
		skw_disconnected(iface->ndev, status,
				mgmt->u.assoc_resp.variable, resp_ie_len,
				false, GFP_KERNEL);
	}

	return 0;
}
