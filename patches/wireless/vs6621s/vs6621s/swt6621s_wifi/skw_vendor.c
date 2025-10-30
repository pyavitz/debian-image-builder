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

#include <net/cfg80211.h>
#include <net/genetlink.h>

#include "skw_vendor.h"
#include "skw_cfg80211.h"
#include "skw_core.h"
#include "skw_iface.h"
#include "skw_util.h"
#include "skw_regd.h"
#include "version.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
const struct nla_policy
skw_set_country_policy[SKW_SET_COUNTRY_RULES] = {
	[SKW_ATTR_SET_COUNTRY] = {.type = NLA_STRING},
};

const struct nla_policy
skw_get_valid_channels_policy[SKW_GET_VALID_CHANNELS_RULES] = {
	[SKW_ATTR_GET_VALID_CHANNELS] = {.type = NLA_U32},
};

const struct nla_policy
skw_get_version_policy[SKW_GET_VERSION_RULES] = {
	[SKW_ATTR_VERSION_DRIVER] = {.type = NLA_U32},
	[SKW_ATTR_VERSION_FIRMWARE] = {.type = NLA_U32},
};

#if 0
static int skw_vendor_dbg_reset_logging(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = SKW_OK;

	skw_dbg("Enter\n");

	return ret;
}

static int skw_vendor_set_p2p_rand_mac(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	int type;
	//struct skw_iface *iface = netdev_priv(wdev->netdev);
	u8 mac_addr[6] = {0};

	skw_dbg("set skw mac addr\n");
	type = nla_type(data);

	if (type == SKW_ATTR_DRIVER_RAND_MAC) {
		memcpy(mac_addr, nla_data(data), 6);
		skw_dbg("mac:%pM\n", mac_addr);
	}

	return 0;
}

static int skw_vendor_set_rand_mac_oui(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	char *oui = nla_data(data);
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);

	if (!oui || (nla_len(data) != DOT11_OUI_LEN))
		return -EINVAL;

	skw_dbg("%02x:%02x:%02x\n", oui[0], oui[1], oui[2]);

	memcpy(iface->rand_mac_oui, oui, DOT11_OUI_LEN);

	return 0;
}
#endif

static int skw_vendor_cmd_reply(struct wiphy *wiphy, const void *data, int len)
{
	struct sk_buff *skb;
	int err;
	struct skw_core *skw = wiphy_priv(wiphy);

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (unlikely(!skb)) {
		skw_chip_err(skw->idx, "skb alloc failed");
		return -ENOMEM;
	}

	/* Push the data to the skb */
	err = nla_put_nohdr(skb, len, data);
	if (err) {
		kfree_skb(skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int skw_vendor_start_logging(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_get_wake_reason_stats(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_get_apf_capabilities(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, SKW_ATTR_APF_VERSION, 4) ||
	    nla_put_u32(skb, SKW_ATTR_APF_MAX_LEN, 1024)) {
		kfree_skb(skb);
		return -ENOMEM;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int skw_vendor_get_ring_buffer_data(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_get_firmware_dump(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_select_tx_power_scenario(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_set_latency_mode(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "inst: %d\n", SKW_WDEV_TO_IFACE(wdev)->id);

	return 0;
}

static int skw_vendor_get_feature_set(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	u32 feature_set = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	/* Hardcoding these values for now, need to get
	 * these values from FW, will change in a later check-in
	 */
	feature_set |= WIFI_FEATURE_INFRA;
	feature_set |= WIFI_FEATURE_INFRA_5G;
	feature_set |= WIFI_FEATURE_P2P;
	feature_set |= WIFI_FEATURE_SOFT_AP;
	feature_set |= WIFI_FEATURE_AP_STA;
	//feature_set |= WIFI_FEATURE_TDLS;
	//feature_set |= WIFI_FEATURE_TDLS_OFFCHANNEL;
	//feature_set |= WIFI_FEATURE_NAN;
	//feature_set |= WIFI_FEATURE_HOTSPOT;
	//feature_set |= WIFI_FEATURE_LINK_LAYER_STATS; //TBC
	//feature_set |= WIFI_FEATURE_RSSI_MONITOR; //TBC with roaming
	//feature_set |= WIFI_FEATURE_MKEEP_ALIVE; //TBC compare with QUALCOM
	//feature_set |= WIFI_FEATURE_CONFIG_NDO; //TBC
	//feature_set |= WIFI_FEATURE_SCAN_RAND;
	//feature_set |= WIFI_FEATURE_RAND_MAC;
	//feature_set |= WIFI_FEATURE_P2P_RAND_MAC ;
	//feature_set |= WIFI_FEATURE_CONTROL_ROAMING;

	skw_chip_dbg(skw->idx, "feature: 0x%x\n", feature_set);

	return skw_vendor_cmd_reply(wiphy, &feature_set, sizeof(u32));
}

static int skw_vendor_set_country(struct wiphy *wiphy, struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	char *country = nla_data(data);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (nla_type(data) != SKW_ATTR_SET_COUNTRY)
		skw_chip_warn(skw->idx, "attr mismatch, type: %d\n", nla_type(data));

	if (!country || strlen(country) != 2) {
		skw_chip_err(skw->idx, "invalid, country: %s\n", country ? country : "null");

		return -EINVAL;
	}

	skw_chip_dbg(skw->idx, "country: %c%c\n", country[0], country[1]);

	return skw_set_regdom(wiphy, country);
}

static int skw_vendor_get_version(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int len)
{
	char version[64] = {0};
	struct skw_core *skw = wiphy_priv(wiphy);

	switch (nla_type(data)) {
	case SKW_ATTR_VERSION_DRIVER:
		strncpy(version, SKW_VERSION, sizeof(version));
		break;

	case SKW_ATTR_VERSION_FIRMWARE:
		snprintf(version, sizeof(version), "%s-%s",
			 skw->fw.plat_ver, skw->fw.wifi_ver);
		break;

	default:
		skw_chip_err(skw->idx, "invalid nla type\n");
		strcpy(version, "invalid");
		break;
	}

	return skw_vendor_cmd_reply(wiphy, version, sizeof(version));
}

static int skw_vendor_get_usable_channels(struct wiphy *wiphy,
			struct wireless_dev *wdev, const void *data, int len)
{
	int i, nr, max;
	struct sk_buff *skb;
	enum nl80211_band band;
	struct skw_usable_chan *chans;
	struct skw_usable_chan_req *req = (struct skw_usable_chan_req *)data;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "band_mask: 0x%x\n", req->band_mask);

	max = ieee80211_get_num_supported_channels(wiphy);

	chans = SKW_ZALLOC(max * sizeof(*chans), GFP_KERNEL);
	if (!chans)
		return -ENOMEM;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb) {
		SKW_KFREE(chans);
		return -ENOMEM;
	}

	for (nr = 0, band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(req->band_mask & BIT(to_skw_band(band))) ||
		    !wiphy->bands[band])
			continue;

		for (i = 0; i < wiphy->bands[band]->n_channels; i++) {
			struct ieee80211_channel *chan;

			chan = &wiphy->bands[band]->channels[i];

			if (chan->flags & IEEE80211_CHAN_DISABLED)
				continue;

			chans[nr].center_freq = chan->center_freq;
			chans[nr].band_width = SKW_CHAN_WIDTH_20;
			chans[nr].iface_mode_mask = BIT(SKW_STA_MODE) |
						    BIT(SKW_AP_MODE) |
						    BIT(SKW_GC_MODE) |
						    BIT(SKW_GO_MODE);
			nr++;
		}
	}

	if (nla_put_nohdr(skb, nr * sizeof(*chans), chans)) {
		SKW_KFREE(chans);
		kfree_skb(skb);

		return -ENOMEM;
	}

	SKW_KFREE(chans);

	return cfg80211_vendor_cmd_reply(skb);
}

static int skw_vendor_get_valid_channels(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	int channels[32], size;
	int i, band, nr_channels;
	struct sk_buff *skb;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (nla_type(data) != SKW_ATTR_GET_VALID_CHANNELS)
		skw_chip_warn(skw->idx, "attr mismatch, type: %d", nla_type(data));

	band = nla_get_u32(data);
	if (band > NL80211_BAND_5GHZ) {
		skw_chip_err(skw->idx, "invalid band: %d\n", band);
		return -EINVAL;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	nr_channels = wiphy->bands[band]->n_channels;
	size = nr_channels * sizeof(int);

	for (i = 0; i < nr_channels; i++)
		channels[i] = wiphy->bands[band]->channels[i].hw_value;

	if (nla_put_u32(skb, SKW_ATTR_VALID_CHANNELS_COUNT, nr_channels) ||
	    nla_put(skb, SKW_ATTR_VALID_CHANNELS_LIST, size, channels)) {
		kfree_skb(skb);

		return -ENOMEM;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int skw_vendor_get_ring_buffers_status(struct wiphy *wiphy,
			struct wireless_dev *wdev, const void  *data, int len)
{
	struct sk_buff *skb;
	struct skw_ring_buff_status status = {
		.name = "skw_drv",
		.flags = 0,
		.ring_id = 0,
		.ring_buffer_byte_size = 1024,
		.verbose_level = 0,
		.written_bytes = 0,
		.read_bytes = 0,
		.written_records = 0,
	};

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, SKW_ATTR_RING_BUFFERS_COUNT, 1) ||
	    nla_put(skb, SKW_ATTR_RING_BUFFERS_STATUS, sizeof(status), &status)) {
		kfree_skb(skb);

		return -ENOMEM;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int skw_vendor_get_logger_feature(struct wiphy *wiphy,
			struct wireless_dev *wdev, const void  *data, int len)
{
	u32 features = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "features: 0x%x\n", features);

	return skw_vendor_cmd_reply(wiphy, &features, sizeof(features));
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
#define SKW_VENDOR_CMD(oui, cmd, flag, func, nla_policy, max_attr)  \
{                                                                   \
	.info = {.vendor_id = oui, .subcmd = cmd},                  \
	.flags = flag,                                              \
	.doit = func,                                               \
	.policy = nla_policy,                                       \
	.maxattr = max_attr,                                        \
}
#else
#define SKW_VENDOR_CMD(oui, cmd, flag, func, nla_policy, max_attr)  \
{                                                                   \
	.info = {.vendor_id = oui, .subcmd = cmd},                  \
	.flags = flag,                                              \
	.doit = func,                                               \
}
#endif

#define SKW_VENDOR_DEFAULT_FLAGS (WIPHY_VENDOR_CMD_NEED_WDEV |      \
				  WIPHY_VENDOR_CMD_NEED_NETDEV)

static struct wiphy_vendor_command skw_vendor_cmds[] = {
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_VALID_CHANNELS,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_valid_channels,
			skw_get_valid_channels_policy,
			SKW_GET_VALID_CHANNELS_RULES),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_FEATURE_SET,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_feature_set,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_VERSION,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_version,
			skw_get_version_policy,
			SKW_GET_VERSION_RULES),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_RING_BUFFERS_STATUS,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_ring_buffers_status,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_LOGGER_FEATURE,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_logger_feature,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_APF_CAPABILITIES,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_apf_capabilities,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_USABLE_CHANS,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_usable_channels,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_SET_COUNTRY,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_set_country,
			skw_set_country_policy, SKW_SET_COUNTRY_RULES),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_START_LOGGING,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_start_logging,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_FIRMWARE_DUMP,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_firmware_dump,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_RING_BUFFER_DATA,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_ring_buffer_data,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_GET_WAKE_REASON_STATS,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_get_wake_reason_stats,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_SELECT_TX_POWER_SCENARIO,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_select_tx_power_scenario,
			VENDOR_CMD_RAW_DATA, 0),
	SKW_VENDOR_CMD(OUI_GOOGLE, SKW_VID_SET_LATENCY_MODE,
			SKW_VENDOR_DEFAULT_FLAGS,
			skw_vendor_set_latency_mode,
			VENDOR_CMD_RAW_DATA, 0),
};

static struct nl80211_vendor_cmd_info skw_vendor_events[] = {
	{
		.vendor_id = 0,
		.subcmd = 0,
	},
};

void skw_vendor_init(struct wiphy *wiphy)
{
	wiphy->vendor_commands = skw_vendor_cmds;
	wiphy->n_vendor_commands = ARRAY_SIZE(skw_vendor_cmds);
	wiphy->vendor_events = skw_vendor_events;
	wiphy->n_vendor_events = ARRAY_SIZE(skw_vendor_events);
}

void skw_vendor_deinit(struct wiphy *wiphy)
{
	wiphy->vendor_commands = NULL;
	wiphy->n_vendor_commands = 0;
}

#endif
