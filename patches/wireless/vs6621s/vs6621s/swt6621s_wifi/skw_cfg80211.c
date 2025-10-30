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

#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <linux/if_tunnel.h>

#include "skw_core.h"
#include "skw_iface.h"
#include "skw_msg.h"
#include "skw_cfg80211.h"
#include "skw_regd.h"
#include "skw_mlme.h"
#include "skw_timer.h"
#include "skw_work.h"
#include "skw_tdls.h"
#include "skw_calib.h"
#include "skw_recovery.h"
#include "skw_dfs.h"
#include "skw_iw.h"

#define SKW_BIT_ULL(nr)        (1ULL << (nr))

static int ms_sus = 0;

int to_skw_bw(enum nl80211_chan_width bw)
{
	switch (bw) {
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		return SKW_CHAN_WIDTH_20;

	case NL80211_CHAN_WIDTH_40:
		return SKW_CHAN_WIDTH_40;

	case NL80211_CHAN_WIDTH_80:
		return SKW_CHAN_WIDTH_80;

	case NL80211_CHAN_WIDTH_80P80:
		return SKW_CHAN_WIDTH_80P80;

	case NL80211_CHAN_WIDTH_160:
		return SKW_CHAN_WIDTH_160;

	default:
		break;
	}

	return SKW_CHAN_WIDTH_MAX;
}

static int to_skw_gtk(u8 key_index)
{
	switch (key_index) {
	case 0 ... 3:
		return SKW_KEY_TYPE_GTK;
	case 4 ... 5:
		return SKW_KEY_TYPE_IGTK;
	case 6:
		return SKW_KEY_TYPE_BIGTK;
	default:
		break;
	}

	return SKW_KEY_TYPE_GTK;
}

static int to_skw_cipher_type(u32 cipher)
{
#define SKW_CASE_CIPHER_TYPE(c)                        \
	{                                              \
		case SKW_CIPHER_SUITE_##c:             \
			return SKW_CIPHER_TYPE_##c;    \
	}

	switch (cipher) {
	SKW_CASE_CIPHER_TYPE(WEP40);
	SKW_CASE_CIPHER_TYPE(WEP104);
	SKW_CASE_CIPHER_TYPE(SMS4);
	SKW_CASE_CIPHER_TYPE(TKIP);
	SKW_CASE_CIPHER_TYPE(CCMP);
	SKW_CASE_CIPHER_TYPE(CCMP_256);
	SKW_CASE_CIPHER_TYPE(AES_CMAC);
	SKW_CASE_CIPHER_TYPE(BIP_CMAC_256);
	SKW_CASE_CIPHER_TYPE(BIP_GMAC_128);
	SKW_CASE_CIPHER_TYPE(BIP_GMAC_256);
	SKW_CASE_CIPHER_TYPE(GCMP);
	SKW_CASE_CIPHER_TYPE(GCMP_256);

	default:
		break;
	}
#undef SKW_CASE_CIPHER_TYPE

	return SKW_CIPHER_TYPE_INVALID;
}

static const struct ieee80211_iface_limit skw_iface_limits[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_AP),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_GO) |
			 BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 10)
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_DEVICE),
	},
#endif
};

static const struct ieee80211_iface_limit skw_iface_limits_change[] = {
	{
		.max = 3,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_AP)     |
			 BIT(NL80211_IFTYPE_P2P_GO) |
			 BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 10)
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_DEVICE),
	},
#endif
};

static const struct ieee80211_iface_limit skw_iface_limits_aps[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION) |
			 BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
	{
		.max = 2,
		.types = BIT(NL80211_IFTYPE_AP),
	},
};

static const struct ieee80211_iface_limit skw_iface_limits_monitor[] = {
	{
		.max = 2,
		.types = BIT(NL80211_IFTYPE_MONITOR),
	},
};

#ifdef CONFIG_SWT6621S_DFS_MASTER
static const struct ieee80211_iface_limit skw_iface_limits_dfs[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_AP),
	},
};

static const struct ieee80211_iface_limit skw_iface_limits_dfs_change[] = {
	{
		.max = 2,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
};
#endif

static const struct ieee80211_iface_combination skw_iface_combos[] = {
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 10)
		.max_interfaces = 4,
#else
		.max_interfaces = 3,
#endif
		.num_different_channels = 2,
		.limits = skw_iface_limits,
		.n_limits = ARRAY_SIZE(skw_iface_limits),
	},
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 10)
		.max_interfaces = 4,
#else
		.max_interfaces = 3,
#endif
		.num_different_channels = 2,
		.limits = skw_iface_limits_change,
		.n_limits = ARRAY_SIZE(skw_iface_limits_change),
	},
	{
		.max_interfaces = 3,
		.num_different_channels = 1,
		.limits = skw_iface_limits_aps,
		.n_limits = ARRAY_SIZE(skw_iface_limits_aps),
	},
	{
		.max_interfaces = 2,
		.num_different_channels = 1,
		.limits = skw_iface_limits_monitor,
		.n_limits = ARRAY_SIZE(skw_iface_limits_monitor),
	},
#ifdef CONFIG_SWT6621S_DFS_MASTER
	{
		.max_interfaces = 2,
		.num_different_channels = 1,
		.limits = skw_iface_limits_dfs,
		.n_limits = ARRAY_SIZE(skw_iface_limits_dfs),
		.radar_detect_widths = BIT(NL80211_CHAN_WIDTH_20_NOHT) |
				       BIT(NL80211_CHAN_WIDTH_20) |
				       BIT(NL80211_CHAN_WIDTH_40) |
				       BIT(NL80211_CHAN_WIDTH_80),
	},
	{
		.max_interfaces = 2,
		.num_different_channels = 1,
		.limits = skw_iface_limits_dfs_change,
		.n_limits = ARRAY_SIZE(skw_iface_limits_dfs_change),
		.radar_detect_widths = BIT(NL80211_CHAN_WIDTH_20_NOHT) |
				       BIT(NL80211_CHAN_WIDTH_20) |
				       BIT(NL80211_CHAN_WIDTH_40) |
				       BIT(NL80211_CHAN_WIDTH_80),
	},
#endif
};

static const struct
ieee80211_txrx_stypes skw_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_ADHOC] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			BIT(IEEE80211_STYPE_AUTH >> 4) |
			BIT(IEEE80211_STYPE_DEAUTH >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
	},
	[NL80211_IFTYPE_STATION] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			BIT(IEEE80211_STYPE_AUTH >> 4),
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			BIT(IEEE80211_STYPE_AUTH >> 4) |
			BIT(IEEE80211_STYPE_DEAUTH >> 4) |
			BIT(IEEE80211_STYPE_ACTION >> 4),
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			BIT(IEEE80211_STYPE_AUTH >> 4) |
			BIT(IEEE80211_STYPE_DEAUTH >> 4) |
			BIT(IEEE80211_STYPE_ACTION >> 4),
	},
	[NL80211_IFTYPE_P2P_DEVICE] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
	},
};

#define SKW_CHAN2G(_channel, _freq, _flags) {		\
	.band			= NL80211_BAND_2GHZ,	\
	.center_freq		= (_freq),		\
	.hw_value		= (_channel),		\
	.flags			= (_flags),		\
	.max_antenna_gain	= 0,			\
	.max_power		= 30,			\
}

static struct ieee80211_channel skw_2ghz_chan[] = {
	SKW_CHAN2G(1, 2412, 0),
	SKW_CHAN2G(2, 2417, 0),
	SKW_CHAN2G(3, 2422, 0),
	SKW_CHAN2G(4, 2427, 0),
	SKW_CHAN2G(5, 2432, 0),
	SKW_CHAN2G(6, 2437, 0),
	SKW_CHAN2G(7, 2442, 0),
	SKW_CHAN2G(8, 2447, 0),
	SKW_CHAN2G(9, 2452, 0),
	SKW_CHAN2G(10, 2457, 0),
	SKW_CHAN2G(11, 2462, 0),
	SKW_CHAN2G(12, 2467, 0),
	SKW_CHAN2G(13, 2472, 0),
	SKW_CHAN2G(14, 2484, 0),
};
#undef SKW_CHAN2G

#define SKW_CHAN5G(_channel, _flags) {			    \
	.band			= NL80211_BAND_5GHZ,	    \
	.center_freq		= 5000 + (5 * (_channel)),  \
	.hw_value		= (_channel),		    \
	.flags			= (_flags),		    \
	.max_antenna_gain	= 0,			    \
	.max_power		= 30,			    \
}

static struct ieee80211_channel skw_5ghz_chan[] = {
	SKW_CHAN5G(36, 0),
	SKW_CHAN5G(40, 0),
	SKW_CHAN5G(44, 0),
	SKW_CHAN5G(48, 0),
	SKW_CHAN5G(52, 0),
	SKW_CHAN5G(56, 0),
	SKW_CHAN5G(60, 0),
	SKW_CHAN5G(64, 0),
	SKW_CHAN5G(100, 0),
	SKW_CHAN5G(104, 0),
	SKW_CHAN5G(108, 0),
	SKW_CHAN5G(112, 0),
	SKW_CHAN5G(116, 0),
	SKW_CHAN5G(120, 0),
	SKW_CHAN5G(124, 0),
	SKW_CHAN5G(128, 0),
	SKW_CHAN5G(132, 0),
	SKW_CHAN5G(136, 0),
	SKW_CHAN5G(140, 0),
	SKW_CHAN5G(144, 0),
	SKW_CHAN5G(149, 0),
	SKW_CHAN5G(153, 0),
	SKW_CHAN5G(157, 0),
	SKW_CHAN5G(161, 0),
	SKW_CHAN5G(165, 0),
	SKW_CHAN5G(169, 0),
	SKW_CHAN5G(173, 0),
	SKW_CHAN5G(177, 0),
	SKW_CHAN5G(181, 0),
};
#undef SKW_CHAN5G

#ifdef CONFIG_SWT6621S_6GHZ
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define SKW_CHAN6G(_channel, _flags) {                  \
	.band = NL80211_BAND_6GHZ,                      \
	.center_freq = 5950 + (5 * (_channel)),         \
	.hw_value = (_channel),                         \
	.flags = (_flags),                              \
	.max_antenna_gain = 0,                          \
	.max_power = 30,                                \
}

static struct ieee80211_channel skw_6ghz_chan[] = {
	SKW_CHAN6G(1, 0),
	SKW_CHAN6G(2, 0),
	SKW_CHAN6G(5, 0),
	SKW_CHAN6G(9, 0),
	SKW_CHAN6G(13, 0),
	SKW_CHAN6G(17, 0),
	SKW_CHAN6G(21, 0),
	SKW_CHAN6G(25, 0),
	SKW_CHAN6G(29, 0),
	SKW_CHAN6G(33, 0),
	SKW_CHAN6G(37, 0),
	SKW_CHAN6G(41, 0),
	SKW_CHAN6G(45, 0),
	SKW_CHAN6G(49, 0),
	SKW_CHAN6G(53, 0),
	SKW_CHAN6G(57, 0),
	SKW_CHAN6G(61, 0),
	SKW_CHAN6G(65, 0),
	SKW_CHAN6G(69, 0),
	SKW_CHAN6G(73, 0),
	SKW_CHAN6G(77, 0),
	SKW_CHAN6G(81, 0),
	SKW_CHAN6G(85, 0),
	SKW_CHAN6G(89, 0),
	SKW_CHAN6G(93, 0),
	SKW_CHAN6G(97, 0),
	SKW_CHAN6G(101, 0),
	SKW_CHAN6G(105, 0),
	SKW_CHAN6G(109, 0),
	SKW_CHAN6G(113, 0),
	SKW_CHAN6G(117, 0),
	SKW_CHAN6G(121, 0),
	SKW_CHAN6G(125, 0),
	SKW_CHAN6G(129, 0),
	SKW_CHAN6G(133, 0),
	SKW_CHAN6G(137, 0),
	SKW_CHAN6G(141, 0),
	SKW_CHAN6G(145, 0),
	SKW_CHAN6G(149, 0),
	SKW_CHAN6G(153, 0),
	SKW_CHAN6G(157, 0),
	SKW_CHAN6G(161, 0),
	SKW_CHAN6G(165, 0),
	SKW_CHAN6G(169, 0),
	SKW_CHAN6G(173, 0),
	SKW_CHAN6G(177, 0),
	SKW_CHAN6G(181, 0),
	SKW_CHAN6G(185, 0),
	SKW_CHAN6G(189, 0),
	SKW_CHAN6G(193, 0),
	SKW_CHAN6G(197, 0),
	SKW_CHAN6G(201, 0),
	SKW_CHAN6G(205, 0),
	SKW_CHAN6G(209, 0),
	SKW_CHAN6G(213, 0),
	SKW_CHAN6G(217, 0),
	SKW_CHAN6G(221, 0),
	SKW_CHAN6G(225, 0),
	SKW_CHAN6G(229, 0),
	SKW_CHAN6G(233, 0),
};
#undef SKW_CHAN6G
#endif
#endif

#define SKW_RATETAB_ENT(_rate, _rateid, _flags)     \
{                                                   \
	.bitrate        = (_rate),                  \
	.hw_value       = (_rateid),                \
	.flags          = (_flags),                 \
}

static struct ieee80211_rate skw_rates[] = {
	SKW_RATETAB_ENT(10, 0x1, 0),
	SKW_RATETAB_ENT(20, 0x2, 0),
	SKW_RATETAB_ENT(55, 0x5, 0),
	SKW_RATETAB_ENT(110, 0xb, 0),
	SKW_RATETAB_ENT(60, 0x6, 0),
	SKW_RATETAB_ENT(90, 0x9, 0),
	SKW_RATETAB_ENT(120, 0xc, 0),
	SKW_RATETAB_ENT(180, 0x12, 0),
	SKW_RATETAB_ENT(240, 0x18, 0),
	SKW_RATETAB_ENT(360, 0x24, 0),
	SKW_RATETAB_ENT(480, 0x30, 0),
	SKW_RATETAB_ENT(540, 0x36, 0),
};

#undef SKW_RATETAB_ENT

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
static const struct ieee80211_sband_iftype_data skw_he_capa_2ghz = {
	.types_mask = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP),
	.he_cap = {
		.has_he = true,
		.he_cap_elem = {
			.mac_cap_info[0] = SKW_HE_MAC_CAP0_HTC_HE,
			.mac_cap_info[1] = SKW_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
				SKW_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
			.mac_cap_info[2] = SKW_HE_MAC_CAP2_BSR |
				SKW_HE_MAC_CAP2_MU_CASCADING |
				SKW_HE_MAC_CAP2_ACK_EN,
			.mac_cap_info[3] = SKW_HE_MAC_CAP3_OMI_CONTROL |
				SKW_HE_MAC_CAP3_GRP_ADDR_MULTI_STA_BA_DL_MU |
				SKW_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_VHT_2,
			.mac_cap_info[4] = SKW_HE_MAC_CAP4_AMDSU_IN_AMPDU,
			.phy_cap_info[0] = SKW_HE_PHY_CAP0_DUAL_BAND |
				SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G,
			.phy_cap_info[1] = SKW_HE_PHY_CAP1_DEVICE_CLASS_A |
				SKW_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK |
				SKW_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
				SKW_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS,
			.phy_cap_info[2] = SKW_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
				SKW_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
				SKW_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
				SKW_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
				SKW_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO,
		},
		.he_mcs_nss_supp = {
			.rx_mcs_80 = cpu_to_le16(0xfffa),
			.tx_mcs_80 = cpu_to_le16(0xfffa),
			.rx_mcs_160 = cpu_to_le16(0xffff),
			.tx_mcs_160 = cpu_to_le16(0xffff),
			.rx_mcs_80p80 = cpu_to_le16(0xffff),
			.tx_mcs_80p80 = cpu_to_le16(0xffff),
		},
	},
};

static const struct ieee80211_sband_iftype_data skw_he_capa_5ghz = {
	.types_mask = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP),
	.he_cap = {
		.has_he = true,
		.he_cap_elem = {
			.mac_cap_info[0] = SKW_HE_MAC_CAP0_HTC_HE,
			.mac_cap_info[1] = SKW_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
				SKW_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
			.mac_cap_info[2] = SKW_HE_MAC_CAP2_BSR |
				SKW_HE_MAC_CAP2_MU_CASCADING |
				SKW_HE_MAC_CAP2_ACK_EN,
			.mac_cap_info[3] = SKW_HE_MAC_CAP3_OMI_CONTROL |
				SKW_HE_MAC_CAP3_GRP_ADDR_MULTI_STA_BA_DL_MU |
				SKW_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_VHT_2,
			.mac_cap_info[4] = SKW_HE_MAC_CAP4_AMDSU_IN_AMPDU,

			.phy_cap_info[0] = SKW_HE_PHY_CAP0_DUAL_BAND |
				SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G,
			.phy_cap_info[1] = SKW_HE_PHY_CAP1_DEVICE_CLASS_A |
				SKW_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK |
				SKW_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
				SKW_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS,
			.phy_cap_info[2] = SKW_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
				SKW_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
				SKW_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
				SKW_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
				SKW_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO,
		},
		.he_mcs_nss_supp = {
			.rx_mcs_80 = cpu_to_le16(0xfffa),
			.tx_mcs_80 = cpu_to_le16(0xfffa),
			.rx_mcs_160 = cpu_to_le16(0xffff),
			.tx_mcs_160 = cpu_to_le16(0xffff),
			.rx_mcs_80p80 = cpu_to_le16(0xffff),
			.tx_mcs_80p80 = cpu_to_le16(0xffff),
		},
	},
};

#endif

#define skw_a_rates       (skw_rates + 4)
#define skw_a_rates_size  8
#define skw_g_rates       (skw_rates + 0)
#define skw_g_rates_size  12

static struct ieee80211_supported_band skw_band_2ghz = {
	.channels = skw_2ghz_chan,
	.n_channels = ARRAY_SIZE(skw_2ghz_chan),
	.bitrates = skw_g_rates,
	.n_bitrates = skw_g_rates_size,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	.n_iftype_data = 1,
	.iftype_data = &skw_he_capa_2ghz,
#endif
};

static struct ieee80211_supported_band skw_band_5ghz = {
	.channels = skw_5ghz_chan,
	.n_channels = ARRAY_SIZE(skw_5ghz_chan),
	.bitrates = skw_a_rates,
	.n_bitrates = skw_a_rates_size,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	.n_iftype_data = 1,
	.iftype_data = &skw_he_capa_5ghz,
#endif
};

#ifdef CONFIG_SWT6621S_6GHZ
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static struct ieee80211_supported_band skw_band_6ghz = {
	.channels = skw_6ghz_chan,
	.n_channels = ARRAY_SIZE(skw_6ghz_chan),
	.bitrates = skw_a_rates,
	.n_bitrates = skw_a_rates_size,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	.n_iftype_data = 1,
	.iftype_data = &skw_he_capa_5ghz, //TBD:check it
#endif
};
#endif
#endif

static const u32 skw_cipher_suites[] = {
	/* keep WEP first, it may be removed below */
	SKW_CIPHER_SUITE_WEP40,
	SKW_CIPHER_SUITE_TKIP,
	SKW_CIPHER_SUITE_CCMP,
	SKW_CIPHER_SUITE_WEP104,
	SKW_CIPHER_SUITE_AES_CMAC,
	SKW_CIPHER_SUITE_GCMP,

	SKW_CIPHER_SUITE_CCMP_256,
	SKW_CIPHER_SUITE_GCMP_256,
	SKW_CIPHER_SUITE_BIP_CMAC_256,
	SKW_CIPHER_SUITE_BIP_GMAC_128,
	SKW_CIPHER_SUITE_BIP_GMAC_256,

	SKW_CIPHER_SUITE_SMS4,
};

static inline void skw_iftype_dump(struct skw_core *skw, int iftype_num[NUM_NL80211_IFTYPES])
{
	int i;

	for (i = 0; i < NUM_NL80211_IFTYPES; i++) {
		if (iftype_num[i])
			skw_chip_info(skw->idx, "%s: %d\n", skw_iftype_name(i), iftype_num[i]);
	}
}

static void skw_count_iftype(struct wiphy *wiphy, int num[NUM_NL80211_IFTYPES])
{
	int i;
	struct skw_iface *iface;
	struct skw_core *skw = wiphy_priv(wiphy);

	spin_lock_bh(&skw->vif.lock);

	for (i = 0; i < SKW_NR_IFACE; i++) {
		iface = skw->vif.iface[i];
		if (!iface ||
		    (iface->flags & SKW_IFACE_FLAG_LEGACY_P2P_DEV) ||
		    (iface->wdev.iftype == NL80211_IFTYPE_P2P_DEVICE))
			continue;

		num[iface->wdev.iftype]++;
	}

	spin_unlock_bh(&skw->vif.lock);
}

static struct wireless_dev *skw_add_virtual_intf(struct wiphy *wiphy,
		const char *name, unsigned char name_assign_type,
		enum nl80211_iftype type, u32 *flags, struct vif_params *params)
{
	int ret;
	struct skw_iface *iface;
	u8 vif_id = SKW_INVALID_ID;
	int iftype_num[NUM_NL80211_IFTYPES] = {0};
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s(%s), mac: %pM\n", name, skw_iftype_name(type),
		params->macaddr);

	skw_count_iftype(wiphy, iftype_num);
	ret = skw_compat_check_combs(wiphy, 0, 0, iftype_num);
	if (ret) {
		skw_chip_err(skw->idx, "check combinations failed, %s(%s)\n",
			name, skw_iftype_name(type));

		skw_iftype_dump(skw, iftype_num);

		return ERR_PTR(-EINVAL);
	}

	if (type == NL80211_IFTYPE_P2P_DEVICE)
		vif_id = SKW_LAST_IFACE_ID;

	iface = skw_add_iface(wiphy, name, type, params->macaddr, vif_id,
				type != NL80211_IFTYPE_P2P_DEVICE);
	if (IS_ERR(iface)) {
		skw_chip_err(skw->idx, "failed, %ld\n", PTR_ERR(iface));
		return ERR_CAST(iface);
	}

	return &iface->wdev;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
static struct wireless_dev *skw_cfg80211_add_virtual_intf(struct wiphy *wiphy,
			const char *name, unsigned char name_assign_type,
			enum nl80211_iftype type, struct vif_params *params)
{
	u32 flags = 0;

	return skw_add_virtual_intf(wiphy, name, name_assign_type,
			type, &flags, params);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static struct wireless_dev *skw_cfg80211_add_virtual_intf(struct wiphy *wiphy,
			const char *name, unsigned char name_assign_type,
			enum nl80211_iftype type, u32 *flags,
			struct vif_params *params)
{
	return skw_add_virtual_intf(wiphy, name, name_assign_type,
			type, flags, params);
}
#else
static struct wireless_dev *skw_cfg80211_add_virtual_intf(struct wiphy *wiphy,
			const char *name, enum nl80211_iftype type, u32 *flags,
			struct vif_params *params)
{
	return skw_add_virtual_intf(wiphy, name, 0, type, flags, params);
}
#endif

static int skw_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "iftype: %d, iface id: %d\n", wdev->iftype, iface->id);

	return skw_del_iface(wiphy, iface);
}

static int skw_change_intf(struct wiphy *wiphy, struct net_device *dev,
			   enum nl80211_iftype type, u32 *flags,
			   struct vif_params *params)
{
	u8 *mac;
	int ret;
	struct skw_core *skw = wiphy_priv(wiphy);
	int iftype_num[NUM_NL80211_IFTYPES] = {0};
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "%s (inst: %d), %s -> %s, mac: %pM, 4addr: %d, flags: 0x%x\n",
		netdev_name(dev), iface->id,
		skw_iftype_name(dev->ieee80211_ptr->iftype),
		skw_iftype_name(type), params->macaddr,
		params->use_4addr, iface->flags);

	if (iface->flags & SKW_IFACE_FLAG_LEGACY_P2P_DEV)
		iface->wdev.iftype = type;

	if (iface->wdev.iftype == type)
		return 0;

	skw_count_iftype(wiphy, iftype_num);
	iftype_num[type]++;
	iftype_num[iface->wdev.iftype]--;
	ret = skw_compat_check_combs(wiphy, 0, 0, iftype_num);
	if (ret) {
		skw_chip_err(skw->idx, "check combinations failed, %s(inst: %d), %s -> %s\n",
			netdev_name(dev), iface->id,
			skw_iftype_name(dev->ieee80211_ptr->iftype),
			skw_iftype_name(type));

		skw_iftype_dump(skw, iftype_num);

		return ret;
	}

	if (iface->ndev)
		netif_tx_stop_all_queues(dev);

	ret = skw_iface_teardown(wiphy, iface);
	if (ret) {
		skw_chip_err(skw->idx, "teardown failed, %s (inst: %d), ret: %d\n",
			skw_iftype_name(iface->wdev.iftype), iface->id, ret);

		goto out;
	}

	if (params->use_4addr && skw_config_use_4addr(skw->config))
		skw_set_mib_bool(wiphy, 0, SKW_MIB_SET_WDS_ENABLE, true);

	if (is_valid_ether_addr(params->macaddr))
		mac = params->macaddr;
	else
		mac = (u8 *)wdev_address(dev->ieee80211_ptr);

	ret = skw_iface_setup(wiphy, dev, iface, mac, type, iface->id);
	if (ret) {
		skw_chip_err(skw->idx, "open dev failed, %s (inst: %d)\n",
			skw_iftype_name(type), iface->id);

			ret = skw_iface_setup(wiphy, dev, iface, iface->addr,
				iface->wdev.iftype, iface->id);
			if (ret)
				skw_chip_err(skw->idx, "open dev failed, %s (inst: %d)\n",
					skw_iftype_name(iface->wdev.iftype), iface->id);
	}

out:
	if (iface->ndev)
		netif_tx_start_all_queues(dev);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
static int skw_cfg80211_change_intf(struct wiphy *wiphy, struct net_device *dev,
		enum nl80211_iftype type, struct vif_params *params)
{
	u32 flags = 0;

	return skw_change_intf(wiphy, dev, type, &flags, params);
}
#else
static int skw_cfg80211_change_intf(struct wiphy *wiphy, struct net_device *dev,
		enum nl80211_iftype type, u32 *flags, struct vif_params *params)
{
	return skw_change_intf(wiphy, dev, type, flags, params);
}
#endif

static int skw_get_key(struct wiphy *wiphy, struct net_device *dev,
		int link_id, u8 key_idx, bool pairwise,
		const u8 *mac_addr, void *cookie,
		void (*callback)(void *cookie, struct key_params *params))
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, link: %d, key_idx: %d, pairwise: %d, mac: %pM\n",
		netdev_name(dev), link_id, key_idx, pairwise, mac_addr);

	return 0;
}

static int skw_cmd_add_key(struct wiphy *wiphy, struct net_device *dev,
			   int cipher, u8 key_idx, int key_type,
			   const u8 *key, int key_len, const u8 *addr)
{
	struct skw_key_params params;
	struct skw_iface *iface = netdev_priv(dev);
	u8 wapi_tx_pn[] = {0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c};
	struct skw_core *skw = wiphy_priv(wiphy);

	memset(&params, 0x0, sizeof(params));

	if (addr)
		skw_ether_copy(params.mac_addr, addr);
	else
		memset(params.mac_addr, 0xff, ETH_ALEN);

	memcpy(params.key, key, key_len);

	params.key_type = key_type;
	params.key_len = key_len;
	params.key_id = key_idx;
	params.cipher_type = cipher;
	params.pn[0] = 1;

	switch (cipher) {
	case SKW_CIPHER_TYPE_SMS4:
		memcpy(params.pn, wapi_tx_pn, SKW_PN_LEN);

		if (is_skw_ap_mode(iface))
			params.pn[0] += 1;

		break;

	case SKW_CIPHER_TYPE_TKIP:
		if (is_skw_ap_mode(iface))
			memcpy(&params.key[0], key, 32);
		else {
			memcpy(&params.key[0], key, 16);
			memcpy(&params.key[16], key + 24, 8);
			memcpy(&params.key[24], key + 16, 8);
		}

		break;

	default:
		break;
	}

	if (is_skw_ap_mode(iface) && iface->buf_keys_idx < SKW_MAX_BUF_KEYS &&
		!SKW_TEST(iface->flags, SKW_IFACE_FLAG_AP_STARTED)) {
		memcpy(&iface->buf_keys[iface->buf_keys_idx++],
			 &params, sizeof(params));
		SKW_SET(iface->flags, SKW_IFACE_FLAG_BUF_KEY);
		skw_chip_dbg(skw->idx, "lead key buffered, idx: %d\n", iface->buf_keys_idx - 1);
		return 0;
	} else
		return skw_send_msg(wiphy, dev, SKW_CMD_ADD_KEY, &params,
			sizeof(params), NULL, 0);
}

static int skw_set_key(struct wiphy *wiphy, struct net_device *dev,
			struct skw_key_conf *conf, u8 key_idx, int key_type,
			const u8 *addr, struct key_params *params)
{
	int i, cipher, ret;
	struct skw_key *key, *old_key;
	struct skw_core *skw = wiphy_priv(wiphy);

	cipher = to_skw_cipher_type(params->cipher);
	if (cipher == SKW_CIPHER_TYPE_INVALID) {
		skw_chip_warn(skw->idx, "cipher 0x%x unsupported\n", params->cipher);
		return -ENOTSUPP;
	}

	key = SKW_ZALLOC(sizeof(struct skw_key), GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	key->key_len = params->key_len;
	memcpy(key->key_data, params->key, params->key_len);

	if (params->seq) {
		skw_hex_dump("seq", params->seq, params->seq_len, false);

		for (i = 1; i < IEEE80211_NUM_TIDS; i++)
			memcpy(key->rx_pn[i], params->seq, SKW_PN_LEN);
	}

	conf->skw_cipher = cipher;

	old_key = rcu_dereference_protected(conf->key[key_idx],
			lockdep_is_held(&conf->lock));

	rcu_assign_pointer(conf->key[key_idx], key);

	SKW_SET(conf->installed_bitmap, BIT(key_idx));

	if (old_key)
		kfree_rcu(old_key, rcu);

	if (cipher == SKW_CIPHER_TYPE_WEP40 ||
	    cipher == SKW_CIPHER_TYPE_WEP104) {
		SKW_SET(conf->flags, SKW_KEY_FLAG_WEP_SHARE);
		return 0;
	}

	ret = skw_cmd_add_key(wiphy, dev, cipher, key_idx, key_type,
			params->key, params->key_len, addr);
	if (ret) {
		RCU_INIT_POINTER(conf->key[key_idx], NULL);
		SKW_CLEAR(conf->installed_bitmap, BIT(key_idx));
		kfree_rcu(key, rcu);
	}

	return ret;
}

static int skw_add_key(struct wiphy *wiphy, struct net_device *dev,
		       int link_id, u8 key_idx, bool pairwise,
		       const u8 *addr, struct key_params *params)
{
	const u8 *mac;
	int ret, key_type;
	struct skw_key_conf *conf;
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, key_idx: %d, cipher: 0x%x, pairwise: %d, mac: %pM\n",
		netdev_name(dev), key_idx, params->cipher, pairwise, addr);

	key_type = pairwise ? SKW_KEY_TYPE_PTK : to_skw_gtk(key_idx);

	if (addr) {
		ctx = skw_peer_ctx(iface, addr);
		if (!ctx) {
			skw_chip_warn(skw->idx, "%pM not linked\n", addr);
			return -ENOLINK;
		}

		skw_peer_ctx_lock(ctx);

		if (!ctx->peer) {
			skw_peer_ctx_unlock(ctx);
			return 0;
		}

		if (pairwise)
			conf = &ctx->peer->ptk_conf;
		else
			conf = &ctx->peer->gtk_conf;

		mutex_lock(&conf->lock);

		ret = skw_set_key(wiphy, dev, conf, key_idx,
				  key_type, addr, params);

		mutex_unlock(&conf->lock);

		skw_peer_ctx_unlock(ctx);

	} else {
		if (is_skw_ap_mode(iface))
			mac = NULL;
		else
			mac = iface->sta.core.bss.bssid;

		conf = &iface->key_conf;

		mutex_lock(&conf->lock);

		ret = skw_set_key(wiphy, dev, conf, key_idx,
				  key_type, mac, params);

		mutex_unlock(&conf->lock);
	}

	if (ret)
		skw_chip_err(skw->idx, "failed, cipher: 0x%x, ptk: %d, idx: %d, ret: %d\n",
			params->cipher, pairwise, key_idx, ret);

	return ret;
}

static int skw_cmd_del_key(struct wiphy *wiphy, struct net_device *dev,
			u8 key_idx, int key_type, int cipher, const u8 *addr)
{
	struct skw_key_params params;

	memset(&params, 0x0, sizeof(params));

	if (addr)
		skw_ether_copy(params.mac_addr, addr);
	else
		memset(params.mac_addr, 0xff, ETH_ALEN);

	params.key_type = key_type;
	params.cipher_type = cipher;
	params.key_id = key_idx;

	return skw_send_msg(wiphy, dev, SKW_CMD_DEL_KEY, &params,
			   sizeof(params), NULL, 0);
}

static int skw_remove_key(struct wiphy *wiphy, struct net_device *dev,
			struct skw_key_conf *conf, u8 key_idx,
			int key_type, const u8 *addr)
{
	int ret;
	struct skw_key *key;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (SKW_TEST(conf->installed_bitmap, BIT(key_idx))) {
		ret = skw_cmd_del_key(wiphy, dev, key_idx, key_type,
				conf->skw_cipher, addr);
		if (ret)
			skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
	}

	key = rcu_dereference_protected(conf->key[key_idx],
			lockdep_is_held(&conf->lock));

	RCU_INIT_POINTER(conf->key[key_idx], NULL);

	SKW_CLEAR(conf->installed_bitmap, BIT(key_idx));

	if (SKW_TEST(conf->flags, SKW_KEY_FLAG_WEP_SHARE)) {
		SKW_CLEAR(conf->flags, SKW_KEY_FLAG_WEP_SHARE);
		SKW_CLEAR(conf->flags, SKW_KEY_FLAG_WEP_UNICAST);
		SKW_CLEAR(conf->flags, SKW_KEY_FLAG_WEP_MULTICAST);
	}

	if (key)
		kfree_rcu(key, rcu);

	return 0;
}

static int skw_del_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool pairwise, const u8 *addr)
{
	int ret, key_type;
	struct skw_key_conf *conf;
	const u8 *mac = NULL;
	struct skw_peer_ctx *ctx = NULL;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "link: %d, key_idx: %d, pairwise: %d, mac: %pM\n",
		link_id, key_idx, pairwise, addr);

	if (key_idx >= SKW_NUM_MAX_KEY) {
		skw_chip_err(skw->idx, "key index %d out of bounds\n", key_idx);
		return -EINVAL;
	}

	key_type = pairwise ? SKW_KEY_TYPE_PTK : to_skw_gtk(key_idx);

	if (addr) {
		ctx = skw_peer_ctx(iface, addr);
		if (!ctx)
			return 0;

		skw_peer_ctx_lock(ctx);

		if (!ctx->peer) {
			skw_peer_ctx_unlock(ctx);
			return 0;
		}

		if (pairwise)
			conf = &ctx->peer->ptk_conf;
		else
			conf = &ctx->peer->gtk_conf;

		mutex_lock(&conf->lock);

		ret = skw_remove_key(wiphy, dev, conf, key_idx, key_type, addr);

		mutex_unlock(&conf->lock);

		skw_peer_ctx_unlock(ctx);

	} else {

		conf = &iface->key_conf;

		if (is_skw_sta_mode(iface))
			mac = iface->sta.core.bss.bssid;

		mutex_lock(&conf->lock);

		ret = skw_remove_key(wiphy, dev, conf, key_idx, key_type, mac);

		mutex_unlock(&conf->lock);
	}

	return ret;
}

/* for WEP keys */
static int skw_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool unicast, bool multicast)
{
	int ret = 0, key_len;
	struct skw_key *key;
	const u8 *mac = NULL;
	u8 key_data[WLAN_MAX_KEY_LEN] = {0};
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_key_conf *conf = &iface->key_conf;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, link: %d, key_idx: %d, unicast: %d, multicast: %d\n",
		netdev_name(dev), link_id, key_idx, unicast, multicast);

	if (!(conf->installed_bitmap & BIT(key_idx)))
		return 0;

	if (is_skw_sta_mode(iface))
		mac = iface->sta.core.bss.bssid;

	rcu_read_lock();

	key = conf->key[key_idx];
	if (key) {
		memcpy(key_data, key->key_data, key->key_len);
		key_len = key->key_len;
	}

	rcu_read_unlock();

	if (!key)
		return 0;

	conf->wep_idx = key_idx;

	if (unicast) {
		ret = skw_cmd_add_key(wiphy, dev, conf->skw_cipher,
				      key_idx, SKW_KEY_TYPE_PTK,
				      key_data, key_len, mac);

		if (ret)
			SKW_SET(conf->flags, SKW_KEY_FLAG_WEP_UNICAST);
	}

	if (multicast) {
		ret = skw_cmd_add_key(wiphy, dev, conf->skw_cipher,
				      key_idx, SKW_KEY_TYPE_GTK,
				      key_data, key_len, mac);

		if (ret)
			SKW_SET(conf->flags, SKW_KEY_FLAG_WEP_MULTICAST);
	}

	return 0;
}

/* for 11w */
static int skw_set_default_mgmt_key(struct wiphy *wiphy, struct net_device *dev,
				int link_id, u8 key_index)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, key index: %d\n", netdev_name(dev), key_index);

	return 0;
}

static int skw_set_mac_acl(struct wiphy *wiphy, struct net_device *dev,
			const struct cfg80211_acl_data *acl)
{
	int size;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!acl)
		return 0;

	skw_chip_dbg(skw->idx, "dev: %s, nr_entries: %d\n",
		netdev_name(dev), acl->n_acl_entries);

	if (!acl->n_acl_entries) {
		SKW_KFREE(iface->sap.acl);
		return 0;
	}

	size = acl->n_acl_entries * sizeof(struct mac_address);
	size += sizeof(struct cfg80211_acl_data);

	SKW_KFREE(iface->sap.acl);

	iface->sap.acl = SKW_ZALLOC(size, GFP_KERNEL);
	if (!iface->sap.acl)
		return -ENOMEM;

	memcpy(iface->sap.acl, acl, size);

	skw_queue_work(wiphy, netdev_priv(dev), SKW_WORK_ACL_CHECK, NULL, 0);

	return 0;
}

static bool skw_channel_allowed(struct wiphy *wiphy, u16 channel)
{
#define BITMAP_SIZE ((164 + BITS_PER_LONG) / BITS_PER_LONG)
	int i, nr_channel;
	struct skw_iface *iface;
	bool extra_chn = false;
	struct skw_core *skw = wiphy_priv(wiphy);
	int iftype_num[NUM_NL80211_IFTYPES] = {0};
	long channel_map[BITMAP_SIZE] = {0};

	spin_lock_bh(&skw->vif.lock);

	for (nr_channel = 0, i = 0; i < SKW_NR_IFACE; i++) {
		struct ieee80211_channel *chan = NULL;

		iface = skw->vif.iface[i];
		if (!iface)
			continue;

		switch (iface->wdev.iftype) {
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
			chan = iface->sap.cfg.channel;
			break;

		case NL80211_IFTYPE_STATION:
			if (atomic_read(&iface->actived_ctx) > 1)
				extra_chn = true;

			/* fall through */
			skw_fallthrough;
		case NL80211_IFTYPE_P2P_CLIENT:
			chan = iface->sta.core.bss.channel;
			break;

		default:
			break;
		}

		if (chan && !test_and_set_bit(chan->hw_value, channel_map))
			nr_channel++;
	}

	spin_unlock_bh(&skw->vif.lock);

	for (i = 0; extra_chn && (i < SKW_MAX_PEER_SUPPORT); i++) {
		struct skw_peer_ctx *ctx = &skw->hw.lmac[iface->lmac_id].peer_ctx[i];

		skw_peer_ctx_lock(ctx);

		if (ctx->peer && ctx->peer->channel &&
		    !test_and_set_bit(ctx->peer->channel, channel_map))
			nr_channel++;

		skw_peer_ctx_unlock(ctx);
	}

	if (!test_bit(channel, channel_map))
		nr_channel++;

	if (!skw_compat_check_combs(wiphy, nr_channel, 0, iftype_num))
		return true;

	skw_chip_err(skw->idx, "channel %d not allowed, total:%d\n", channel, nr_channel);
	skw_hex_dump("channels", channel_map, sizeof(channel_map), true);

	return false;
}

static const u8 *skw_find_ie_match(u8 eid, const u8 *ies, int len, const u8 *match,
			    int match_len, int match_offset)
{
	const struct skw_element *elem;

	/* match_offset can't be smaller than 2, unless match_len is
	 * zero, in which case match_offset must be zero as well.
	 */
	if (WARN_ON((match_len && match_offset < 2) ||
		    (!match_len && match_offset)))
		return NULL;

	skw_foreach_element_id(elem, eid, ies, len) {
		if (elem->datalen >= match_offset - 2 + match_len &&
		    !memcmp(elem->data + match_offset - 2, match, match_len))
			return (void *)elem;
	}

	return NULL;
}

int skw_sap_set_mib(struct wiphy *wiphy, struct net_device *dev,
		const u8 *ies, int ies_len)
{
	int ret = 0;
	struct skw_tlv_conf conf;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (skw_tlv_alloc(&conf, 128, GFP_KERNEL))
		return -ENOMEM;

	skw_tlv_start(&conf);

	if (iface->extend.wireless_mode) {
		skw_chip_dbg(skw->idx, "wireless_mode: %d\n", iface->extend.wireless_mode);

		switch (iface->extend.wireless_mode) {
		case SKW_WIRELESS_11G_ONLY:
			if (skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HE, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_VHT, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HT, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_B, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_A, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_G, true)) {

				ret = -ENOMEM;
				skw_chip_err(skw->idx, "set 11G mode failed\n");

				goto free;
			}

			break;

		case SKW_WIRELESS_11N_ONLY:
			if (skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HE, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_VHT, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HT, true) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_B, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_A, false) ||
			    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_G, false)) {

				ret = -ENOMEM;
				skw_chip_err(skw->idx, "set 11N mode failed\n");

				goto free;
			}

			break;

		default:
			break;
		}

	} else if (ies && ies_len) {
		u8 ext_eid;
		const u8 *ie;
		bool ht_enable = false, vht_enable = false, he_enable = false;

		ie = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, ies, ies_len);
		if (ie && ie[1])
			ht_enable = true;

		ie = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, ies, ies_len);
		if (ie && ie[1])
			vht_enable = true;

		ext_eid = SKW_WLAN_EID_EXT_HE_CAPABILITY;
		ie = skw_find_ie_match(SKW_WLAN_EID_EXTENSION, ies, ies_len, &ext_eid, 1, 2);
		if (ie && ie[1])
			he_enable = true;

		skw_chip_dbg(skw->idx, "ht_enable: %d, vht_enable: %d, he_enable: %d\n",
			ht_enable, vht_enable, he_enable);

		if (skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HT, ht_enable) ||
		    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_VHT, vht_enable) ||
		    skw_tlv_add_bool(&conf, SKW_MIB_DOT11_MODE_HE, he_enable)) {

			ret = -ENOMEM;
			skw_chip_err(skw->idx, "set beacon capa failed\n");

			goto free;
		}
	}

	skw_tlv_end(&conf);

	if (conf.total_len)
		ret = skw_send_msg(wiphy, dev, SKW_CMD_SET_MIB, conf.buff,
				  conf.total_len, NULL, 0);

free:
	skw_tlv_free(&conf);

	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	return ret;
}

#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
static int
skw_switch_usb3_to_usb2_using_2G(struct skw_iface *iface, enum nl80211_band band)
{
	struct skw_core *skw = iface->skw;
	char mode[20];
	int ret = 0;

	skw_chip_dbg(skw->idx, "bus:%d align_value:%d band:%d\n", skw->hw.bus,
		skw->hw_pdata->align_value, band);
	if (skw->hw.bus == SKW_BUS_USB &&
		skw->hw_pdata->align_value == 1024 &&
		band == NL80211_BAND_2GHZ &&
		skw->hw_pdata->usb_speed_switch) {
		if (test_bit(SKW_FLAG_SWITCHING_USB_MODE, &skw->flags)) {
			skw_chip_dbg(skw->idx, "already in switching\n");
			return -EBUSY;
		}
		set_bit(SKW_FLAG_SWITCHING_USB_MODE, &skw->flags);
		skw->hw_pdata->usb_speed_switch(mode);
		skw_chip_dbg(skw->idx, "change usb mode to %s\n", mode);

		skw_chip_dbg(skw->idx, "waiting for the switch completion");
		if (wait_for_completion_interruptible_timeout(&skw->usb_switch_done,
					SKW_RECOVERY_TIMEOUT) == 0) {
			skw_chip_err(skw->idx, "switch timeout\n");
			ret = -ETIME;
		}
	}

	return ret;
}
#endif

static int skw_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_ap_settings *settings)
{
	int ret, bw;
	bool dfs_required = false;
	int total, fixed, offset = 0;
	struct skw_startap_resp resp = {};
	struct skw_startap_param *param = NULL;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);
	struct cfg80211_beacon_data *bcn = &settings->beacon;
	struct cfg80211_chan_def *chandef = &settings->chandef;
	struct skw_key_conf *conf = &iface->key_conf;

	skw_chip_info(skw->idx, "ndev: %s\n", netdev_name(dev));
	skw_chip_dbg(skw->idx, "       * ssid: %s\n", settings->ssid);
	skw_chip_dbg(skw->idx, "       * bssid: %pM\n", iface->addr);
	skw_chip_dbg(skw->idx, "       * channel: %d band:%d (BW: %d)\n", chandef->chan->hw_value,
			chandef->chan->band, chandef->width);
	skw_chip_dbg(skw->idx, "       * auth type: %d\n", settings->auth_type);
	skw_chip_dbg(skw->idx, "       * akm_suites: %d\n", settings->crypto.n_akm_suites);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
	dfs_required = settings->radar_required;
#else
	dfs_required = skw_compat_cfg80211_chandef_dfs_required(wiphy,
				chandef, iface->wdev.iftype);
#endif
	if (!skw_channel_allowed(wiphy, chandef->chan->hw_value))
		return -ENOTSUPP;

	bw = to_skw_bw(settings->chandef.width);
	if (bw == SKW_CHAN_WIDTH_MAX) {
		skw_chip_err(skw->idx, "BW %d not support\n", settings->chandef.width);
		return -ENOTSUPP;
	}

#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
	ret = skw_switch_usb3_to_usb2_using_2G(iface, chandef->chan->band);
	if (ret)
		return ret;
#endif

	skw_sap_set_mib(wiphy, dev, bcn->tail, bcn->tail_len);

	fixed = sizeof(struct skw_startap_param);
	total = fixed +
		bcn->head_len +
		bcn->tail_len +
		bcn->probe_resp_len;

	param = SKW_ZALLOC(total, GFP_KERNEL);
	if (!param) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	param->chan_width = bw;
	param->chan = chandef->chan->hw_value;
	param->band = to_skw_band(chandef->chan->band);
	param->center_chn1 = skw_freq_to_chn(chandef->center_freq1);
	param->center_chn2 = skw_freq_to_chn(chandef->center_freq2);

	param->beacon_int = settings->beacon_interval;
	param->dtim_period = settings->dtim_period;
	param->ssid_len = settings->ssid_len;
	memcpy(param->ssid, settings->ssid, settings->ssid_len);

	if (settings->hidden_ssid)
		param->flags |= settings->hidden_ssid;

	if (bcn->head) {
		skw_hex_dump("beacon_head", bcn->head, bcn->head_len, false);

		param->beacon_head_len = bcn->head_len;
		param->beacon_head_offset = offset + fixed;

		memcpy(param->ies + offset, bcn->head, bcn->head_len);
		offset += bcn->head_len;
	}

	if (bcn->tail) {
		skw_hex_dump("beacon_tail", bcn->tail, bcn->tail_len, false);

		param->beacon_tail_offset = offset + fixed;
		param->beacon_tail_len = bcn->tail_len;

		memcpy(param->ies + offset, bcn->tail, bcn->tail_len);
		offset += bcn->tail_len;

		skw_iface_set_wmm_capa(iface, bcn->tail, bcn->tail_len);
	}

	if (bcn->probe_resp) {
		skw_hex_dump("probe_resp", bcn->probe_resp,
				bcn->probe_resp_len, false);

		param->probe_rsp_ies_offset = offset + fixed;
		param->probe_rsp_ies_len = bcn->probe_resp_len;

		memcpy(param->ies + offset, bcn->probe_resp,
			bcn->probe_resp_len);

		offset += bcn->probe_resp_len;

		if (iface->sap.probe_resp) {
			memcpy(iface->sap.probe_resp, bcn->probe_resp,
					bcn->probe_resp_len);
			iface->sap.probe_resp_len = bcn->probe_resp_len;
		}
	}

	if (skw_recovery_data_update(iface, param, total)) {
		skw_chip_err(skw->idx, "build recovery failed\n");

		SKW_KFREE(param);
		return -ENOMEM;
	}

	skw_edma_mask_irq(skw, iface->lmac_id);
	ret = skw_send_msg(wiphy, dev, SKW_CMD_START_AP, param, total,
			   &resp, sizeof(resp));
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

		skw_recovery_data_clear(iface);
		SKW_KFREE(param);

		return ret;
	}

	skw_startap_resp_handler(skw, iface, &resp);

	if (SKW_TEST(conf->flags, SKW_KEY_FLAG_WEP_UNICAST) ||
	    SKW_TEST(conf->flags, SKW_KEY_FLAG_WEP_MULTICAST))
		skw_set_default_key(wiphy, dev, 0, conf->wep_idx,
				SKW_TEST(conf->flags, SKW_KEY_FLAG_WEP_UNICAST),
				SKW_TEST(conf->flags, SKW_KEY_FLAG_WEP_MULTICAST));

	if (dfs_required) {
		ret = skw_dfs_chan_init(wiphy, dev, chandef, 0);
		if (ret) {
			skw_chip_err(skw->idx, "dfs channel init failed, ret: %d\n", ret);
			SKW_KFREE(param);

			return ret;
		}

		ret = skw_dfs_start_monitor(wiphy, dev);
		if (ret) {
			skw_chip_dbg(skw->idx, "start dfs monitor failed, ret: %d\n", ret);
			SKW_KFREE(param);

			return ret;
		}
	}

	skw_dpd_set_coeff_params(wiphy, dev, param->chan,
		param->center_chn1, param->center_chn2, param->chan_width);

	skw_set_mac_acl(wiphy, dev, settings->acl);

#if 0
	skw_set_ratio_level_mib(wiphy, skw->config.fw.noa_ratio_en,
		 skw->config.fw.noa_ratio_idx);
	skw_set_once_noa_mib(wiphy, skw->config.fw.once_noa_en,
		 skw->config.fw.once_noa_pre, skw->config.fw.once_noa_abs);
#endif

	memcpy(iface->sap.cfg.ssid, settings->ssid, settings->ssid_len);
	iface->sap.cfg.ssid_len = settings->ssid_len;

	iface->sap.cfg.auth_type = settings->auth_type;
	iface->sap.cfg.channel = chandef->chan;
	iface->sap.cfg.width = bw;

	skw_ether_copy(iface->sap.cfg.bssid, iface->addr);
	memcpy(&iface->sap.cfg.crypto, &settings->crypto,
		sizeof(settings->crypto));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	iface->sap.cfg.ht_cap = SKW_KMEMDUP(settings->ht_cap,
				sizeof(*settings->ht_cap), GFP_KERNEL);
	iface->sap.cfg.vht_cap = SKW_KMEMDUP(settings->vht_cap,
				sizeof(*settings->vht_cap), GFP_KERNEL);

	iface->sap.ht_required = settings->ht_required;
	iface->sap.vht_required = settings->vht_required;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
	iface->sap.cfg.crypto.wep_keys = NULL;
#endif
	iface->sap.cfg.crypto.psk = NULL;
#else
	iface->sap.cfg.ht_cap = NULL;
	iface->sap.cfg.vht_cap = NULL;

	iface->sap.ht_required = false;
	iface->sap.vht_required = false;
#endif

	if (iface->skw->hw.bus == SKW_BUS_PCIE) {
		if (skw_edma_get_refill((void *)iface->skw, iface->lmac_id) == 0)
			skw_edma_init_data_chan((void *)iface->skw, iface->lmac_id);
		else
			skw_edma_inc_refill((void *)iface->skw, iface->lmac_id);
	}

	SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_DEAUTH);
	netif_carrier_on(dev);

	if (SKW_TEST(iface->flags, SKW_IFACE_FLAG_BUF_KEY)) {
		int i;

		for (i = 0; i < iface->buf_keys_idx; i++) {
			skw_send_msg(wiphy, dev, SKW_CMD_ADD_KEY,
				&iface->buf_keys[i], sizeof(struct skw_key_params), NULL, 0);
			skw_chip_dbg(skw->idx, "send key buffered, idx: %d\n", i);
			memset(&iface->buf_keys[i], 0, sizeof(struct skw_key_params));
		}

		iface->buf_keys_idx = 0;
		SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_BUF_KEY);
	}

	SKW_SET(iface->flags, SKW_IFACE_FLAG_AP_STARTED);

	SKW_KFREE(param);

	return 0;
}

static int skw_sap_del_sta(struct wiphy *wiphy, struct net_device *dev,
			struct skw_peer_ctx *ctx, u8 subtype, u16 reason)
{
	int ret;
	bool tx = true;
	const u8 *mac = NULL;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!ctx)
		return 0;

	skw_peer_ctx_lock(ctx);

	if (ctx->peer) {
		mac = ctx->peer->addr;
		__skw_peer_ctx_transmit(ctx, false);
		skw_set_state(skw, &ctx->peer->sm, SKW_STATE_NONE);

		tx = !(ctx->peer->flags & SKW_PEER_FLAG_DEAUTHED);
		SKW_SET(ctx->peer->flags, SKW_PEER_FLAG_DEAUTHED);
	}

	skw_peer_ctx_unlock(ctx);

	if (!mac)
		return 0;

	skw_mlme_ap_remove_client(iface, mac);

	ret = skw_cmd_del_sta(wiphy, dev, mac, subtype, reason, tx);
	if (!ret)
		skw_peer_ctx_bind(iface, ctx, NULL);

	return ret;
}

static void skw_sap_flush_sta(struct wiphy *wiphy, struct skw_iface *iface,
				u8 subtype, u16 reason)
{
	int idx;
	struct skw_peer_ctx *ctx;
	struct skw_core *skw = wiphy_priv(wiphy);
	u32 peer_map = atomic_read(&iface->peer_map);

	while (peer_map) {
		idx = ffs(peer_map) - 1;
		SKW_CLEAR(peer_map, BIT(idx));

		ctx = &skw->hw.lmac[iface->lmac_id].peer_ctx[idx];
		if (!ctx)
			continue;

		skw_sap_del_sta(wiphy, iface->ndev, ctx, subtype, reason);
	}
}

static int skw_stop_ap(struct wiphy *wiphy, struct net_device *dev,
			unsigned int link_id)
{
	int ret = 0;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "ndev: %s, link id: %d\n", netdev_name(dev), link_id);

	if (iface->skw->hw.bus == SKW_BUS_PCIE)
		skw_edma_dec_refill((void *)iface->skw, iface->lmac_id);

	netif_carrier_off(dev);

	skw_sap_flush_sta(wiphy, iface, 12, SKW_LEAVE);

	// set flag for tx thread to filter out skb in tx cache
	// mutex_lock(&skw->txrx.lock);
	// SKW_CLEAR(skw->txrx.tx_map, BIT(iface->id));
	// mutex_unlock(&skw->txrx.lock);

	// WARN_ON(iface->sta_list.count);
	skw_purge_key_conf(&iface->key_conf);
	skw_recovery_data_clear(iface);

	SKW_SET(iface->flags, SKW_IFACE_FLAG_DEAUTH);

	skw_dfs_deinit(wiphy, dev);

	ret = skw_send_msg(wiphy, dev, SKW_CMD_STOP_AP, NULL, 0, NULL, 0);
	if (ret) {
		SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_DEAUTH);
		skw_chip_err(skw->idx, "failed, ret = %d\n", ret);
		return ret;
	}

	SKW_KFREE(iface->sap.acl);
	SKW_KFREE(iface->sap.cfg.ht_cap);
	SKW_KFREE(iface->sap.cfg.vht_cap);
	SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_AP_STARTED);

	skw_lmac_unbind_iface(wiphy_priv(wiphy), iface->lmac_id, iface->id);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
static int skw_change_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_ap_update *info)
#else
static int skw_change_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_beacon_data *bcn)
#endif
{
	int ret = -1;
	int total, fixed, offset = 0;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_beacon_params *param = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15))
	struct cfg80211_beacon_data *bcn = &(info->beacon);
#endif

	skw_chip_dbg(skw->idx, "dev: %s\n", netdev_name(dev));
	if (!bcn)
		return -EINVAL;

	fixed = sizeof(struct skw_beacon_params);
	total = fixed +
		bcn->head_len +
		bcn->tail_len +
		bcn->probe_resp_len;

	param = SKW_ZALLOC(total, GFP_KERNEL);
	if (!param) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	if (bcn->head) {
		skw_hex_dump("beacon_head", bcn->head, bcn->head_len, false);

		param->beacon_head_len = bcn->head_len;
		param->beacon_head_offset = fixed + offset;
		memcpy(param->ies + offset, bcn->head, bcn->head_len);
		offset += bcn->head_len;
	}

	if (bcn->tail) {
		skw_hex_dump("beacon_tail", bcn->tail, bcn->tail_len, false);

		param->beacon_tail_offset = fixed + offset;
		param->beacon_tail_len = bcn->tail_len;
		memcpy(param->ies + offset, bcn->tail, bcn->tail_len);
		offset += bcn->tail_len;
	}

	if (bcn->probe_resp) {
		skw_hex_dump("probe_resp", bcn->probe_resp, bcn->probe_resp_len, false);

		param->probe_rsp_offset = fixed + offset;
		param->probe_rsp_len = bcn->probe_resp_len;
		memcpy(param->ies + offset, bcn->probe_resp,
				bcn->probe_resp_len);
		offset += bcn->probe_resp_len;

		if (iface->sap.probe_resp) {
			memcpy(iface->sap.probe_resp, bcn->probe_resp,
				bcn->probe_resp_len);

			iface->sap.probe_resp_len = bcn->probe_resp_len;
		}
	}

	ret = skw_send_msg(wiphy, dev, SKW_CMD_CHANGE_BEACON,
			param, total, NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	SKW_KFREE(param);

	return ret;
}

void skw_set_state(struct skw_core *skw, struct skw_sm *sm, enum SKW_STATES state)
{
	skw_chip_log(skw->idx, SKW_STATE, "[%s] inst: %d, %s -> %pM, state: %s -> %s\n",
		SKW_TAG_STATE, sm->inst, skw_iftype_name(sm->iface_iftype),
		sm->addr, skw_state_name(sm->state), skw_state_name(state));

	sm->state = state;
}

int skw_change_station(struct wiphy *wiphy, struct net_device *dev,
			const u8 *mac, struct station_parameters *params)
{
	struct skw_iface *iface = netdev_priv(dev);
	u32 flags_set = params->sta_flags_set;
	struct skw_peer_ctx *ctx = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s(%s), mac: %pM, flags_set: 0x%x\n",
		netdev_name(dev), skw_iftype_name(dev->ieee80211_ptr->iftype),
		mac, params->sta_flags_set);

	ctx = skw_peer_ctx(iface, mac);
	if (!ctx)
		return -EINVAL;

	skw_peer_ctx_lock(ctx);

	switch (dev->ieee80211_ptr->iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_ADHOC:

		if (flags_set & BIT(NL80211_STA_FLAG_ASSOCIATED)) {
			__skw_peer_ctx_transmit(ctx, true);
			skw_set_state(skw, &ctx->peer->sm, SKW_STATE_ASSOCED);

			if (iface->sap.cfg.crypto.n_akm_suites == 0)
				flags_set |= BIT(NL80211_STA_FLAG_AUTHORIZED);

		}

		if (flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
			skw_set_state(skw, &ctx->peer->sm, SKW_STATE_COMPLETED);
			atomic_set(&ctx->peer->rx_filter, SKW_RX_FILTER_NONE);
		}

		break;

	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		if (flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
			skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_COMPLETED);
			atomic_set(&ctx->peer->rx_filter, SKW_RX_FILTER_NONE);
			skw_set_ip_to_fw(wiphy, dev);
		}

		break;

	default:
		break;
	}

	skw_peer_ctx_unlock(ctx);

	return 0;
}

static int skw_cfg80211_change_station(struct wiphy *wiphy, struct net_device *dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
			const u8 *mac,
#else
			u8 *mac,
#endif
			struct station_parameters *params)
{
	return skw_change_station(wiphy, dev, (const u8 *)mac, params);
}

static int skw_set_sta_wep_key(struct wiphy *wiphy, struct skw_iface *iface,
			const u8 *mac, enum SKW_KEY_TYPE key_type)
{
	int idx;
	struct skw_key_params key_params;
	struct skw_key *key;
	struct skw_key_conf *conf = &iface->key_conf;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "addr: %pM, key type: %d\n", mac, key_type);

	memset(&key_params, 0x0, sizeof(key_params));

	idx = skw_key_idx(conf->installed_bitmap);
	if (idx == SKW_INVALID_ID)
		return -EINVAL;

	rcu_read_lock();
	key = rcu_dereference(conf->key[idx]);
	rcu_read_unlock();

	key_params.cipher_type = conf->skw_cipher;
	key_params.key_id = idx;
	key_params.key_len = key->key_len;
	key_params.key_type = key_type;

	memcpy(key_params.key, key->key_data, key->key_len);
	skw_ether_copy(key_params.mac_addr, mac);

	if (is_skw_ap_mode(iface) && iface->buf_keys_idx < SKW_MAX_BUF_KEYS &&
		!SKW_TEST(iface->flags, SKW_IFACE_FLAG_AP_STARTED)) {
		memcpy(&iface->buf_keys[iface->buf_keys_idx++],
			 &key_params, sizeof(key_params));
		SKW_SET(iface->flags, SKW_IFACE_FLAG_BUF_KEY);
		skw_chip_dbg(skw->idx, "lead key buffered, idx: %d\n", iface->buf_keys_idx - 1);
		return 0;
	} else
		return skw_send_msg(wiphy, iface->ndev, SKW_CMD_ADD_KEY,
			&key_params, sizeof(key_params), NULL, 0);
}

int skw_cmd_del_sta(struct wiphy *wiphy, struct net_device *dev,
		const u8 *mac, u8 type, u16 reason, bool tx_frame)
{
	struct skw_del_sta_param params;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s: addr: %pM, reason: %d, tx frame: %d\n",
		netdev_name(dev), mac, reason, tx_frame);

	params.reason_code = reason;
	skw_ether_copy(params.mac, mac);
	params.tx_frame = tx_frame;

	return  skw_send_msg(wiphy, dev, SKW_CMD_DEL_STA, &params,
			    sizeof(params), NULL, 0);
}

int skw_del_station(struct wiphy *wiphy, struct net_device *dev,
			const u8 *mac, u8 subtype, u16 reason)
{
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "subtype: %d, reason: %d, mac: %pM\n", subtype, reason, mac);

	if (!mac || is_broadcast_ether_addr(mac)) {
		skw_sap_flush_sta(wiphy, iface, subtype, reason);

		return 0;
	}

	ctx = skw_peer_ctx(iface, mac);
	if (!ctx)
		return -ENOENT;

	return skw_sap_del_sta(wiphy, dev, ctx, subtype, reason);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
static int skw_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev,
			   struct station_del_parameters *params)
{
	return skw_del_station(wiphy, dev, params->mac,
			params->subtype, params->reason_code);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static int skw_cfg80211_del_station(struct wiphy *wiphy,
		struct net_device *dev, const u8 *mac)
{
	return skw_del_station(wiphy, dev, mac,
				12,  /* Deauth */
				WLAN_REASON_DEAUTH_LEAVING);
}
#else
static int skw_cfg80211_del_station(struct wiphy *wiphy,
		struct net_device *dev, u8 *mac)
{
	return skw_del_station(wiphy, dev, (const u8 *)mac,
				12,  /* Deauth */
				WLAN_REASON_DEAUTH_LEAVING);
}
#endif

int skw_add_station(struct wiphy *wiphy, struct net_device *dev,
		    const u8 *mac, struct station_parameters *params)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_peer_ctx *ctx;
	struct skw_peer *peer;
	int ret;
	u8 idx;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "ndev: %s, mac: %pM, flags: 0x%x\n",
		netdev_name(dev), mac, params->sta_flags_set);

	ctx = skw_peer_ctx(iface, mac);
	if (!ctx) {
		peer = skw_peer_alloc();
		if (!peer) {
			skw_chip_err(skw->idx, "failed, addr: %pM\n", mac);
			return -ENOMEM;
		}

		ret = skw_send_msg(wiphy, dev, SKW_CMD_ADD_STA, (void *)mac,
				   ETH_ALEN, &idx, sizeof(idx));
		if (ret) {
			skw_chip_err(skw->idx, "command failed, addr: %pM, ret: %d\n",
				mac, ret);

			SKW_KFREE(peer);
			return ret;
		}

		skw_peer_init(peer, mac, idx);
		ctx = skw_get_ctx(iface->skw, iface->lmac_id, idx);
		ret = skw_peer_ctx_bind(iface, ctx, peer);
		if (ret) {
			skw_cmd_del_sta(wiphy, dev, mac, 12, SKW_LEAVE, false);
			SKW_KFREE(peer);
			return -EINVAL;
		}
	}

	skw_peer_ctx_lock(ctx);

	__skw_peer_ctx_transmit(ctx, false);
	skw_set_state(skw, &ctx->peer->sm, SKW_STATE_AUTHED);

	skw_peer_ctx_unlock(ctx);

	if (iface->key_conf.flags & SKW_KEY_FLAG_WEP_SHARE)
		skw_set_sta_wep_key(wiphy, iface, mac, SKW_KEY_TYPE_PTK);

	return 0;
}

static int skw_cfg80211_add_station(struct wiphy *wiphy, struct net_device *dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
		    const u8 *mac,
#else
		    u8 *mac,
#endif
		    struct station_parameters *params)
{
	return skw_add_station(wiphy, dev, (const u8 *)mac, params);
}

static void skw_set_rate_info(struct skw_rate *rate, struct rate_info *rinfo)
{
#if 0
	skw_dbg("flags: %d, mcs: %d, bw: %d, gi: %d, nss: %d, he_ru: %d\n",
		rate->flags, rate->mcs_idx, rate->bw,
		rate->gi, rate->nss, rate->he_ru);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	switch (rate->bw) {
	case SKW_RATE_INFO_BW_40:
		rinfo->bw = RATE_INFO_BW_40;
		break;

	case SKW_RATE_INFO_BW_80:
		rinfo->bw = RATE_INFO_BW_80;
		break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	case SKW_RATE_INFO_BW_HE_RU:
		rinfo->bw = RATE_INFO_BW_HE_RU;
		rinfo->he_ru_alloc = rate->he_ru;
		break;
#endif
	default:
		rinfo->bw = RATE_INFO_BW_20;
		break;
	}
#endif

	rinfo->flags = 0;
	switch (rate->flags) {
	case SKW_RATE_INFO_FLAGS_HT:
		rinfo->mcs = rate->mcs_idx;

		rinfo->flags |= RATE_INFO_FLAGS_MCS;
		if (rate->gi)
			rinfo->flags |= RATE_INFO_FLAGS_SHORT_GI;

		break;

	case SKW_RATE_INFO_FLAGS_VHT:
		rinfo->mcs = rate->mcs_idx;
		rinfo->nss = rate->nss;

		rinfo->flags |= RATE_INFO_FLAGS_VHT_MCS;
		if (rate->gi)
			rinfo->flags |= RATE_INFO_FLAGS_SHORT_GI;

		break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	case SKW_RATE_INFO_FLAGS_HE:
		rate->gi = skw_gi_to_nl80211_info_gi(rate->gi);
		rinfo->mcs = rate->mcs_idx;
		rinfo->nss = rate->nss;
		rinfo->he_gi = rate->gi;
		rinfo->he_dcm = rate->he_dcm;
		rinfo->flags |= RATE_INFO_FLAGS_HE_MCS;
		break;
#endif
	default:
		rinfo->legacy = rate->legacy_rate;
		break;
	}
}

static int skw_desc_get_rx_rate(struct skw_core *skw, struct skw_rate *rate, u8 bw, u8 mode, u8 gi,
		    u8 nss, u8 dcm, u16 data_rate)
{
	u16 skw_supp_bs_rate[] = {
		20, 55, 110,
		/*2M,5.5M,11M*/
	};
	u16 skw_supp_bl_rate[] = {
		10, 20, 55, 110,
		/*1M,2M,5.5M,11M*/
	};
	u16 skw_supp_g_rate[] = {
		60, 90, 120, 180, 240, 360, 480, 540,
		/*6M, 9M, 12M, 18M, 24M, 36M, 48M, 54M*/
	};

	memset(rate, 0x0, sizeof(struct skw_rate));

	rate->bw = bw;
	rate->nss = nss;

	switch (mode) {
	case SKW_PPDUMODE_HT_MIXED:
		rate->flags = SKW_RATE_INFO_FLAGS_HT;
		rate->mcs_idx = 0x3F & data_rate;
		rate->gi = gi;
		break;

	case SKW_PPDUMODE_VHT_SU:
	case SKW_PPDUMODE_VHT_MU:
		rate->flags = SKW_RATE_INFO_FLAGS_VHT;
		rate->mcs_idx = 0xF & data_rate;
		rate->gi = gi;
		break;

	case SKW_PPDUMODE_HE_SU:
	case SKW_PPDUMODE_HE_TB:
	case SKW_PPDUMODE_HE_ER_SU:
	case SKW_PPDUMODE_HE_MU:
		rate->flags = SKW_RATE_INFO_FLAGS_HE;
		rate->mcs_idx = 0xF & data_rate;

		if (dcm) {
			rate->mcs_idx = 0x3 & data_rate;
			rate->he_dcm = dcm;
		} else if (mode == SKW_PPDUMODE_HE_ER_SU) {
			rate->mcs_idx = 0x3 & data_rate;
		}

		rate->gi = gi;

		if (bw != SKW_DESC_BW_USED_RU)
			rate->he_ru = bw + 3;
		break;

	case SKW_PPDUMODE_11B_SHORT:
		rate->flags = SKW_RATE_INFO_FLAGS_LEGACY;

		if (data_rate < sizeof(skw_supp_bs_rate))
			rate->legacy_rate = skw_supp_bs_rate[data_rate];
		else
			skw_chip_warn(skw->idx, "illegal 11B_SHORT rate:%d\n", data_rate);
		break;

	case SKW_PPDUMODE_11B_LONG:
		rate->flags = SKW_RATE_INFO_FLAGS_LEGACY;

		if (data_rate < sizeof(skw_supp_bl_rate))
			rate->legacy_rate = skw_supp_bl_rate[data_rate];
		else
			skw_chip_warn(skw->idx, "illegal 11B_LONG rate:%d\n", data_rate);
		break;

	case SKW_PPDUMODE_11G:
		rate->flags = SKW_RATE_INFO_FLAGS_LEGACY;

		if (data_rate < sizeof(skw_supp_g_rate))
			rate->legacy_rate = skw_supp_g_rate[data_rate];
		else
			skw_chip_warn(skw->idx, "illegal 11G rate:%d\n", data_rate);
		break;

	default:
		skw_chip_warn(skw->idx, "unsupport ppdu mode:%d\n", mode);
		break;
	};

	return 0;
}

static int skw_get_station(struct wiphy *wiphy, struct net_device *dev,
			   const u8 *mac, struct station_info *sinfo)
{
	u64 ts;
	int ret = -1;
	struct skw_peer_ctx *ctx;
	struct skw_station_params params = {0};
	struct skw_get_sta_resp get_sta_resp;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	// skw_dbg("dev: %s, mac: %pM\n", netdev_name(dev), mac);

	if (!mac)
		return 0;

	ctx = skw_peer_ctx(iface, mac);
	if (!ctx)
		return -ENOENT;

	memset(&get_sta_resp, 0, sizeof(get_sta_resp));

	ts = skw_local_clock();
	do_div(ts, 1000000);
	params.timestamp = ts;
	skw_ether_copy(params.mac, mac);

	ret = skw_send_msg(wiphy, dev, SKW_CMD_GET_STA, &params,
			   sizeof(params), &get_sta_resp,
			   sizeof(struct skw_get_sta_resp));
	if (ret) {
		skw_chip_warn(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	sinfo->tx_failed = get_sta_resp.tx_failed;
	sinfo->filled |= SKW_COMPAT_TX_FAILED;

	sinfo->signal = get_sta_resp.signal;
	sinfo->filled |= SKW_COMPAT_SIGNAL;

	if (is_skw_sta_mode(iface)) {
		sinfo->tx_packets = dev->stats.tx_packets;
		sinfo->filled |= SKW_COMPAT_TX_PACKETS;

		sinfo->tx_bytes = dev->stats.tx_bytes;
		sinfo->filled |= SKW_COMPAT_TX_BYTES;

		sinfo->rx_packets = dev->stats.rx_packets;
		sinfo->filled |= SKW_COMPAT_RX_PACKETS;

		sinfo->rx_bytes = dev->stats.rx_bytes;
		sinfo->filled |= SKW_COMPAT_RX_BYTES;
	}

	skw_set_rate_info(&get_sta_resp.tx_rate, &sinfo->txrate);
	sinfo->filled |= SKW_COMPAT_TX_BITRATE;

	skw_peer_ctx_lock(ctx);

	if (ctx->peer) {
		ctx->peer->tx.rssi = sinfo->signal;

		skw_hex_dump("get_sta_rx_rate:", &get_sta_resp.rx_rate,
			sizeof(get_sta_resp.rx_rate), false);

		skw_desc_get_rx_rate(skw, &ctx->peer->rx.rate, get_sta_resp.rx_rate.bw,
			get_sta_resp.rx_rate.ppdu_mode,
			skw_desc_gi_to_skw_gi(get_sta_resp.rx_rate.gi_type,
				get_sta_resp.rx_rate.ppdu_mode),
			skw_desc_nss_to_nss_num(get_sta_resp.rx_rate.nss),
			get_sta_resp.rx_rate.dcm,
			get_sta_resp.rx_rate.data_rate);
		skw_set_rate_info(&ctx->peer->rx.rate, &sinfo->rxrate);

		sinfo->filled |= SKW_COMPAT_RX_BITRATE;

		memcpy(&ctx->peer->tx.rate, &get_sta_resp.tx_rate,
			 sizeof(struct skw_rate));

		ctx->peer->tx.tx_psr = get_sta_resp.tx_psr;
		ctx->peer->tx.tx_failed = get_sta_resp.tx_failed;

		memcpy(ctx->peer->rx.filter_cnt,
			get_sta_resp.filter_cnt, sizeof(get_sta_resp.filter_cnt));
		memcpy(ctx->peer->rx.filter_drop_offload_cnt,
			get_sta_resp.filter_drop_offload_cnt,
			sizeof(get_sta_resp.filter_drop_offload_cnt));

		ctx->peer->tx.percent = get_sta_resp.tx_percent;
		ctx->peer->rx.percent = get_sta_resp.rx_percent;

		if (is_skw_ap_mode(iface)) {
			sinfo->tx_packets = ctx->peer->tx.pkts;
			sinfo->tx_bytes = ctx->peer->tx.bytes;
			sinfo->rx_packets = ctx->peer->rx.pkts;
			sinfo->rx_bytes = ctx->peer->rx.bytes;
			sinfo->filled |= SKW_COMPAT_TX_PACKETS | SKW_COMPAT_TX_BYTES |
				         SKW_COMPAT_RX_PACKETS | SKW_COMPAT_RX_BYTES;
		}
	}

	skw_peer_ctx_unlock(ctx);

//	skw_dbg("tx packets:%u tx_bytes:%llu rx_packets:%u rx_bytes:%llu\n",
//		sinfo->tx_packets, sinfo->tx_bytes,
//		sinfo->rx_packets, sinfo->rx_bytes);

	return ret;
}

static int skw_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
			   const u8 *mac,
#else
			   u8 *mac,
#endif
			   struct station_info *sinfo)
{
	return skw_get_station(wiphy, dev, (const u8 *)mac, sinfo);
}

static void skw_scan_timeout(void *data)
{
	struct skw_iface *iface = data;
	struct skw_core *skw = iface->skw;

	if (unlikely(!iface)) {
		skw_chip_warn(skw->idx, "iface is NULL\n");
		return;
	}

	skw_queue_work(priv_to_wiphy(iface->skw), iface,
			SKW_WORK_SCAN_TIMEOUT, NULL, 0);
}

static bool skw_cqm_bg_scan(struct skw_iface *iface,
		struct cfg80211_scan_request *req, u16 *target_chn)
{
	bool ret;
	struct skw_core *skw = iface->skw;

	if (iface->wdev.iftype != NL80211_IFTYPE_STATION)
		return false;

	spin_lock_bh(&iface->sta.roam_data.lock);
	if (iface->sta.roam_data.flags & SKW_IFACE_STA_ROAM_FLAG_CQM_LOW &&
		iface->sta.core.sm.state == SKW_STATE_COMPLETED &&
		req->n_channels > 10 && req->n_ssids == 1 &&
		req->ssids != NULL && req->ssids->ssid_len != 0 &&
		req->ssids->ssid_len == iface->sta.core.bss.ssid_len &&
		memcmp(req->ssids->ssid, iface->sta.core.bss.ssid,
			req->ssids->ssid_len) == 0) {
		skw_chip_dbg(skw->idx, "only %d", iface->sta.roam_data.target_chn);
		*target_chn = iface->sta.roam_data.target_chn;
		iface->sta.roam_data.flags &= ~SKW_IFACE_STA_ROAM_FLAG_CQM_LOW;
		skw_del_timer_work(iface->skw, skw_cqm_scan_timeout);
		ret = true;
	} else {
		*target_chn = 0;
		ret = false;
	}

	spin_unlock_bh(&iface->sta.roam_data.lock);

	return ret;
}

static bool is_skw_6ghz_non_psc_chan(struct ieee80211_channel *channel)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	if (channel->band != NL80211_BAND_6GHZ)
		return false;

	if (channel->hw_value % 16 != 5)
		return true;
#endif

	return false;
}

static int skw_scan(struct wiphy *wiphy, struct cfg80211_scan_request *req)
{
	int i, ret;
	struct skw_scan_chan_info *chan;
	int size, nssids_size, offset;
	u16 roam_chn = 0;
	u16 scan_chn_num = 0;
	char *buff = NULL;
	struct skw_scan_param *param = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(req->wdev);

	skw_chip_dbg(skw->idx, "%s: chip: %d, nr_chan: %d, n_ssids: %d, ie_len: %zd\n",
		skw_iftype_name(req->wdev->iftype), skw->idx,
		req->n_channels, req->n_ssids, req->ie_len);

	size = sizeof(struct skw_scan_param) +
	       req->n_channels * sizeof(*chan) +
	       req->n_ssids * sizeof(struct cfg80211_ssid) +
	       req->ie_len;

	buff = SKW_ZALLOC(size, GFP_KERNEL);
	if (!buff) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", size);
		return -ENOMEM;
	}

	offset = 0;

	param = (struct skw_scan_param *)buff;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	if (req->flags & NL80211_SCAN_FLAG_RANDOM_ADDR &&
	    iface->wdev.iftype == NL80211_IFTYPE_STATION) {
		param->flags |= SKW_SCAN_FLAG_RND_MAC;

		get_random_mask_addr(param->rand_mac,
				     req->mac_addr,
				     req->mac_addr_mask);
	}
#endif

	if (iface->wdev.iftype == NL80211_IFTYPE_AP) {
		param->flags |= SKW_SCAN_FLAG_ACS;
		skw_purge_survey_data(iface);
	}

	offset += sizeof(struct skw_scan_param);
	param->chan_offset = offset;

	chan = (struct skw_scan_chan_info *)(buff + offset);

	skw_cqm_bg_scan(iface, req, &roam_chn);

	for (i = 0; i < req->n_channels; i++) {
		if (unlikely(iface->extend.scan_band_filter)) {
			if (!(iface->extend.scan_band_filter & BIT(req->channels[i]->band)))
				continue;
		}

		if (unlikely(roam_chn && roam_chn != req->channels[i]->hw_value))
			continue;

		if (is_skw_6ghz_non_psc_chan(req->channels[i]))
			continue;

		chan->band = to_skw_band(req->channels[i]->band);
		chan->chan_num = req->channels[i]->hw_value;

		if ((req->channels[i]->flags & SKW_PASSIVE_SCAN) ||
		    (!req->n_ssids && is_skw_sta_mode(iface)))
			chan->scan_flags |= SKW_SCAN_FLAG_PASSIVE;

		scan_chn_num++;
		chan++;
	}

	param->nr_chan = scan_chn_num;
	offset += scan_chn_num * sizeof(*chan);

	param->n_ssid = req->n_ssids;
	if (req->n_ssids) {
		nssids_size = req->n_ssids * sizeof(struct cfg80211_ssid);
		memcpy(buff + offset, req->ssids, nssids_size);
		param->ssid_offset = offset;
		offset += nssids_size;
	}

	if (req->ie_len) {
		memcpy(buff + offset, req->ie, req->ie_len);
		param->ie_offset = offset;
		param->ie_len = req->ie_len;
	}

	skw->scan_req = req;
	skw->nr_scan_results = 0;

	skw_add_timer_work(skw, "scan_timeout", skw_scan_timeout, iface,
			SKW_SCAN_TIMEOUT, req, GFP_KERNEL);

	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_START_SCAN,
			   buff, size, NULL, 0);
	if (ret) {
		skw->scan_req = NULL;
		skw_del_timer_work(skw, req);
		skw_chip_dbg(skw->idx, "failed, ret: %d\n", ret);
	}

	SKW_KFREE(buff);

	return ret;
}

void skw_scan_done(struct skw_core *skw, struct skw_iface *iface, bool aborted)
{
	struct cfg80211_scan_request *scan_req;
	int ret_val;

	mutex_lock(&skw->lock);

	if (!skw->scan_req)
		goto ret;

	if (&iface->wdev != skw->scan_req->wdev)
		goto ret;

	skw_chip_dbg(skw->idx, "inst: %d, aborted: %d, scan result: %d\n",
		iface->id, aborted, skw->nr_scan_results);

	scan_req = skw->scan_req;
	skw->scan_req = NULL;

	skw_del_timer_work(skw, scan_req);

	if (aborted) {
		ret_val = skw_msg_xmit(priv_to_wiphy(skw), iface->id,
				SKW_CMD_STOP_SCAN, NULL, 0, NULL, 0);
		if (ret_val)
			skw_chip_warn(skw->idx, "failed, return: %d\n", ret_val);
	}

	skw_compat_scan_done(scan_req, aborted);

ret:
	mutex_unlock(&skw->lock);
}

static void skw_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	struct skw_core *skw = wiphy_priv(wiphy);
	int ret;

	skw_chip_dbg(skw->idx, "inst: %d, scaning: %d\n", iface->id, !!skw->scan_req);

	if (!skw->scan_req)
		return;

	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_STOP_SCAN, NULL, 0, NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	skw_scan_done(skw, iface, false);
}

static int skw_mbssid_index(struct skw_core *skw, struct cfg80211_bss *bss)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0))
	return skw_bss_priv(bss)->bssid_index;
#else
	return bss->bssid_index;
#endif
}

static int skw_mbssid_max_indicator(struct skw_core *skw,
				struct cfg80211_bss *bss)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0))
	return skw_bss_priv(bss)->max_bssid_indicator;
#else
	return bss->max_bssid_indicator;
#endif
}

const u8 *skw_bss_get_ext_ie(struct cfg80211_bss *bss, u8 ext_eid)
{
	const struct cfg80211_bss_ies *ies;

	ies = rcu_dereference(bss->ies);
	if (!ies)
		return NULL;

	return skw_find_ie_match(SKW_WLAN_EID_EXTENSION, ies->data,
				 ies->len, &ext_eid, 1, 2);
}

static void skw_parse_center_chn(struct skw_core *skw, struct cfg80211_bss *bss, int *he_enable,
				 struct skw_center_chn *cc)
{
	unsigned int diff;
	const u8 *ht_ie, *vht_ie;
	u8 vht_seg0_idx, vht_seg1_idx;
	struct ieee80211_ht_operation *ht_oper;
	struct ieee80211_vht_operation *vht_oper;
	const u8 *he_ie;
	struct skw_he_cap_elem *he_cap;

	cc->bw = SKW_CHAN_WIDTH_20;
	cc->center_chn1 = bss->channel->hw_value;
	cc->center_chn2 = 0;

	*he_enable = 1;

	if (WARN_ON(!bss))
		return;

	rcu_read_lock();

	ht_ie = ieee80211_bss_get_ie(bss, WLAN_EID_HT_OPERATION);
	if (ht_ie && ht_ie[1]) {
		ht_oper = (struct ieee80211_ht_operation *)(ht_ie + 2);

		cc->center_chn2 = 0;

		switch (ht_oper->ht_param & 0x3) {
		case IEEE80211_HT_PARAM_CHA_SEC_NONE:
			cc->bw = SKW_CHAN_WIDTH_20;
			cc->center_chn1 = ht_oper->primary_chan;

			break;

		case IEEE80211_HT_PARAM_CHA_SEC_ABOVE:
			cc->bw = SKW_CHAN_WIDTH_40;
			cc->center_chn1 = ht_oper->primary_chan + 2;
			break;

		case IEEE80211_HT_PARAM_CHA_SEC_BELOW:
			cc->bw = SKW_CHAN_WIDTH_40;
			cc->center_chn1 = ht_oper->primary_chan - 2;
			break;

		default:
			break;
		}
	}

	vht_ie = ieee80211_bss_get_ie(bss, WLAN_EID_VHT_OPERATION);
	if (vht_ie && vht_ie[1]) {
		vht_oper = (struct ieee80211_vht_operation *)(vht_ie + 2);
		cc->center_chn2 = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
		vht_seg0_idx = vht_oper->center_freq_seg0_idx;
		vht_seg1_idx = vht_oper->center_freq_seg1_idx;
#else
		vht_seg0_idx = vht_oper->center_freq_seg1_idx;
		vht_seg1_idx = vht_oper->center_freq_seg2_idx;
#endif
		switch (vht_oper->chan_width) {
		case IEEE80211_VHT_CHANWIDTH_80MHZ:
			cc->bw = SKW_CHAN_WIDTH_80;
			cc->center_chn1 = vht_seg0_idx;

			if (vht_seg1_idx) {
				diff = abs(vht_seg1_idx - vht_seg0_idx);
				if (diff == 8) {
					cc->bw = SKW_CHAN_WIDTH_160;
					cc->center_chn1 = vht_seg1_idx;
				} else if (diff > 8) {
					cc->bw = SKW_CHAN_WIDTH_80P80;
					cc->center_chn2 = vht_seg1_idx;
				}
			}

			break;

		case IEEE80211_VHT_CHANWIDTH_160MHZ:
			cc->bw = SKW_CHAN_WIDTH_160;
			cc->center_chn1 = vht_seg0_idx;
			break;

		case IEEE80211_VHT_CHANWIDTH_80P80MHZ:
			cc->bw = SKW_CHAN_WIDTH_80P80;
			cc->center_chn1 = vht_seg0_idx;
			cc->center_chn2 = vht_seg1_idx;
			break;

		default:
			break;
		}
	}

	he_ie = skw_bss_get_ext_ie(bss, SKW_WLAN_EID_EXT_HE_CAPABILITY);
	if (he_ie && he_ie[1]) {
		skw_hex_dump("he capa", he_ie, he_ie[1] + 2, false);

		/* 802.11ax D3.0 */
		he_cap = (struct skw_he_cap_elem *)(he_ie + 3); // ID: 1 + len: 1 + Num: 1

		skw_chip_dbg(skw->idx, "band: %d, ppe: 0x%x, phy_cap_info[0]: 0x%x\n",
			bss->channel->band, he_cap->ppe, he_cap->phy_cap_info[0]);

		if ((he_cap->phy_cap_info[6] & 0x80) == 0x80 &&
		    (he_cap->ppe & 0x78) == 0x60) { // check BIT[3:6]
			switch (bss->channel->band) {
			case NL80211_BAND_2GHZ:
				*he_enable = 0;
				break;

			case NL80211_BAND_5GHZ:
				if (!(he_cap->phy_cap_info[0] &
				    SKW_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G))
					*he_enable = 0;
				break;

			default:
				break;
			}
		}
	}

	cc->band = to_skw_band(bss->channel->band);
	skw_chip_dbg(skw->idx, "cc->bw:%d cc->band:%d\n", cc->bw, cc->band);

	rcu_read_unlock();
}

static int skw_cmd_join(struct wiphy *wiphy, struct net_device *ndev,
			struct cfg80211_bss *bss, u32 bw, u8 band,
			u16 center_chn1, u16 center_chn2,
			bool roaming, struct skw_join_resp *resp)
{
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_join_param *params;
	int ret = 0, size = 0;

	skw_chip_dbg(skw->idx, "bssid: %pM(idx: %d, ind: %d), chn: %d(%d, %d), bw: %d band: %d\n",
		bss->bssid, skw_mbssid_index(skw, bss),
		skw_mbssid_max_indicator(skw, bss),
		bss->channel->hw_value,
		center_chn1, center_chn2, bw, band);

	size = sizeof(struct skw_join_param) + bss->ies->len;
	params = SKW_ZALLOC(size, GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	params->bandwidth = bw;
	params->band = band;
	params->center_chn1 = center_chn1;
	params->center_chn2 = center_chn2;
	params->chan_num = bss->channel->hw_value;

	params->reserved = 0;
	params->roaming = !!roaming;
	params->capability = bss->capability;
	params->beacon_interval = bss->beacon_interval;
	params->bssid_index = skw_mbssid_index(skw, bss);
	params->max_bssid_indicator = skw_mbssid_max_indicator(skw, bss);
	memcpy(params->bssid, bss->bssid, ETH_ALEN);

	if (bss->ies->len) {
		memcpy(params->bss_ie, bss->ies->data, bss->ies->len);
		params->bss_ie_offset = sizeof(struct skw_join_param);
		params->bss_ie_len = bss->ies->len;
	}

	skw_edma_mask_irq(skw, iface->lmac_id);
	ret = skw_send_msg(wiphy, ndev, SKW_CMD_JOIN, params,
			   size, resp, sizeof(*resp));
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	SKW_KFREE(params);

	return ret;
}

int skw_cmd_unjoin(struct wiphy *wiphy, struct net_device *ndev,
		   const u8 *addr, u16 reason, bool tx_frame)
{
	int ret;
	struct skw_disconnect_param params;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, bssid: %pM, reason: %d\n",
		netdev_name(ndev), addr, reason);

	memset(&params, 0x0, sizeof(params));

	params.type = SKW_DISCONNECT_ONLY;
	params.reason_code = reason;
	params.local_state_change = !tx_frame;

	if (tx_frame)
		params.type = SKW_DISCONNECT_SEND_DEAUTH;

	ret = skw_send_msg(wiphy, ndev, SKW_CMD_DISCONNECT, &params,
			   sizeof(params), NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	return ret;
}

int skw_cmd_monitor(struct wiphy *wiphy, struct cfg80211_chan_def *chandef, u8 mode)
{
	int ret = 0;
	struct skw_set_monitor_param param = {0};

	param.mode = mode;
	switch (param.mode) {
	case SKW_MONITOR_CLOSE:
		break;
	case SKW_MONITOR_COMMON:
	case SKW_MONITOR_MAC_CAP:
	case SKW_MONITOR_PHY_CAP:
		if (chandef == NULL || chandef->chan == NULL)
			return -EINVAL;
		param.chan_num = chandef->chan->hw_value;
		param.center_chn1 = skw_freq_to_chn(chandef->center_freq1);
		param.center_chn2 = skw_freq_to_chn(chandef->center_freq2);

		param.bandwidth = to_skw_bw(chandef->width);

		break;

	default:
		return -EINVAL;
	}

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SET_MONITOR_PARAM,
		&param, sizeof(struct skw_set_monitor_param), NULL, 0);

	return ret;
}

static int skw_cmd_auth(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_auth_request *req)
{
	int ret = 0;
	u16 auth_alg;
	int size, offset;
	struct skw_auth_param *params = NULL;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	const u8 *auth_data = req->auth_data;
	size_t auth_data_len = req->auth_data_len;
#else
	const u8 *auth_data = req->sae_data;
	size_t auth_data_len = req->sae_data_len;
#endif

	switch (req->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		auth_alg = WLAN_AUTH_OPEN;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		auth_alg = WLAN_AUTH_SHARED_KEY;
		break;
	case NL80211_AUTHTYPE_FT:
		auth_alg = WLAN_AUTH_FT;
		break;
	case NL80211_AUTHTYPE_NETWORK_EAP:
		auth_alg = WLAN_AUTH_LEAP;
		break;
	case NL80211_AUTHTYPE_SAE:
		auth_alg = WLAN_AUTH_SAE;
		break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	case NL80211_AUTHTYPE_FILS_SK:
		auth_alg = WLAN_AUTH_FILS_SK;
		break;
	case NL80211_AUTHTYPE_FILS_SK_PFS:
		auth_alg = WLAN_AUTH_FILS_SK_PFS;
		break;
	case NL80211_AUTHTYPE_FILS_PK:
		auth_alg = WLAN_AUTH_FILS_PK;
		break;
#endif
	case NL80211_AUTHTYPE_AUTOMATIC:
		/*
		 * Fixme: try open wep first, then set share key after using
		 * open wep failed.
		 */
		auth_alg = WLAN_AUTH_OPEN;
		break;
	default:
		return -EOPNOTSUPP;
	}

	size = sizeof(struct skw_auth_param) +
	       req->ie_len +
	       auth_data_len;

	params = SKW_ZALLOC(size, GFP_KERNEL);
	if (!params) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", size);
		return -ENOMEM;
	}

	offset = sizeof(struct skw_auth_param);
	params->auth_algorithm = auth_alg;

	if (auth_data_len) {
		params->auth_data_offset = offset;
		params->auth_data_len = auth_data_len;

		memcpy((u8 *)params + offset, auth_data,
		       auth_data_len);

		offset += auth_data_len;
	}

	if (req->ie && req->ie_len) {
		params->auth_ie_offset = offset;
		params->auth_ie_len = req->ie_len;
		memcpy((u8 *)params + offset, req->ie, req->ie_len);

		offset += req->ie_len;
	}

	memcpy(iface->sta.core.pending.auth_cmd, params, size);
	iface->sta.core.pending.auth_cmd_len = size;

	ret = skw_msg_xmit_timeout(wiphy, SKW_NDEV_ID(dev), SKW_CMD_AUTH,
				params, size, NULL, 0, "SKW_CMD_AUTH",
				msecs_to_jiffies(SKW_CMD_TIMEOUT), 0);

	SKW_KFREE(params);

	return ret;
}

static inline void skw_oper_and_ht_capa(struct ieee80211_ht_cap *ht_capa,
		const struct ieee80211_ht_cap *ht_capa_mask)
{
	int i;
	u8 *p1, *p2;

	if (!ht_capa_mask) {
		memset(ht_capa, 0, sizeof(*ht_capa));
		return;
	}

	p1 = (u8 *)(ht_capa);
	p2 = (u8 *)(ht_capa_mask);
	for (i = 0; i < sizeof(*ht_capa); i++)
		p1[i] &= p2[i];
}

 /*  Do a logical ht_capa &= ht_capa_mask.  */
static inline void skw_oper_and_vht_capa(struct ieee80211_vht_cap *vht_capa,
				const struct ieee80211_vht_cap *vht_capa_mask)
{
	int i;
	u8 *p1, *p2;

	if (!vht_capa_mask) {
		memset(vht_capa, 0, sizeof(*vht_capa));
		return;
	}

	p1 = (u8 *)(vht_capa);
	p2 = (u8 *)(vht_capa_mask);
	for (i = 0; i < sizeof(*vht_capa); i++)
		p1[i] &= p2[i];
}

static int skw_cmd_assoc(struct wiphy *wiphy, struct net_device *dev,
			 struct cfg80211_assoc_request *req)
{
	int ret = 0;
	int size, offset;
	char *buff = NULL;
	struct skw_assoc_req_param *param = NULL;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	size = sizeof(struct skw_assoc_req_param) + req->ie_len;

	buff = SKW_ZALLOC(size, GFP_KERNEL);
	if (!buff) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", size);
		return -ENOMEM;
	}

	offset = 0;
	param = (struct skw_assoc_req_param *)buff;
	memcpy(&param->ht_capa, &req->ht_capa, sizeof(req->ht_capa));

	skw_oper_and_ht_capa(&param->ht_capa, &req->ht_capa_mask);
	memcpy(&param->vht_capa, &req->vht_capa, sizeof(req->vht_capa));

	skw_oper_and_vht_capa(&param->vht_capa, &req->vht_capa_mask);
	memcpy(param->bssid, req->bss->bssid, ETH_ALEN);

	if (req->prev_bssid)
		memcpy(param->pre_bssid, req->prev_bssid, ETH_ALEN);

	param->req_ie_len = req->ie_len;

	offset += sizeof(struct skw_assoc_req_param);
	param->req_ie_offset = offset;

	if (req->ie_len)
		memcpy(param->req_ie, req->ie, req->ie_len);

	memcpy(iface->sta.core.pending.assoc_cmd, buff, size);
	iface->sta.core.pending.assoc_cmd_len = size;

	ret = skw_msg_xmit_timeout(wiphy, SKW_NDEV_ID(dev), SKW_CMD_ASSOC,
				buff, size, NULL, 0, "SKW_CMD_ASSOC",
				msecs_to_jiffies(SKW_CMD_TIMEOUT), 0);

	SKW_KFREE(buff);

	return ret;
}

#if 0
static bool skw_bss_check_vendor_name(struct cfg80211_bss *bss, const u8 *oui)
{
	const struct cfg80211_bss_ies *ies;
	const struct skw_element *elem;
	bool ret = false;
	u8 eid = WLAN_EID_VENDOR_SPECIFIC;

	ies = rcu_dereference(bss->beacon_ies);
	if (!ies)
		return false;

	skw_hex_dump("beacon ies:", ies->data, ies->len, false);

	skw_foreach_element_id(elem, eid, ies->data, ies->len) {
		if (!memcmp(elem->data, oui, 3)) {
			ret = true;
			break;
		}
	}

	return ret;
}
#endif

static void skw_fix_compatibility_issues(struct wiphy *wiphy,
		struct skw_iface *iface, struct cfg80211_bss *bss,
		int he_enable, struct skw_center_chn *cc)
{
	struct net_device *ndev = iface->ndev;

	if (ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_STATION &&
	    ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_P2P_CLIENT)
		return;

	skw_set_mib_bool(wiphy, iface->id, SKW_MIB_DOT11_MODE_HE, !!he_enable);

#if 0 // disabled for lite
	rcu_read_lock();

	he_oper_ie = skw_bss_get_ext_ie(bss, SKW_WLAN_EID_EXT_HE_OPERATION);
	if (he_oper_ie) {
		he_oper = (struct skw_he_oper_elem *)(he_oper_ie + 3);

		if (he_oper->bss_color == 0 &&
		    (int)bss->channel->band == (int)NL80211_BAND_5GHZ &&
		    !(skw->fw.fw_bw_capa & (SKW_BW_5GHZ_80M | SKW_BW_5GHZ_160M | SKW_BW_5GHZ_8080M))) {
			if (he_oper->bss_color_disabled == 0) {
				skw_set_mib_bool(wiphy, iface->id,
						SKW_MIB_DOT11_MODE_VHT, false);
				skw_chip_info(skw->idx, "Disable VHT");
			} else {
				if (cc->bw >= SKW_CHAN_WIDTH_80 &&
				    skw_bss_check_vendor_name(bss, oui)) {
					skw_set_mib_bool(wiphy, iface->id,
							SKW_MIB_DOT11_MODE_HE, false);
					skw_chip_info(skw->idx, "Disable HE");

				}
			}
		}

	}

	rcu_read_unlock();
#endif
}

static int skw_join(struct wiphy *wiphy, struct net_device *ndev,
		    struct cfg80211_bss *bss, bool roaming)
{
	int ret = 0, he_enable;
	struct skw_peer *peer;
	struct skw_peer_ctx *ctx;
	struct skw_center_chn cc = {};
	struct skw_join_resp resp = {};
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_sta_core *core = &iface->sta.core;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_wdev_assert_lock(iface);

	peer = skw_peer_alloc();
	if (!peer) {
		skw_chip_err(skw->idx, "alloc peer failed\n");
		return -ENOMEM;
	}

	skw_parse_center_chn(skw, bss, &he_enable, &cc);
	skw_fix_compatibility_issues(wiphy, iface, bss, he_enable, &cc);

	SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_DEAUTH);
	ret = skw_cmd_join(wiphy, ndev, bss, cc.bw, cc.band, cc.center_chn1,
			   cc.center_chn2, roaming, &resp);
	if (ret < 0) {
		skw_chip_err(skw->idx, "command join failed, ret: %d\n", ret);
		SKW_KFREE(peer);

		return ret;
	}

	skw_peer_init(peer, bss->bssid, resp.peer_idx);
	ctx = skw_get_ctx(iface->skw, resp.lmac_id, resp.peer_idx);
	ret = skw_peer_ctx_bind(iface, ctx, peer);
	if (ret) {
		skw_cmd_unjoin(wiphy, ndev, bss->bssid, SKW_LEAVE, false);
		SKW_KFREE(peer);
		return -EFAULT;
	}

	skw_join_resp_handler(wiphy_priv(wiphy), iface, &resp);

	skw_ether_copy(core->bss.bssid, bss->bssid);
	core->bss.channel = bss->channel;
	core->bss.ctx_idx = resp.peer_idx;
	core->bss.width = cc.bw;

	skw_dpd_set_coeff_params(wiphy, ndev, bss->channel->hw_value,
				 cc.center_chn1, cc.center_chn2, cc.bw);

	if (!iface->sta.sme_external) {
		if (!is_valid_ether_addr(iface->sta.conn->prev_bssid))
			core->bss.auth_type = iface->sta.conn->auth_type;
	}

	return 0;
}

static int skw_unjoin(struct wiphy *wiphy, struct net_device *ndev,
		      const u8 *bssid, u16 reason, bool tx_frame)
{
	int ret = 0;
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "bssid: %pM, reason: %d\n", bssid, reason);

	if (ndev->ieee80211_ptr->iftype == NL80211_IFTYPE_STATION) {
		skw_set_mib_bool(wiphy, iface->id, SKW_MIB_DOT11_MODE_HE, true);
		skw_set_mib_bool(wiphy, iface->id, SKW_MIB_DOT11_MODE_VHT, true);
	}

	ctx = skw_peer_ctx(iface, bssid);
	if (!ctx) {
		skw_chip_warn(skw->idx, "bssid: %pM not exist\n", bssid);
		return 0;
	}

	skw_peer_ctx_transmit(ctx, false);

	iface->sta.is_wep = false;
	SKW_SET(iface->flags, SKW_IFACE_FLAG_DEAUTH);

	ret = skw_cmd_unjoin(wiphy, ndev, bssid, reason, tx_frame);
	if (!ret) {
		memset(&iface->sta.core.bss, 0x0, sizeof(iface->sta.core.bss));
		iface->sta.core.bss.ctx_idx = SKW_INVALID_ID;

		skw_lmac_unbind_iface(wiphy_priv(wiphy), iface->lmac_id, iface->id);

		skw_peer_ctx_bind(iface, ctx, NULL);
	} else {
		skw_chip_warn(skw->idx, "command unjoin failed, ret: %d\n", ret);
		SKW_CLEAR(iface->flags, SKW_IFACE_FLAG_DEAUTH);
	}

	return ret;
}

int skw_sta_leave(struct wiphy *wiphy, struct net_device *dev,
		const u8 *bssid, u16 reason, bool tx_frame)
{
	int i;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "bssid: %pM, reason: %d\n", bssid, reason);

	skw_wdev_assert_lock(iface);

	netif_carrier_off(dev);

	if (iface->skw->hw.bus == SKW_BUS_PCIE &&
		iface->sta.core.sm.state >= SKW_STATE_ASSOCED)
		skw_edma_dec_refill((void *)iface->skw, iface->lmac_id);

	memset(&iface->wmm, 0x0, sizeof(iface->wmm));

	del_timer_sync(&iface->sta.core.timer);

	skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
	iface->sta.core.sm.flags = 0;

	skw_unjoin(wiphy, dev, bssid, reason, tx_frame);
	skw_purge_key_conf(&iface->key_conf);

	memset(iface->sta.core.bss.ssid, 0x0, IEEE80211_MAX_SSID_LEN);
	iface->sta.core.bss.ssid_len = 0;

	for (i = 0; i < SKW_MAX_DEFRAG_ENTRY; i++) {
		skb_queue_purge(&iface->frag[i].skb_list);
		iface->frag[i].tid = SKW_INVALID_ID;
	}

	return 0;
}

void skw_tx_mlme_mgmt(struct net_device *dev, u16 stype,
		      const u8 *bssid, const u8 *da, u16 reason)
{
	struct ieee80211_mgmt mgmt;
	struct skw_iface *iface = netdev_priv(dev);

	mgmt.duration = 0;
	mgmt.seq_ctrl = 0;
	memcpy(mgmt.da, da, ETH_ALEN);
	memcpy(mgmt.sa, iface->addr, ETH_ALEN);
	memcpy(mgmt.bssid, bssid, ETH_ALEN);
	mgmt.u.deauth.reason_code = cpu_to_le16(reason);
	mgmt.frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | stype);

	skw_cfg80211_tx_mlme_mgmt(dev, (void *)&mgmt, SKW_DEAUTH_FRAME_LEN);
}

static int skw_auth(struct wiphy *wiphy, struct net_device *ndev,
		    struct cfg80211_auth_request *req)
{
	int ret;
	struct key_params key;
	bool roaming = false;
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_bss_cfg *bss = &iface->sta.core.bss;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	const u8 *auth_data = req->auth_data;
#else
	const u8 *auth_data = req->sae_data;
#endif
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "%s, bssid: %pM, auth type: %d, state: %s\n",
		 netdev_name(ndev), req->bss->bssid,
		 req->auth_type, skw_state_name(iface->sta.core.sm.state));

	skw_wdev_assert_lock(iface);

#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
	ret = skw_switch_usb3_to_usb2_using_2G(iface, req->bss->channel->band);
	if (ret)
		return ret;
#endif

	skw_abort_scan(wiphy, ndev->ieee80211_ptr);

	// skw_scan_done(iface->skw, iface, true);
	// skw_sched_scan_stop(wiphy, ndev, iface->skw->sched_scan_req->reqid);

	switch (iface->sta.core.sm.state) {
	case SKW_STATE_AUTHING:
	case SKW_STATE_ASSOCING:
		return -EBUSY;

	case SKW_STATE_ASSOCED:
	case SKW_STATE_COMPLETED:
		if (ether_addr_equal(bss->bssid, req->bss->bssid))
			return 0;

		roaming = true;

		if (iface->sta.sme_external)
			skw_tx_mlme_mgmt(iface->ndev, IEEE80211_STYPE_DEAUTH,
				iface->sta.core.bss.bssid,
				iface->sta.core.bss.bssid, SKW_LEAVE);

		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);

		ret = skw_unjoin(wiphy, ndev, bss->bssid, SKW_LEAVE, false);
		if (ret)
			return ret;

		/* fall through */
		skw_fallthrough;
	case SKW_STATE_NONE:
		if (is_valid_ether_addr(bss->bssid)) {
			skw_chip_warn(skw->idx, "unexpected bssid: %pM\n", bss->bssid);
			ret = skw_unjoin(wiphy, ndev, bss->bssid, 3, false);
			if (ret)
				return ret;
		}

		if (!skw_channel_allowed(wiphy, req->bss->channel->hw_value))
			return -EBUSY;

		ret = skw_join(wiphy, ndev, req->bss, roaming);
		if (ret < 0)
			return ret;

		break;

	default:
		break;
	}

	if (req->key && req->key_len) {
		key.seq = NULL;
		key.seq_len = 0;
		key.key = (u8 *)req->key;
		key.key_len = req->key_len;
		key.cipher = SKW_CIPHER_SUITE_WEP40;

		if (req->key_len != 5)
			key.cipher = SKW_CIPHER_SUITE_WEP104;

		ret = skw_add_key(wiphy, ndev, 0, req->key_idx, false, NULL, &key);
		if (ret < 0) {
			skw_chip_err(skw->idx, "add share key failed, ret: %d\n", ret);
			goto unjoin;
		}

		iface->sta.is_wep = true;
		skw_set_default_key(wiphy, ndev, 0, req->key_idx, true, true);
	}

	iface->sta.core.auth_start = jiffies;
	iface->sta.core.pending.retry = 0;
	iface->sta.core.pending.redo = 0;
	iface->sta.core.pending.step_start = jiffies;
	iface->sta.core.pending.ctx_start = jiffies;
	iface->sta.core.pending.auth_type = req->auth_type;
	iface->sta.core.sm.rty_state = SKW_RETRY_NONE;
	iface->sta.core.pending.ctx_to = SKW_AUTH_TIMEOUT;
	skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHING);

	ret = skw_cmd_auth(wiphy, ndev, req);
	if (ret) {
		skw_chip_dbg(skw->idx, "command auth failed, ret: %d\n", ret);

		goto unjoin;
	}

	skw_set_sta_timer(&iface->sta.core, SKW_STEP_TIMEOUT);

	/* SAE confirm */
	if (auth_data && le16_to_cpu(*((u16 *)auth_data) == 2) &&
	    iface->sta.core.sm.flags & SKW_SM_FLAG_SAE_RX_CONFIRM)
		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHED);

	iface->sta.report_deauth = true;
	return 0;

unjoin:
	skw_unjoin(wiphy, ndev, req->bss->bssid, SKW_LEAVE, false);

	skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);

	return ret;
}

static int skw_cfg80211_auth(struct wiphy *wiphy, struct net_device *dev,
			     struct cfg80211_auth_request *req)
{
	struct skw_iface *iface = netdev_priv(dev);

	iface->sta.report_deauth = false;

	return skw_auth(wiphy, dev, req);
}

static int skw_assoc(struct wiphy *wiphy, struct net_device *dev,
		struct cfg80211_assoc_request *req)
{
	int ret;
	const u8 *ssid_ie;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_sta_core *core = &iface->sta.core;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, bssid: %pM\n", netdev_name(dev), req->bss->bssid);

	skw_wdev_assert_lock(iface);

	switch (core->sm.state) {
	case SKW_STATE_AUTHING:
	case SKW_STATE_ASSOCING:
		return -EBUSY;

	case SKW_STATE_ASSOCED:
	case SKW_STATE_COMPLETED:
		if (ether_addr_equal(core->bss.bssid, req->bss->bssid))
			return 0;

		skw_set_state(skw, &core->sm, SKW_STATE_NONE);

		ret = skw_unjoin(wiphy, dev, core->bss.bssid, SKW_LEAVE, false);
		if (ret)
			return ret;

		ret = skw_join(wiphy, dev, req->bss, true);
		if (ret)
			return ret;

		skw_set_state(skw, &core->sm, SKW_STATE_AUTHED);

		break;

		/* continue */
	case SKW_STATE_AUTHED:
		break;

	default:
		return -EINVAL;
	}

	rcu_read_lock();

	ssid_ie = ieee80211_bss_get_ie(req->bss, WLAN_EID_SSID);
	if (ssid_ie) {
		memcpy(core->bss.ssid, ssid_ie + 2, ssid_ie[1]);
		core->bss.ssid_len = ssid_ie[1];
	}

	rcu_read_unlock();

	core->cbss = req->bss;
	core->pending.retry = 0;
	core->pending.ctx_start = jiffies;
	iface->sta.core.pending.ctx_to = SKW_ASSOC_TIMEOUT;
	core->assoc_req_ie_len = 0;
	memset(core->assoc_req_ie, 0x0, SKW_2K_SIZE);

	skw_set_state(skw, &core->sm, SKW_STATE_ASSOCING);

	ret = skw_cmd_assoc(wiphy, dev, req);
	if (!ret) {
		skw_set_sta_timer(core, SKW_STEP_TIMEOUT);
	} else {
		skw_chip_err(skw->idx, "command assoc failed, ret: %d\n", ret);

		core->cbss = NULL;

		del_timer_sync(&core->timer);

		skw_unjoin(wiphy, dev, req->bss->bssid, SKW_LEAVE, false);
		skw_set_state(skw, &core->sm, SKW_STATE_NONE);

		memset(core->bss.ssid, 0x0, IEEE80211_MAX_SSID_LEN);
		core->bss.ssid_len = 0;
	}

	return ret;
}

static int skw_cfg80211_assoc(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_assoc_request *req)
{
	return skw_assoc(wiphy, dev, req);
}

static int skw_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_deauth_request *req)
{
	int ret;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);
	bool tx_frame = !req->local_state_change && iface->sta.report_deauth;

	skw_chip_info(skw->idx, "%s: bssid: %pM, reason: %d, tx frame: %d\n",
		 netdev_name(dev), req->bssid, req->reason_code, tx_frame);

	ret = skw_sta_leave(wiphy, dev, req->bssid, req->reason_code, tx_frame);
	if (!ret && iface->sta.report_deauth) {
		skw_tx_mlme_mgmt(dev, IEEE80211_STYPE_DEAUTH,
				 req->bssid, req->bssid,
				 req->reason_code);
	} else {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
	}

	return ret;
}

static int skw_disassoc(struct wiphy *wiphy, struct net_device *dev,
			const u8 *bssid, u16 reason, bool tx_frame)
{
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "%s, bssid: %pM, reason: %d, tx frame: %d\n",
		 netdev_name(dev), bssid, reason, tx_frame);

	ret = skw_sta_leave(wiphy, dev, bssid, reason, tx_frame);
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
		return ret;
	}

	skw_tx_mlme_mgmt(dev, IEEE80211_STYPE_DISASSOC, bssid, bssid, reason);

	return 0;
}

void skw_connected(struct net_device *dev, struct skw_connect_param *conn,
		   const u8 *req_ie, int req_ie_len, const u8 *resp_ie,
		   int resp_ie_len, u16 status, gfp_t gfp)
{
	if (conn->flags & SKW_CONN_FLAG_ASSOCED) {
		skw_compat_cfg80211_roamed(dev, conn->bssid, req_ie,
				req_ie_len, resp_ie, resp_ie_len, gfp);
	} else {
		cfg80211_connect_result(dev, conn->bssid, req_ie, req_ie_len,
				resp_ie, resp_ie_len, status, gfp);
	}

	SKW_SET(conn->flags, SKW_CONN_FLAG_ASSOCED);
}

void skw_disconnected(struct net_device *dev, u16 reason, const u8 *resp_ie,
	int resp_ie_len, bool local_gen, gfp_t gfp)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_connect_param *conn = iface->sta.conn;

	mutex_lock(&conn->lock);
	if (conn->flags & SKW_CONN_FLAG_ASSOCED) {
		skw_compat_disconnected(dev, reason, NULL, 0, local_gen, gfp);
	} else {
		cfg80211_connect_result(dev, conn->bssid, NULL, 0,
			resp_ie, resp_ie_len, reason, gfp);
	}

	SKW_CLEAR(iface->sta.conn->flags, SKW_CONN_FLAG_ASSOCED);
	mutex_unlock(&conn->lock);
}

int skw_connect_sae_auth(struct wiphy *wiphy, struct net_device *dev,
			 struct cfg80211_bss *bss)
{
	int ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
	bool roaming = false;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_connect_param *conn = iface->sta.conn;
	struct cfg80211_external_auth_params params;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!bss) {
		cfg80211_connect_result(dev, conn->bssid, NULL, 0, NULL, 0,
				WLAN_STATUS_UNSPECIFIED_FAILURE,
				GFP_KERNEL);

		return -EINVAL;
	}

	// TODO:
	// unjoin prev bssid for roaming connection

	roaming = is_valid_ether_addr(conn->prev_bssid);
	ret = skw_join(wiphy, dev, bss, roaming);
	if (ret < 0) {
		skw_chip_err(skw->idx, "join %pM failed\n", conn->bssid);
		return ret;
	}

	skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHING);

	params.action = NL80211_EXTERNAL_AUTH_START;
	memcpy(params.bssid, conn->bssid, ETH_ALEN);

	params.ssid.ssid_len = conn->ssid_len;
	memcpy(params.ssid.ssid, conn->ssid, conn->ssid_len);

	params.key_mgmt_suite = cpu_to_be32(WLAN_AKM_SUITE_SAE);
	params.status = WLAN_STATUS_SUCCESS;

	ret = cfg80211_external_auth_request(dev, &params, GFP_KERNEL);
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

		skw_unjoin(wiphy, dev, conn->bssid, SKW_LEAVE, false);
		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);

		cfg80211_connect_result(dev, conn->bssid, NULL, 0, NULL, 0,
				WLAN_STATUS_UNSPECIFIED_FAILURE,
				GFP_KERNEL);
	}
#endif

	return ret;
}

int skw_connect_auth(struct wiphy *wiphy, struct net_device *dev,
		struct skw_connect_param *conn, struct cfg80211_bss *bss)
{
	struct cfg80211_auth_request req;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!bss) {
		skw_chip_warn(skw->idx, "Invalid bss\n");
		return -EINVAL;
	}

	memset(&req, 0x0, sizeof(req));

	req.bss = bss;
	req.key = conn->key_len ? conn->key : NULL;
	req.key_len = conn->key_len;
	req.key_idx = conn->key_idx;
	req.auth_type = conn->auth_type;

	return skw_auth(wiphy, dev, &req);
}

int skw_connect_assoc(struct wiphy *wiphy, struct net_device *ndev,
		struct skw_connect_param *conn)
{
	int ret = 0;
	struct cfg80211_assoc_request req = {};
	struct skw_core *skw = wiphy_priv(wiphy);

	req.bss = cfg80211_get_bss(wiphy, conn->channel, conn->bssid,
				   conn->ssid, conn->ssid_len,
				   SKW_BSS_TYPE_ESS, SKW_PRIVACY_ESS_ANY);
	if (!req.bss) {
		skw_chip_info(skw->idx, "cfg80211_get_bss null\n");
		return -ENOENT;
	}

	req.ie = conn->assoc_ie;
	req.ie_len = conn->assoc_ie_len;
	req.prev_bssid = conn->prev_bssid;
	req.use_mfp = conn->flags & SKW_CONN_FLAG_USE_MFP;
	req.flags = conn->flags;
	req.ht_capa = conn->ht_capa;
	req.ht_capa_mask = conn->ht_capa_mask;
	req.vht_capa = conn->vht_capa;
	req.vht_capa_mask = conn->vht_capa_mask;

	ret = skw_assoc(wiphy, ndev, &req);

	cfg80211_put_bss(wiphy, req.bss);

	return ret;
}

int skw_roam_connect(struct skw_iface *iface, const u8 *bssid, u8 chn,
		     enum nl80211_band band)
{
	struct ieee80211_channel *req_channel = NULL;
	struct wiphy *wiphy = iface->wdev.wiphy;
	struct skw_connect_param *conn = iface->sta.conn;
	u32 freq = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!is_valid_ether_addr(bssid))
		return -EINVAL;

	skw_chip_dbg(skw->idx, "roam from %pM to %pM auth_type: %d, chn: %d, band: %d\n",
		conn->bssid, bssid, conn->auth_type, chn, band);
	freq = ieee80211_channel_to_frequency(chn, band);
	req_channel = ieee80211_get_channel(wiphy, freq);

	if (!req_channel) {
		skw_chip_err(skw->idx, "invalid channel: %d\n", chn);
		return -EINVAL;
	}
#if 0
	iface->sta.backup = SKW_KMEMDUP(&iface->sta.core.bss,
					sizeof(iface->sta.core.bss),
					GFP_KERNEL);
	if (!iface->sta.backup)
		return -EINVAL;

	// skw_peer_transmit();
	memset(&iface->sta.core.bss, 0x0, sizeof(iface->sta.core.bss));
#endif
	conn->channel = req_channel;
	skw_ether_copy(conn->bssid, bssid);
	skw_ether_copy(conn->prev_bssid, iface->sta.core.bss.bssid);

	conn->auth_type = iface->sta.conn->auth_type;

	skw_queue_local_event(priv_to_wiphy(iface->skw), iface,
			      SKW_EVENT_LOCAL_STA_CONNECT, NULL, 0);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
static int skw_set_cqm_rssi_config(struct wiphy *wiphy, struct net_device *dev,
				s32 rssi_thold, u32 rssi_hyst)
{
	struct skw_set_cqm_rssi_param cqm_param;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, thold: %d, hyst: %d\n",
		netdev_name(dev), rssi_thold, rssi_hyst);

	//TBD: whether to store the config at host driver

	cqm_param.rssi_thold = rssi_thold;
	cqm_param.rssi_hyst = (u8)rssi_hyst;

	return skw_send_msg(wiphy, dev, SKW_CMD_SET_CQM_RSSI, &cqm_param,
			    sizeof(cqm_param), NULL, 0);
}

static int skw_set_cqm_rssi_range_config(struct wiphy *wiphy,
							struct net_device *dev,
							s32 rssi_low, s32 rssi_high)
{
	struct skw_set_cqm_rssi_param cqm_param = {0};
	s32 val = rssi_high - rssi_low;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, rssi_low: %d, rssi_high: %d\n",
		netdev_name(dev), rssi_low, rssi_high);

	cqm_param.rssi_thold = rssi_low;
	if (val < 0) {
		skw_chip_warn(skw->idx, "rssi err\n");
		return -EINVAL;
	} else {
		cqm_param.rssi_hyst = (u8)val;
	}

	return skw_send_msg(wiphy, dev, SKW_CMD_SET_CQM_RSSI, &cqm_param,
				sizeof(cqm_param), NULL, 0);
}
#endif

static int skw_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev,
			struct cfg80211_connect_params *req)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_connect_param *conn = iface->sta.conn;
	const u8 *bssid = skw_compat_bssid(req);
	struct ieee80211_channel *channel = skw_compat_channel(req);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, ssid: %s, bssid: %pM, auth: %d, chn: %d key_len: %d\n",
		netdev_name(ndev), req->ssid, bssid, req->auth_type,
		channel->hw_value, req->key_len);

	if (!conn) {
		skw_chip_dbg(skw->idx, "conn is NULL\n");
		return -ENOMEM;
	}

	if (unlikely(req->ssid_len > IEEE80211_MAX_SSID_LEN)) {
		skw_chip_err(skw->idx, "Invalid SSID: %s, len: %zd\n",
			req->ssid, req->ssid_len);

		return -EINVAL;
	}

	mutex_lock(&conn->lock);

	skw_ether_copy(conn->bssid, bssid);
	eth_zero_addr(conn->prev_bssid);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	if (req->prev_bssid)
		skw_ether_copy(conn->prev_bssid, req->prev_bssid);
#endif

	if (req->ie && req->ie_len)
		memcpy(conn->assoc_ie, req->ie, req->ie_len);

	conn->assoc_ie_len = req->ie_len;

	if (req->auth_type == NL80211_AUTHTYPE_AUTOMATIC) {
		conn->auth_type = NL80211_AUTHTYPE_OPEN_SYSTEM;
		SKW_SET(conn->flags, SKW_CONN_FLAG_AUTH_AUTO);
	}

	if (req->key && req->key_len) {
		memcpy(conn->key, req->key, req->key_len);
		conn->key_len = req->key_len;
		conn->key_idx = req->key_idx;
		SKW_SET(conn->flags, SKW_CONN_FLAG_KEY_VALID);
	} else
		conn->key_len = 0;

	conn->ssid_len = req->ssid_len;
	memcpy(conn->ssid, req->ssid, req->ssid_len);

	conn->auth_type = req->auth_type;
	conn->ht_capa = req->ht_capa;
	conn->vht_capa = req->vht_capa;

	conn->ht_capa_mask = req->ht_capa_mask;
	conn->vht_capa_mask = req->vht_capa_mask;

	mutex_unlock(&conn->lock);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	if (iface->wdev.iftype == NL80211_IFTYPE_STATION) {
		skw_set_cqm_rssi_config(wiphy, ndev, SKW_CQM_DEFAUT_RSSI_THOLD,
					SKW_CQM_DEFAUT_RSSI_HYST);
	}
#endif

	return skw_queue_local_event(wiphy, iface,
			SKW_EVENT_LOCAL_STA_CONNECT, NULL, 0);
}


static int skw_cfg80211_disconnect(struct wiphy *wiphy,
			struct net_device *dev, u16 reason)
{
	int ret;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_sta_core *core = &iface->sta.core;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "%s, reason: %d\n", netdev_name(dev), reason);

	ret = skw_sta_leave(wiphy, dev, core->bss.bssid, reason, true);
	if (!ret)
		skw_disconnected(dev, reason, NULL, 0, true, GFP_KERNEL);

	return ret;
}

static u64 skw_tx_cookie(void)
{
	static u64 skw_cookie;

	if (WARN_ON(++skw_cookie == 0))
		skw_cookie++;

	return skw_cookie;
}

static int skw_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
				 struct ieee80211_channel *chan,
				 unsigned int duration, u64 *cookie)
{
	int ret;
	struct skw_roc_param roc;
	u64 tx_cookie = skw_tx_cookie();
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "iface: %u, chan: %u, band: %u duration: %d, cookie: %llu\n",
		iface->id, chan->hw_value, chan->band, duration, tx_cookie);

	roc.enable = 1;
	roc.channel_num = chan->hw_value;
	roc.band = to_skw_band(chan->band);
	roc.duration = duration;
	roc.cookie = *cookie = tx_cookie;
	//TBD: define the referenced value
	if (chan->flags & IEEE80211_CHAN_NO_HT40MINUS)
		roc.channel_type = 2;
	else if (chan->flags & IEEE80211_CHAN_NO_HT40PLUS)
		roc.channel_type = 1;
	else if (chan->flags & SKW_IEEE80211_CHAN_NO_20MHZ)
		roc.channel_type = 0;
	else
		roc.channel_type = 3;

	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_REMAIN_ON_CHANNEL,
			   &roc, sizeof(roc), NULL, 0);

	return ret;
}

static int skw_cancel_roc(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	struct skw_roc_param param;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "cookie: %lld\n", cookie);

#if 0
	// fixme:
	if (cookie != skw->remain_on_channel_cookie)
		return -ENOENT;
#endif

	memset(&param, 0x0, sizeof(param));

	return skw_msg_xmit(wiphy, iface->id, SKW_CMD_REMAIN_ON_CHANNEL,
			    &param, sizeof(param), NULL, 0);
}
static inline void __skw_set_peer_flags(struct skw_peer_ctx *ctx, u32 flags)
{
	if (ctx) {
		skw_peer_ctx_lock(ctx);

		if (ctx->peer)
			ctx->peer->flags |= flags;

		skw_peer_ctx_unlock(ctx);
	}
}

static void skw_set_peer_flags(struct skw_iface *iface,
			const u8 *addr, u32 flags)
{
	int idx;
	struct skw_peer_ctx *ctx;
	u32 peer_map = atomic_read(&iface->peer_map);

	if (!addr)
		return;

	if (is_unicast_ether_addr(addr)) {
		ctx = skw_peer_ctx(iface, addr);
		__skw_set_peer_flags(ctx, flags);
		return;
	}

	while (peer_map) {
		idx = ffs(peer_map) - 1;
		SKW_CLEAR(peer_map, BIT(idx));

		ctx = &iface->skw->hw.lmac[iface->lmac_id].peer_ctx[idx];
		__skw_set_peer_flags(ctx, flags);
	}
}

int skw_mgmt_tx(struct wiphy *wiphy, struct skw_iface *iface,
		struct ieee80211_channel *chan, u32 wait, u64 *cookie,
		bool dont_wait_ack, const void *frame, int frame_len,
		int total_frame_len, const struct ieee80211_mgmt *mgmt, bool switchover)
{
	int ret, total_len;
	struct skw_mgmt_tx_param *param = NULL;
	u64 tx_cookie = skw_tx_cookie();
	u16 fc = SKW_MGMT_SFC(mgmt->frame_control);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!chan || !iface)
		return -EINVAL;

	if (frame_len > total_frame_len)
		return -EFBIG;

	skw_chip_dbg(skw->idx, "%s: chan: %d, wait: %d, cookie: 0x%llx, no_ack: %d, len: %d total: %d\n",
		skw_mgmt_name(fc), chan->hw_value, wait, tx_cookie,
		dont_wait_ack, frame_len, total_frame_len);

	skw_hex_dump("mgmt tx", frame, frame_len, false);

	total_len = sizeof(*param) + frame_len;
	param = SKW_ZALLOC(total_len, GFP_KERNEL);
	if (!param) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", total_len);
		return -ENOMEM;
	}

	param->wait = wait;
	param->channel = chan->hw_value;
	param->band = to_skw_band(chan->band);
	param->dont_wait_for_ack = dont_wait_ack;
	param->cookie = *cookie = tx_cookie;

	memcpy(param->mgmt, frame, frame_len);
	param->mgmt_frame_len = total_frame_len;

	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_TX_MGMT,
			   param, total_len, NULL, 0);
	if (!ret) {
		if (switchover)
			if (is_unicast_ether_addr(mgmt->da) &&
				(fc == IEEE80211_STYPE_DEAUTH ||
				fc == IEEE80211_STYPE_DISASSOC)) {
				skw_set_peer_flags(iface, mgmt->da,
						SKW_PEER_FLAG_DEAUTHED);
			}
	} else {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
	}

	SKW_KFREE(param);

	return ret;
}

static inline bool is_skw_rrm_report(const void *buf, int buf_len)
{
	const struct ieee80211_mgmt *mgmt = buf;

	if (!ieee80211_is_action(mgmt->frame_control))
		return false;

	if (buf_len < IEEE80211_MIN_ACTION_SIZE +
		      sizeof(mgmt->u.action.u.measurement))
		return false;

	if (mgmt->u.action.category != SKW_WLAN_CATEGORY_RADIO_MEASUREMENT)
		return false;

	if (mgmt->u.action.u.measurement.action_code != WLAN_ACTION_SPCT_MSR_RPRT)
		return false;

	return true;
}

static int __skw_cfg80211_mgmt_tx(struct wiphy *wiphy, struct skw_iface *iface,
				  struct ieee80211_channel *chan, u32 wait,
				  u64 *cookie, bool dont_wait_for_ack,
				  const void *frame, int frame_len)
{
	struct ieee80211_channel *tx_chan = chan;
	struct skw_core *skw = wiphy_priv(wiphy);
	int ret = 0;

#define SKW_MGMT_TX_LEN 1500

	if (!tx_chan) {
		if (is_skw_sta_mode(iface))
			tx_chan = iface->sta.core.bss.channel;
		else
			tx_chan = iface->sap.cfg.channel;
	}


	skw_hex_dump("frame", frame, frame_len, false);

	down(&skw->cmd.mgmt_cmd_lock);

	if (!skw_cmd_data_len_in_limit(skw, frame_len + sizeof(struct skw_mgmt_tx_param))) {
		if (is_skw_rrm_report(frame, frame_len)) {
			int head_offset = offsetof(struct ieee80211_mgmt,
					u.action.u.measurement.element_id);

			int ret = -E2BIG;
			int elem_len = 0, next_len = 0;
			int left = frame_len - head_offset;
			char *pos = (u8 *)frame + head_offset, *next = pos;
			char *data = NULL;

			data = SKW_ZALLOC(SKW_MGMT_TX_LEN, GFP_KERNEL);
			if (!data) {
				skw_chip_err(skw->idx, "alloc %d failed\n", SKW_MGMT_TX_LEN);
				ret = -ENOMEM;
				goto unlock;
			}

			while (left) {
				int tx_len;

				next_len = next[1] + 2;
				tx_len = elem_len + head_offset + next_len;
				if (tx_len < SKW_MGMT_TX_LEN) {
					elem_len += next_len;
					left -= next_len;

					if (left) {
						next += next_len;
						continue;
					}
				}

				memcpy(data, frame, head_offset);
				memcpy(data + head_offset, pos, elem_len);

				skw_hex_dump("rrm", data, elem_len + head_offset, false);

				ret = skw_mgmt_tx(wiphy, iface, tx_chan, wait,
						cookie, dont_wait_for_ack, data,
						elem_len + head_offset, elem_len + head_offset, frame, true);

				pos = next;
				elem_len = 0;
			}

			SKW_KFREE(data);
			goto unlock;

		} else {
			int send_len;
			int remain = frame_len;
			const u8 *data = frame;
			int buf_size = SKW_MGMT_TX_LEN;

			while (remain > 0) {
				send_len = (remain < buf_size) ? remain : buf_size;
				remain -= send_len;

				skw_chip_dbg(skw->idx, "mgmt part : remain: %d, send_len: %d \n", remain, send_len);
				skw_hex_dump("part", data, send_len, false);

				ret = skw_mgmt_tx(wiphy, iface, tx_chan, wait,
						cookie, true, data, send_len,
						frame_len, frame, true);

				if (ret) {
					skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
					goto unlock;
				}

				data += send_len;
			}

			if (dont_wait_for_ack == false) {
				struct skw_mgmt_status status;
				status.mgmt_status_data = SKW_ZALLOC(frame_len, GFP_KERNEL);

				if (!status.mgmt_status_data) {
					skw_chip_err(skw->idx, "malloc mgmt_status_data failed, size: %d\n", frame_len);
					ret = -ENOMEM;
				} else {
					status.mgmt_status_data_len = frame_len;
					memcpy(status.mgmt_status_data, frame, frame_len);
					status.mgmt_status_cookie = *cookie;

					skw_chip_dbg(skw->idx, "split part coockie:0x%llx\n", status.mgmt_status_cookie);
					skw_queue_work(wiphy, iface,
						SKW_WORK_SPLIT_MGMT_TX_STATUS, &status, sizeof(status));
				}
			}

			goto unlock;
		}
	}

#undef SKW_MGMT_TX_LEN
	skw_dump_frame((u8 *)frame, (u16)frame_len);
	ret = skw_mgmt_tx(wiphy, iface, tx_chan, wait, cookie,
			dont_wait_for_ack, frame, frame_len, frame_len, frame, true);

unlock:
	up(&skw->cmd.mgmt_cmd_lock);
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static int skw_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
				struct cfg80211_mgmt_tx_params *params,
				u64 *cookie)
{
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);

	return __skw_cfg80211_mgmt_tx(wiphy, iface, params->chan,
				      params->wait, cookie,
				      params->dont_wait_for_ack,
				      params->buf, params->len);
}
#else
static int skw_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
			  struct ieee80211_channel *chan, bool offchan,
			  unsigned int wait, const u8 *buf, size_t len,
			  bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);

	return __skw_cfg80211_mgmt_tx(wiphy, iface, chan, wait, cookie,
			dont_wait_for_ack, buf, len);
}
#endif

static int skw_join_ibss(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_ibss_params *params)
{
	int i;
	u8 *pos;
	struct cfg80211_bss *bss;
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_supported_band *sband;
	struct skw_iface *iface = netdev_priv(dev);
	struct cfg80211_chan_def *chandef = &params->chandef;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s, bssid: %pM, ssid: %s, channel: %d, band: %u, chan_fixed: %d\n",
		netdev_name(dev), params->bssid, params->ssid,
		chandef->chan->hw_value, chandef->chan->band, params->channel_fixed);

	if (params->bssid)
		memcpy(iface->ibss.bssid, params->bssid, ETH_ALEN);
	else
		eth_random_addr(iface->ibss.bssid);

	iface->ibss.bw = to_skw_bw(params->chandef.width);
	if (iface->ibss.bw == SKW_CHAN_WIDTH_MAX)
		return -EINVAL;

	iface->ibss.beacon_int = params->beacon_interval;
	iface->ibss.channel = chandef->chan->hw_value;
	iface->ibss.band = to_skw_band(chandef->chan->band);
	iface->ibss.center_freq1 = chandef->center_freq1;
	iface->ibss.center_freq2 = chandef->center_freq2;
	iface->ibss.chandef = params->chandef;

	// start build presp frame
	mgmt = SKW_ZALLOC(SKW_2K_SIZE, GFP_KERNEL);
	if (!mgmt) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", SKW_2K_SIZE);
		return -ENOMEM;
	}

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					IEEE80211_STYPE_PROBE_RESP);

	eth_broadcast_addr(mgmt->da);
	memcpy(mgmt->sa, iface->addr, ETH_ALEN);
	memcpy(mgmt->bssid, iface->ibss.bssid, ETH_ALEN);

	mgmt->u.beacon.beacon_int = cpu_to_le16(params->beacon_interval);
	// mgmt->u.beacon.timestamp = cpu_to_le64(0);
	mgmt->u.beacon.capab_info = cpu_to_le16(WLAN_CAPABILITY_IBSS);

	pos = mgmt->u.beacon.variable;

	*pos++ = WLAN_EID_SSID;
	*pos++ = params->ssid_len;
	memcpy(pos, params->ssid, params->ssid_len);
	pos += params->ssid_len;

	*pos++ = WLAN_EID_SUPP_RATES;
	*pos++ = 8;
	sband = wiphy->bands[chandef->chan->band];

	for (i = 0; i < sband->n_bitrates; i++) {
		int rate = DIV_ROUND_UP(sband->bitrates[i].bitrate, 5);
		*pos++ = rate | 0x80;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
	if (sband->band == IEEE80211_BAND_2GHZ) {
#else
	if (sband->band == NL80211_BAND_2GHZ) {
#endif
		*pos++ = WLAN_EID_DS_PARAMS;
		*pos++ = 1;
		*pos++ = chandef->chan->hw_value;
	}

	*pos++ = WLAN_EID_IBSS_PARAMS;
	*pos++ = 2;
	*pos++ = 0;
	*pos++ = 0;
#if 0
	*pos++ = WLAN_EID_EXT_SUPP_RATES;
	*pos++ = 0;
#endif
	if (params->ie) {
		memcpy(pos, params->ie, params->ie_len);
		pos += params->ie_len;
	}
	// end build frame

//	skw_set_template_frame();
	bss = cfg80211_get_bss(wiphy, chandef->chan, params->bssid,
				params->ssid, params->ssid_len,
				SKW_BSS_TYPE_IBSS,
				SKW_PRIVACY_IBSS_ANY);
	if (!bss) {
		skw_chip_info(skw->idx, "creating new ibss: %pM\n", iface->ibss.bssid);

		bss = cfg80211_inform_bss_frame(wiphy, chandef->chan,
				mgmt, pos - (u8 *)mgmt, DBM_TO_MBM(-30), GFP_KERNEL);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
	// fixme:
	if (params->wep_keys) {
		skw_add_key(wiphy, dev, 0, params->wep_tx_key, true,
			    iface->ibss.bssid, params->wep_keys);

		skw_set_default_key(wiphy, dev, 0, params->wep_tx_key, true, true);
	}
#endif

	cfg80211_put_bss(wiphy, bss);

	skw_queue_local_event(wiphy, iface, SKW_EVENT_LOCAL_IBSS_CONNECT,
				NULL, 0);

	SKW_KFREE(mgmt);
	return 0;
}

static int skw_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	struct skw_disconnect_param params = {0};
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s\n", netdev_name(dev));

	iface->ibss.joined = false;
	iface->ibss.ssid_len = 0;

	params.type = SKW_DISCONNECT_ONLY;
	params.reason_code = 0;

	return skw_send_msg(wiphy, dev, SKW_CMD_DISCONNECT,
			&params, sizeof(params), NULL, 0);
}

static int skw_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	int ret = 0;
	struct skw_tlv_conf conf;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "changed: 0x%x\n", changed);

	if (skw_tlv_alloc(&conf, 128, GFP_KERNEL))
		return -ENOMEM;

	skw_tlv_start(&conf);

	if (changed & WIPHY_PARAM_RETRY_SHORT) {
		if (skw_tlv_add(&conf, SKW_MIB_RETRY_SHORT, &wiphy->retry_short,
				sizeof(wiphy->retry_short))) {

			ret = -ENOMEM;
			skw_chip_err(skw->idx, "add SKW_MIB_RETRY_SHORT failed.\n");

			goto free;
		}
	}

	if (changed & WIPHY_PARAM_RETRY_LONG) {
		if (skw_tlv_add(&conf, SKW_MIB_RETRY_LONG, &wiphy->retry_long,
				sizeof(wiphy->retry_long))) {

			ret = -ENOMEM;
			skw_chip_err(skw->idx, "add SKW_MIB_RETRY_LONG failed.\n");

			goto free;
		}
	}


	if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		if (skw_tlv_add(&conf, SKW_MIB_FRAG_THRESHOLD, &wiphy->frag_threshold,
				sizeof(wiphy->frag_threshold))) {
			ret = -ENOMEM;
			skw_chip_err(skw->idx, "add SKW_MIB_FRAG_THRESHOLD failed.\n");

			goto free;
		}
	}

	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		if (skw_tlv_add(&conf, SKW_MIB_RTS_THRESHOLD,
				  &wiphy->rts_threshold,
				  sizeof(wiphy->rts_threshold))) {
			ret = -ENOMEM;
			skw_chip_err(skw->idx, "add SKW_MIB_RTS_THRESHOLD failed.\n");

			goto free;
		}
	}

	skw_tlv_end(&conf);

	if (conf.total_len)
		ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SET_MIB, conf.buff,
				conf.total_len, NULL, 0);

free:
	skw_tlv_free(&conf);

	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	return ret;
}

static int skw_cfg80211_sched_scan_start(struct wiphy *wiphy,
		struct net_device *dev, struct cfg80211_sched_scan_request *req)
{
	int i, ret;
	struct skw_scan_chan_info *chan = NULL;

	u32 delay = 0;
	u64 reqid = 0;
	s8 relative_rssi = 0;
	bool relative_rssi_set = false;
	s32 min_rssi_thold = 0;
	int n_scan_plans = 0, n_plans_len = 0;
	int n_ssids_len, n_match_len;
	int size, fixed, offset = 0;
	u16 scan_chn_num = 0;


	struct skw_sched_match_sets *match_sets;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_sched_scan_param *params;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	reqid = req->reqid;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	relative_rssi_set = req->relative_rssi_set;
	relative_rssi = req->relative_rssi;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	n_scan_plans = req->n_scan_plans;
	n_plans_len = n_scan_plans * sizeof(*req->scan_plans);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	delay = req->delay;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
	min_rssi_thold = req->rssi_thold;
#else
	min_rssi_thold = req->min_rssi_thold;
#endif

	skw_chip_dbg(skw->idx, "%s, n_ssids: %d, n_channels: %d, n_match: %d, n_plans: %d, ie_len: %zd\n",
		netdev_name(dev), req->n_ssids, req->n_channels,
		req->n_match_sets, n_scan_plans, req->ie_len);

	fixed = sizeof(struct skw_sched_scan_param);
	n_ssids_len = req->n_ssids * sizeof(struct cfg80211_ssid);
	n_match_len = req->n_match_sets * sizeof(struct skw_sched_match_sets);

	size = fixed + req->ie_len + n_ssids_len + n_plans_len + n_match_len +
	       req->n_channels * sizeof(*chan);

	params = SKW_ZALLOC(size, GFP_KERNEL);
	if (!params) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", size);

		return -ENOMEM;
	}

	params->req_id = reqid;
	params->flags = req->flags;
	params->delay = delay;
	params->min_rssi_thold = min_rssi_thold;
	params->relative_rssi_set = relative_rssi_set;
	params->relative_rssi = relative_rssi;
	params->scan_width = NL80211_BSS_CHAN_WIDTH_20;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	skw_ether_copy(params->mac_addr, req->mac_addr);
	skw_ether_copy(params->mac_addr_mask, req->mac_addr_mask);
#endif

	params->n_ssids = req->n_ssids;
	if (req->n_ssids) {
		params->n_ssid_offset = fixed + offset;
		params->n_ssids_len = n_ssids_len;
		memcpy(params->data + offset, req->ssids, n_ssids_len);

		offset += n_ssids_len;
	}

	match_sets = (void *)params->data + offset;
	for (i = 0; i < req->n_match_sets; i++) {
		memcpy(match_sets[i].ssid, req->match_sets[i].ssid.ssid, 32);
		match_sets[i].ssid_len = req->match_sets[i].ssid.ssid_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
		match_sets[i].rssi_thold = req->match_sets[i].rssi_thold;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
		skw_ether_copy(match_sets[i].bssid, req->match_sets[i].bssid);
#endif
	}

	params->n_match_sets = req->n_match_sets;
	params->match_sets_offset = fixed + offset;
	params->match_sets_len = n_match_len;
	offset += n_match_len;

	params->n_scan_plans = n_scan_plans;
	if (n_scan_plans) {
		params->scan_plans_offset = fixed + offset;
		params->scan_plans_len = n_plans_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
		memcpy(params->data + offset, req->scan_plans, n_plans_len);
#endif

		offset += n_plans_len;
	}

	if (req->ie_len) {
		params->ie_offset = fixed + offset;
		params->ie_len = req->ie_len;
		memcpy(params->data + offset, req->ie, req->ie_len);
		offset += req->ie_len;
	}

	chan = (struct skw_scan_chan_info *)(params->data + offset);
	for (i = 0; i < req->n_channels; i++) {
		if (is_skw_6ghz_non_psc_chan(req->channels[i]))
			continue;

		chan->band = to_skw_band(req->channels[i]->band);
		chan->chan_num = req->channels[i]->hw_value;
		/* BIT[15]: set 1 means to run a passive scan on this channel */
		if (req->channels[i]->flags & SKW_PASSIVE_SCAN)
			chan->scan_flags |= SKW_SCAN_FLAG_PASSIVE;

		chan++;
		scan_chn_num++;
	}

	params->n_channels = scan_chn_num;
	params->channels_len = scan_chn_num * sizeof(struct skw_scan_chan_info);
	params->channels_offset = fixed + offset;

	skw->sched_scan_req = req;
	ret = skw_send_msg(wiphy, dev, SKW_CMD_START_SCHED_SCAN,
			   params, size, NULL, 0);
	if (ret) {
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);
		skw->sched_scan_req = NULL;
	}

	SKW_KFREE(params);

	return ret;
}

static int skw_sched_scan_stop(struct wiphy *wiphy,
			struct net_device *dev, u64 reqid)
{
	u64 scan_id = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, id: %lld, actived: %d\n",
		netdev_name(dev), scan_id, !!skw->sched_scan_req);

	if (!skw->sched_scan_req)
		return 0;

	skw->sched_scan_req = NULL;
	return skw_send_msg(wiphy, dev, SKW_CMD_STOP_SCHED_SCAN,
			&scan_id, sizeof(scan_id), NULL, 0);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
static int skw_cfg80211_sched_scan_stop(struct wiphy *wiphy,
			struct net_device *dev, u64 reqid)
{
	return skw_sched_scan_stop(wiphy, dev, reqid);
}
#else
static int skw_cfg80211_sched_scan_stop(struct wiphy *wiphy,
			struct net_device *dev)
{
	return skw_sched_scan_stop(wiphy, dev, 0);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
static void skw_mgmt_frame_register(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    struct mgmt_frame_regs *upd)
{
	u64 ts;
	int ret = 0, idx;
	struct skw_mgmt_register_param param;
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	u16 new_mask = upd->interface_stypes;
	u16 temp_mask = new_mask ^ iface->mgmt_frame_bitmap;
	u16 frame_mtype;
	bool reg;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!temp_mask)
		return;

	while (temp_mask) {
		idx = ffs(temp_mask) - 1;
		SKW_CLEAR(temp_mask, BIT(idx));
		frame_mtype = idx << 4;
		reg = new_mask & BIT(idx);

		skw_chip_dbg(skw->idx, "%s %s filter %s\n", skw_iftype_name(wdev->iftype),
			reg ? "add" : "del", skw_mgmt_name(frame_mtype));

		param.frame_type = frame_mtype;
		param.reg = reg;
		ts = skw_local_clock();
		do_div(ts, 1000000);

		param.timestamp = ts;
		ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_REGISTER_FRAME,
				&param, sizeof(param), NULL, 0);
		if (ret) {
			skw_chip_err(skw->idx, "%s %s failed, ret: %d\n",
				reg ? "add" : "del",
				skw_mgmt_name(frame_mtype), ret);
		}
	}

	iface->mgmt_frame_bitmap = new_mask;
}
#else
static void skw_mgmt_frame_register(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    u16 frame_type, bool reg)
{
	u64 ts = 0;
	int ret = 0;
	struct skw_mgmt_register_param param = {0};
	int type = (frame_type >> 4) & 0xf;
	struct skw_iface *iface = SKW_WDEV_TO_IFACE(wdev);
	u16 bitmap = iface->mgmt_frame_bitmap;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (reg)
		iface->mgmt_frame_bitmap |= BIT(type);
	else
		iface->mgmt_frame_bitmap &= ~BIT(type);

	if (bitmap == iface->mgmt_frame_bitmap)
		return;

	skw_chip_dbg(skw->idx, "%s %s filter %s\n", skw_iftype_name(wdev->iftype),
		reg ? "add" : "del", skw_mgmt_name(frame_type));

	param.frame_type = frame_type;
	param.reg = reg;
	ts = local_clock();
	do_div(ts, 1000000);

	param.timestamp = ts;
	ret = skw_msg_xmit(wiphy, iface->id, SKW_CMD_REGISTER_FRAME,
			   &param, sizeof(param), NULL, 0);
	if (ret) {
		skw_chip_err(skw->idx, "%s %s failed, ret: %d\n",
			reg ? "add" : "del",
			skw_mgmt_name(frame_type), ret);
	}
}
#endif

static int skw_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev,
				bool enabled, int timeout)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	/* firmware trigger legacy ps automatically */
	skw_chip_dbg(skw->idx, "%s, enabled: %d, timeout: %d\n",
		netdev_name(dev), enabled, timeout);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static int skw_set_qos_map(struct wiphy *wiphy, struct net_device *dev,
			    struct cfg80211_qos_map *qos_map)
{
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "ndev: %s, %s qos_map\n", netdev_name(dev),
		qos_map ? "add" : "del");

	if (!qos_map) {
		SKW_KFREE(iface->qos_map);
		return 0;
	}

	if (!iface->qos_map) {
		iface->qos_map = SKW_ZALLOC(sizeof(struct cfg80211_qos_map), GFP_KERNEL);
		if (!iface->qos_map)
			return -ENOMEM;
	}

	memcpy(iface->qos_map, qos_map, sizeof(*qos_map));

	return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
static int skw_add_tx_ts(struct wiphy *wiphy, struct net_device *ndev,
			u8 tsid, const u8 *peer, u8 up, u16 admitted_time)
{
	struct skw_ts_info ts;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, ts id: %d, addr: %pM, up: %d, time: %d\n",
		netdev_name(ndev), tsid, peer, up, admitted_time);
	/* cfg80211 will make a sanity check */
	ts.up = up;
	ts.tsid = tsid;
	skw_ether_copy(ts.peer, peer);
	ts.admitted_time = admitted_time;

	return skw_send_msg(wiphy, ndev, SKW_CMD_ADD_TX_TS,
			    &ts, sizeof(ts), NULL, 0);
}

static int skw_del_tx_ts(struct wiphy *wiphy, struct net_device *ndev,
				u8 tsid, const u8 *peer)
{
	struct skw_ts_info ts;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, ts id: %d, addr: %pM\n",
		netdev_name(ndev), tsid, peer);

	ts.tsid = tsid;
	skw_ether_copy(ts.peer, peer);
	ts.up = 0xFF;
	ts.admitted_time = 0;

	return skw_send_msg(wiphy, ndev, SKW_CMD_DEL_TX_TS,
			    &ts, sizeof(ts), NULL, 0);
}
#endif

static int skw_tdls_oper(struct wiphy *wiphy, struct net_device *ndev,
			 const u8 *peer_addr, enum nl80211_tdls_operation oper)
{
	int ret = 0;
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_tdls_oper tdls;
	struct skw_peer_ctx *ctx;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, oper: %d, addr: %pM\n",
		netdev_name(ndev), oper, peer_addr);

	ctx = skw_peer_ctx(iface, peer_addr);
	if (!ctx)
		return -ENOENT;

	switch (oper) {
	case NL80211_TDLS_ENABLE_LINK:
		skw_peer_ctx_transmit(ctx, true);
		break;

	case NL80211_TDLS_DISABLE_LINK:
		skw_peer_ctx_transmit(ctx, false);
		skw_peer_ctx_bind(iface, ctx, NULL);

		break;

	default:
		ret = -ENOTSUPP;
		break;
	}

	if (ret)
		return ret;

	tdls.oper = oper;
	skw_ether_copy(tdls.peer_addr, peer_addr);

	return skw_send_msg(wiphy, ndev, SKW_CMD_TDLS_OPER, &tdls,
			    sizeof(tdls), NULL, 0);

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static int skw_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *ndev,
			 const u8 *peer_addr, enum nl80211_tdls_operation oper)
{
	return skw_tdls_oper(wiphy, ndev, peer_addr, oper);
}
#else
static int skw_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *ndev,
			 u8 *peer_addr, enum nl80211_tdls_operation oper)
{
	return skw_tdls_oper(wiphy, ndev, (const u8 *)peer_addr, oper);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
static int skw_tdls_chn_switch(struct wiphy *wiphy, struct net_device *ndev,
		const u8 *addr, u8 oper_class, struct cfg80211_chan_def *def)
{
	int ret;
	struct skw_tdls_chan_switch tdls;
	struct skw_peer_ctx *ctx;
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, addr: %pM, def chan: %d\n",
		netdev_name(ndev), addr, def->chan->hw_value);

	ctx = skw_peer_ctx(iface, addr);
	if (!ctx) {
		skw_chip_err(skw->idx, "can't find tdls peer: %pM\n", addr);
		return -EINVAL;
	}

	if (!skw_channel_allowed(wiphy, def->chan->hw_value))
		return -EBUSY;

	switch (def->width) {
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		tdls.chan_width = SKW_CHAN_WIDTH_20;
		break;
	case NL80211_CHAN_WIDTH_40:
		tdls.chan_width = SKW_CHAN_WIDTH_40;
		break;
	case NL80211_CHAN_WIDTH_80:
		tdls.chan_width = SKW_CHAN_WIDTH_80;
		break;
	default:
		skw_chip_err(skw->idx, "channel width: %d not support\n", def->width);
		return -ENOTSUPP;
	}

	skw_ether_copy(tdls.addr, addr);
	tdls.chn_switch_enable = 1;
	tdls.oper_class = oper_class;
	tdls.chn = def->chan->hw_value;
	tdls.band = to_skw_band(def->chan->band);

	ret = skw_send_msg(wiphy, ndev, SKW_CMD_TDLS_CHANNEL_SWITCH,
			   &tdls, sizeof(tdls), NULL, 0);
	if (!ret) {
		skw_peer_ctx_lock(ctx);

		if (ctx->peer)
			ctx->peer->channel = def->chan->hw_value;

		skw_peer_ctx_unlock(ctx);
	}

	return ret;
}

static void skw_tdls_cancel_chn_switch(struct wiphy *wiphy,
		struct net_device *ndev, const u8 *addr)
{
	struct skw_tdls_chan_switch tdls;
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, addr: %pM\n", netdev_name(ndev), addr);

	if (!skw_peer_ctx(iface, addr)) {
		skw_chip_dbg(skw->idx, "can't find tdls peer:%pM\n", addr);
		return;
	}

	memset(&tdls, 0x0, sizeof(tdls));

	tdls.chn_switch_enable = 0;
	skw_ether_copy(tdls.addr, addr);

	if (skw_send_msg(wiphy, ndev, SKW_CMD_TDLS_CHANNEL_SWITCH,
			 &tdls, sizeof(tdls), NULL, 0) < 0)
		skw_chip_err(skw->idx, "set command SKW_CMD_TDLS_CANCEL_CHN_SWITCH failed\n");
}
#endif

static int skw_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			const u8 *peer, int link_id, u8 action_code,
			u8 dialog_token, u16 status_code, u32 peer_capability,
			bool initiator, const u8 *buf, size_t len)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	return skw_tdls_build_send_mgmt(skw, dev, peer, action_code,
			dialog_token, status_code, peer_capability,
			initiator, buf, len);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static int skw_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			     const u8 *peer, int link_id,
			     u8 action_code, u8 dialog_token, u16 status_code,
			     u32 peer_capability, bool initiator,
			     const u8 *buf, size_t len)
{
	return skw_tdls_mgmt(wiphy, dev, peer, link_id, action_code, dialog_token,
			status_code, peer_capability, initiator, buf, len);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
static int skw_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			     const u8 *peer, u8 action_code, u8 dialog_token,
			     u16 status_code, u32 peer_capability,
			     bool initiator, const u8 *buf, size_t len)
{
	return skw_tdls_mgmt(wiphy, dev, peer, 0, action_code, dialog_token,
			status_code, peer_capability, initiator, buf, len);
}

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static int skw_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			     const u8 *peer, u8 action_code, u8 dialog_token,
			     u16 status_code, u32 peer_capability,
			     const u8 *buf, size_t len)
{
	return skw_tdls_mgmt(wiphy, dev, peer, 0, action_code, dialog_token,
			status_code, peer_capability, false, buf, len);
}
#else
static int skw_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			     u8 *peer, u8 action_code, u8 dialog_token,
			     u16 status_code, const u8 *buf, size_t len)
{
	return skw_tdls_mgmt(wiphy, dev, (const u8 *)peer, 0, action_code,
			dialog_token, status_code, 0, false, buf, len);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
//iw phy5 wowlan enable patterns 28+43:34:-:12 16+33:-:11:ee:12:34:-:88:99
static int skw_wow_enable(struct wiphy *wiphy)
{
	int ret = 0;
#ifdef CONFIG_PM
	struct cfg80211_wowlan *wow = wiphy->wowlan_config;
	struct cfg80211_pkt_pattern *patterns = wow->patterns;
	u32 i, j;
	int total;
	struct skw_spd_action_param *spd = NULL;
	struct skw_wow_input_param *wow_param = NULL;
	struct skw_wow_rule *rule;
	struct skw_pkt_pattern *ptn;
	struct skw_pkt_pattern ptn_tmp;
	int vi = 0;
	int y, b, start = 0, gap = 0;
	u8 *rdata;
	struct skw_core *skw = wiphy_priv(wiphy);

	total = sizeof(struct skw_spd_action_param) +
			 sizeof(struct skw_wow_input_param);

	if (wow->any) {
		spd = SKW_ZALLOC(total, GFP_KERNEL);
		if (!spd) {
			skw_chip_err(skw->idx, "malloc failed, size: %d\n", total);
			return -ENOMEM;
		}

		wow_param = (struct skw_wow_input_param *)((u8 *)spd
			 + sizeof(*spd));
		wow_param->wow_flags = SKW_WOW_ANY_PKT;
		wow_param->rule_num = 0;
		spd->sub_cmd = ACTION_EN_WOW;
		spd->len = sizeof(struct skw_wow_input_param);
		goto cmd_send;
	}

	total += sizeof(struct skw_wow_rule) * wow->n_patterns;

	spd = SKW_ZALLOC(total, GFP_KERNEL);
	if (!spd) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	wow_param = (struct skw_wow_input_param *)((u8 *)spd
			+ sizeof(*spd));
	wow_param->rule_num = wow->n_patterns;
	spd->sub_cmd = ACTION_EN_WOW;

	if (wow->disconnect)
		wow_param->wow_flags |= SKW_WOW_DISCONNECT;

	if (wow->magic_pkt)
		wow_param->wow_flags |= SKW_WOW_MAGIC_PKT;

	if (wow->gtk_rekey_failure)
		wow_param->wow_flags |= SKW_WOW_GTK_REKEY_FAIL;

	if (wow->eap_identity_req)
		wow_param->wow_flags |= SKW_WOW_EAP_IDENTITY_REQ;

	if (wow->four_way_handshake)
		wow_param->wow_flags |= SKW_WOW_FOUR_WAY_HANDSHAKE;

	if (wow->rfkill_release)
		wow_param->wow_flags |= SKW_WOW_RFKILL_RELEASE;

	for (i = 0; i < wow_param->rule_num; i++) {
		rule = &wow_param->rules[i];
		rdata = rule->rule;
		ptn_tmp.op = PAT_OP_TYPE_SAME;
		ptn_tmp.type_offset = PAT_TYPE_ETH;
		ptn_tmp.offset = patterns[i].pkt_offset;
		ptn_tmp.len = 0;

		vi = 0;
		start = 0;
		gap = 0;
		for (j = 0; j < patterns[i].pattern_len; j++) {
			y = round_up(j + 1, 8)/8 - 1;
			b = j%8;
			if (patterns[i].mask[y] & BIT(b)) {
				if (!start) {
					if (vi + sizeof(ptn_tmp)
						>= sizeof(rule->rule)) {
						skw_chip_warn(skw->idx, "pat:%d overage\n", i);
						break;
					}

					ptn =
					(struct skw_pkt_pattern *)&rdata[vi];
					memcpy(ptn, &ptn_tmp, sizeof(ptn_tmp));
					ptn->offset += gap;
					vi += sizeof(ptn_tmp);
				}

				rdata[vi++] = patterns[i].pattern[j];
				ptn->len++;
				start = 1;
				gap++;

				if (vi >= sizeof(rule->rule)) {
					skw_chip_warn(skw->idx, "pat:%d overage\n", i);
					break;
				}
			} else {
				gap++;
				start = 0;
			}
		}
		rule->len = vi;

		skw_hex_dump("rule", rule, sizeof(*rule), false);
	}

	spd->len = sizeof(struct skw_wow_input_param) +
		sizeof(struct skw_wow_rule) * wow_param->rule_num;

cmd_send:
	skw_chip_dbg(skw->idx, "len:%d %d\n", spd->len, total);
	skw_hex_dump("wow", spd, total, false);

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SET_SPD_ACTION,
			spd, total, NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	SKW_KFREE(spd);
#endif
	return ret;
}
#endif

int skw_wow_disable(struct wiphy *wiphy)
{
	struct skw_spd_action_param spd;
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	spd.sub_cmd = ACTION_DIS_WOW;
	spd.len = 0;

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SET_SPD_ACTION,
			&spd, sizeof(spd), NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "failed, ret: %d\n", ret);

	return ret;
}

int skw_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	int ret;
	unsigned long flags;
	struct skw_suspend_t *suspend;
	struct skw_tlv_conf conf;
	u8 *tcp_off;
	struct skw_core *skw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	struct timespec64 time;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	struct timespec time;
#else
	struct timeval time;
#endif
	int total, fixed, tcp_tlv_data_len;

	tcp_off = SKW_ZALLOC(SKW_CMD_MAX_LEN, GFP_KERNEL);
	if (IS_ERR_OR_NULL(tcp_off)) {
		skw_chip_err(skw->idx, "malloc failed\n");
		return -ENOMEM;
	}

	tcp_tlv_data_len = skw_fill_tcpalive_offload_info(wiphy, tcp_off);

	fixed = sizeof(struct skw_suspend_t);
	total = fixed + sizeof(struct skw_tlv) +
		 tcp_tlv_data_len;

	suspend = SKW_ZALLOC(total, GFP_KERNEL);
	if (IS_ERR_OR_NULL(suspend)) {
		skw_chip_err(skw->idx, "malloc failed, size: %d\n", total);
		ret = -ENOMEM;
		goto free_tcp_off;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	ktime_get_real_ts64(&time);
	ms_sus = time.tv_sec * 1000 + time.tv_nsec / 1000000;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	getnstimeofday(&time);
	ms_sus = time.tv_sec * 1000 + time.tv_nsec / 1000000;
#else
	do_gettimeofday(&time);
	ms_sus = time.tv_sec * 1000 + time.tv_usec / 1000;
#endif

	skw_chip_dbg(skw->idx, "WoW: %s, skw flags: 0x%lx\n",
		wow ? "enabled" : "disabled", skw->flags);

	/*
	 * If we have transmitted packets, but don't receive the txc on pcie
	 * platform, just return busy, because hw can't access the host
	 * memory while host is sleep.
	 */
#if 0
	if (skw->hw.bus == SKW_BUS_PCIE &&
		!skw_edma_is_txc_completed(skw)) {
		skw_chip_dbg(skw->idx, "txc is not completed");
		goto free;
	}
#endif

	set_bit(SKW_FLAG_BLOCK_TX, &skw->flags);

	memset(suspend, 0x0, sizeof(*suspend));

	conf.buff = (u8 *)suspend + sizeof(*suspend);
	conf.total_len = 0;
	conf.buff_len = sizeof(struct skw_tlv) +
		 tcp_tlv_data_len;

	if (skw_tlv_add(&conf, SKW_SET_TCP_KEEPALIVE_OFFLOAD,
			tcp_off, tcp_tlv_data_len))
		skw_chip_err(skw->idx, "add SKW_SET_TCP_KEEPALIVE_OFFLOAD failed.\n");

	skw_hex_dump("off_keep:", tcp_off, tcp_tlv_data_len, false);

	skw_hex_dump("total:", suspend, total, false);

	/* WoW disabled */
	if (!wow) {
		suspend->wow_enable = 0;
		goto send;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	if (wow->nd_config)
		skw_cfg80211_sched_scan_start(wiphy, wow->nd_config->dev, wow->nd_config);
#endif

	suspend->wow_enable = 1;

	if (wow->disconnect)
		suspend->wow_flags |= SKW_WOW_DISCONNECT;

	if (wow->magic_pkt)
		suspend->wow_flags |= SKW_WOW_MAGIC_PKT;

	if (wow->gtk_rekey_failure)
		suspend->wow_flags |= SKW_WOW_GTK_REKEY_FAIL;

	if (wow->eap_identity_req)
		suspend->wow_flags |= SKW_WOW_EAP_IDENTITY_REQ;

	if (wow->four_way_handshake)
		suspend->wow_flags |= SKW_WOW_FOUR_WAY_HANDSHAKE;

	if (wow->rfkill_release)
		suspend->wow_flags |= SKW_WOW_RFKILL_RELEASE;

send:
	flags = BIT(SKW_CMD_FLAG_IGNORE_BLOCK_TX) |
		BIT(SKW_CMD_FLAG_NO_ACK) |
		BIT(SKW_CMD_FLAG_NO_WAKELOCK);

	if (skw->hw.bus == SKW_BUS_SDIO)
		flags |= BIT(SKW_CMD_FLAG_DISABLE_IRQ);

	ret = skw_msg_xmit_timeout(wiphy, 0, SKW_CMD_SUSPEND, suspend,
				   total, NULL, 0,
				   "SKW_CMD_SUSPEND",
				   msecs_to_jiffies(SKW_CMD_TIMEOUT), flags);
	if (ret) {
		clear_bit(SKW_FLAG_BLOCK_TX, &skw->flags);

		skw_chip_err(skw->idx, "ret: %d, fw flags: 0x%lx\n", ret, skw->flags);
	}

//free:
	SKW_KFREE(suspend);
free_tcp_off:
	SKW_KFREE(tcp_off);
	return  ret;
}

static int skw_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "bus: %s, hw flags: 0x%lx\n", skw_bus_name(skw->hw.bus), skw->hw.flags);

	if (test_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags))
		return skw_suspend(wiphy, wow);

	if (!wow) {
		skw->hw.wow.flags = 0;
		skw->hw.wow.enabled = false;

		return 0;
	}

	if (wow->disconnect)
		skw->hw.wow.flags |= SKW_WOW_DISCONNECT;

	if (wow->magic_pkt)
		skw->hw.wow.flags |= SKW_WOW_MAGIC_PKT;

	if (wow->gtk_rekey_failure)
		skw->hw.wow.flags |= SKW_WOW_GTK_REKEY_FAIL;

	if (wow->eap_identity_req)
		skw->hw.wow.flags |= SKW_WOW_EAP_IDENTITY_REQ;

	if (wow->four_way_handshake)
		skw->hw.wow.flags |= SKW_WOW_FOUR_WAY_HANDSHAKE;

	if (wow->rfkill_release)
		skw->hw.wow.flags |= SKW_WOW_RFKILL_RELEASE;

	skw->hw.wow.enabled = true;

	return 0;
}

int skw_resume(struct wiphy *wiphy)
{
	int ret = 0;
	int ms_rsm;
	struct skw_resume_param ret_resume;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	struct timespec64 time;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	struct timespec time;
#else
	struct timeval time;
#endif

	struct skw_core *skw = wiphy_priv(wiphy);

	clear_bit(SKW_FLAG_BLOCK_TX, &skw->flags);

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_RESUME, NULL, 0,
			    &ret_resume, sizeof(ret_resume));
	if (ret)
		skw_chip_warn(skw->idx, "ret: %d\n", ret);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	ktime_get_real_ts64(&time);
	ms_rsm = time.tv_sec * 1000 + time.tv_nsec / 1000000;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	getnstimeofday(&time);
	ms_rsm = time.tv_sec * 1000 + time.tv_nsec / 1000000;
#else
	do_gettimeofday(&time);
	ms_rsm = time.tv_sec * 1000 + time.tv_usec / 1000;
#endif

	skw_chip_dbg(skw->idx, "skw_flags: 0x%lx ret: %d. sleep: %dms (0x%x-0x%x). cptime:%d.\n",
		skw->flags, ret,
		ms_rsm - ms_sus,
		ms_rsm, ms_sus,
		ret_resume.suspend_time);

	skw_chip_dbg(skw->idx, "wake by: event_id:%d. msdu_status:%d, msdu_num:%d. wow_idx:%d(idxcnt:%d)\n",
		ret_resume.wake_event_id,
		ret_resume.wake_msdu_filter,
		ret_resume.wake_msdu_num,
		skw_wow_filter_remap_idx(ret_resume.wake_wow_idx),
		ret_resume.wake_wow_idx);

	skw_hex_dump("fltcnt:", ret_resume.filter_cnt,
		sizeof(ret_resume.filter_cnt), true);
	skw_hex_dump("fltdropcnt:", ret_resume.filter_drop_cnt,
		sizeof(ret_resume.filter_drop_cnt), true);

	return 0;
}

static int skw_cfg80211_resume(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "bus: %s, hw flags: 0x%lx\n", skw_bus_name(skw->hw.bus), skw->hw.flags);

	if (test_bit(SKW_HW_FLAG_CFG80211_PM, &skw->hw.flags))
		return skw_resume(wiphy);

	return 0;
}

static void skw_set_wakeup(struct wiphy *wiphy, bool enabled)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	if (enabled)
		skw_wow_enable(wiphy);
	else
		skw_wow_disable(wiphy);
#endif

	device_set_wakeup_enable(wiphy_dev(wiphy), enabled);
}

static int skw_start_p2p_device(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "traced\n");

	return 0;
}

static void skw_stop_p2p_device(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "traced\n");
}

static int skw_probe_client(struct wiphy *wiphy, struct net_device *dev,
			    const u8 *peer, u64 *cookie)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "traced\n");

	return 0;
}

static int skw_change_bss(struct wiphy *wiphy, struct net_device *ndev,
		struct bss_parameters *params)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s ap_isolate:%d\n", netdev_name(ndev), params->ap_isolate);

	if (params->ap_isolate >= 0)
		iface->sap.ap_isolate = params->ap_isolate;

	return 0;
}

static int skw_set_monitor_channel(struct wiphy *wiphy,
		struct cfg80211_chan_def *chandef)
{
	return skw_cmd_monitor(wiphy, chandef, SKW_MONITOR_COMMON);
}

static int skw_dump_survey(struct wiphy *wiphy, struct net_device *ndev,
		int idx, struct survey_info *info)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_survey_info *sinfo = NULL;
	int freq;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_detail(skw->idx, "%s, idx: %d\n", netdev_name(ndev), idx);

	sinfo = list_first_entry_or_null(&iface->survey_list,
					 struct skw_survey_info, list);
	if (!sinfo) {
		skw_chip_dbg(skw->idx, "last idx: %d\n", idx);
		return -EINVAL;
	}

	list_del(&sinfo->list);

	freq = ieee80211_channel_to_frequency(sinfo->data.chan,
			to_nl80211_band(sinfo->data.band));
	info->noise = sinfo->data.noise;
	info->channel = ieee80211_get_channel(wiphy, freq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	info->time = sinfo->data.time;
	info->time_busy = sinfo->data.time_busy;
	info->time_ext_busy = sinfo->data.time_ext_busy;
	info->filled = SURVEY_INFO_TIME |
		       SURVEY_INFO_TIME_BUSY |
		       SURVEY_INFO_TIME_EXT_BUSY |
		       SURVEY_INFO_NOISE_DBM;
#else
	info->channel_time = sinfo->data.time;
	info->channel_time_busy = sinfo->data.time_busy;
	info->channel_time_ext_busy = sinfo->data.time_ext_busy;
	info->filled = SURVEY_INFO_CHANNEL_TIME |
		       SURVEY_INFO_CHANNEL_TIME_BUSY |
		       SURVEY_INFO_CHANNEL_TIME_EXT_BUSY |
		       SURVEY_INFO_NOISE_DBM;
#endif

	SKW_KFREE(sinfo);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
static int skw_external_auth(struct wiphy *wiphy, struct net_device *ndev,
		struct cfg80211_external_auth_params *params)
{
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "%s bssid: %pM, action: %u, status: %u\n",
		 netdev_name(ndev), params->bssid,
		 params->action, params->status);

	if (iface->wdev.iftype == NL80211_IFTYPE_AP ||
	    iface->wdev.iftype == NL80211_IFTYPE_P2P_GO) {
		return 0;
	}

	/* Non-AP STA */
	if (!iface->sta.conn) {
		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
		return -EINVAL;
	}

	if (params->status != WLAN_STATUS_SUCCESS) {
		skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_NONE);
		skw_unjoin(wiphy, ndev, params->bssid, SKW_LEAVE, false);
		// release peer and report connect result

		cfg80211_connect_result(iface->ndev, params->bssid,
					NULL, 0, NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
		return 0;
	}

	skw_set_state(skw, &iface->sta.core.sm, SKW_STATE_AUTHED);

	return skw_connect_assoc(wiphy, ndev, iface->sta.conn);
}
#endif

static int skw_update_ft_ies(struct wiphy *wiphy, struct net_device *dev,
			     struct cfg80211_update_ft_ies_params *ftie)
{
#if 0
	struct skw_iface *iface = NULL;
	struct cfg80211_assoc_request req;
	u8 *ie = NULL;
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "md:%u\n", ftie->md);

	if (ftie->ie && ftie->ie_len) {
		iface->sta.ft_ie = SKW_ZALLOC(ftie->ie_len, GFP_KERNEL);
		if (iface->sta.ft_ie)
			memcpy(iface->sta.ft_ie, ftie->ie, ftie->ie_len);
		iface->sta.ft_ie_len = ftie->ie_len;
		skw_chip_dbg(skw->idx, "ft ie len:%u\n", iface->sta.ft_ie_len);
	}

	skw_chip_dbg(skw->idx, "state:%u\n", iface->sta.core.sm.state);
	if (iface->sta.core.sm.state != SKW_STATE_AUTHING) {
		skw_chip_dbg(skw->idx, "received update ft cmd during EAPOL process\n");
		return 0;
	}

	// req.bss = iface->sta.associating_bss;
	req.ie_len = iface->sta.assoc_ie_len + ftie->ie_len;
	ie = SKW_ZALLOC(req.ie_len, GFP_KERNEL);
	if (!ie) {
		skw_chip_err(skw->idx, "Mem is not enough\n");
		return -ENOMEM;
	}
	memcpy(ie, ftie->ie, ftie->ie_len);
	memcpy(ie + ftie->ie_len, iface->sta.assoc_ie,
		iface->sta.assoc_ie_len);

	req.ie = ie;
	req.prev_bssid = iface->sta.core.bssid;
	req.use_mfp = iface->sta.use_mfp;
	req.flags = iface->sta.flags;
	req.ht_capa = iface->sta.ht_capa;
	req.ht_capa_mask = iface->sta.ht_capa_mask;
	req.vht_capa = iface->sta.vht_capa;
	req.vht_capa_mask = iface->sta.vht_capa_mask;

	ret = skw_assoc(iface->wdev.wiphy, iface->ndev, &req);

	SKW_KFREE(ie);
	return ret;
#endif
	return 0;
}

static int skw_start_radar_detection(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_chan_def *chandef, u32 cac_time_ms)
{
	int ret;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "dev: %s, channel: %d, cac time: %dms, dfs_region: %d, fw enabled: %d\n",
		netdev_name(dev), chandef->chan->hw_value, cac_time_ms,
		skw->dfs.region, skw->dfs.fw_enabled);

	if (!skw->dfs.fw_enabled)
		return -EINVAL;

	ret = skw_dfs_chan_init(wiphy, dev, chandef, cac_time_ms);
	if (ret)
		return ret;

	ret = skw_dfs_start_cac(wiphy, dev);
	if (!ret) {
		set_bit(SKW_DFS_FLAG_CAC_MODE, &iface->sap.dfs.flags);
		queue_delayed_work(skw->event_wq, &iface->sap.dfs.cac_work,
				msecs_to_jiffies(cac_time_ms));
	}

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
static int skw_cfg80211_start_radar_detection(struct wiphy *wiphy,
					  struct net_device *dev,
					  struct cfg80211_chan_def *chandef,
					  u32 cac_time_ms, int link_id)
{
    return skw_start_radar_detection(wiphy, dev, chandef, cac_time_ms);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
static int skw_cfg80211_start_radar_detection(struct wiphy *wiphy,
			struct net_device *dev,
			struct cfg80211_chan_def *chandef, u32 cac_time_ms)
{
	return skw_start_radar_detection(wiphy, dev, chandef, cac_time_ms);
}
#else
static int skw_cfg80211_start_radar_detection(struct wiphy *wiphy,
			struct net_device *dev, struct cfg80211_chan_def *chandef)
{
	return skw_start_radar_detection(wiphy, dev, chandef, IEEE80211_DFS_MIN_CAC_TIME_MS);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
static int skw_channel_switch(struct wiphy *wiphy, struct net_device *dev,
				  struct cfg80211_csa_settings *csa)
{
	int ret;
	char *buff;
	const u8 *ie;
	int ie_len, offset = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "dev: %s, chan: %d, width: %d\n",
		netdev_name(dev), csa->chandef.chan->hw_value,
		csa->chandef.width);

	buff = SKW_ZALLOC(csa->beacon_csa.tail_len, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	ie = cfg80211_find_ie(WLAN_EID_CHANNEL_SWITCH,
				  csa->beacon_csa.tail,
				  csa->beacon_csa.tail_len);
	if (ie) {
		ie_len = ie[1] + 2;

		memcpy(buff, ie, ie_len);
		offset = ie_len;
	}

	ie = cfg80211_find_ie(WLAN_EID_EXT_CHANSWITCH_ANN,
				   csa->beacon_csa.tail,
				   csa->beacon_csa.tail_len);
	if (ie) {
		ie_len = ie[1] + 2;

		memcpy(buff + offset, ie, ie_len);
		offset += ie_len;
	}

	ie = cfg80211_find_ie(WLAN_EID_WIDE_BW_CHANNEL_SWITCH,
				   csa->beacon_csa.tail,
				   csa->beacon_csa.tail_len);
	if (ie) {
		ie_len = ie[1] + 2;

		memcpy(buff + offset, ie, ie_len);
		offset += ie_len;
	}

	ie = cfg80211_find_ie(WLAN_EID_CHANNEL_SWITCH_WRAPPER,
				   csa->beacon_csa.tail,
				   csa->beacon_csa.tail_len);
	if (ie) {
		ie_len = ie[1] + 2;

		memcpy(buff + offset, ie, ie_len);
		offset += ie_len;
	}

	ret = skw_send_msg(wiphy, dev, SKW_CMD_REQ_CHAN_SWITCH,
			buff, offset, NULL, 0);

	SKW_KFREE(buff);

	return ret;
}
#endif

#ifdef __SKW_ANDROID__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)
static int skw_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
		int link_id, u8 key_idx, bool pairwise,
		const u8 *mac_addr, void *cookie,
		void (*callback)(void *cookie, struct key_params *params))
{
	return skw_get_key(wiphy, dev, link_id, key_idx, pairwise,
			mac_addr, cookie, callback);
}

static int skw_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool pairwise,
			const u8 *addr, struct key_params *params)
{
	return skw_add_key(wiphy, dev, link_id, key_idx, pairwise, addr, params);
}

static int skw_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool pairwise, const u8 *addr)
{
	return skw_del_key(wiphy, dev, link_id, key_idx, pairwise, addr);
}

static int skw_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			       int link_id, u8 key_idx, bool unicast, bool multicast)
{
	return skw_set_default_key(wiphy, dev, link_id, key_idx, unicast, multicast);
}

static int skw_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *dev, int link_id, u8 key_idx)
{
	return skw_set_default_mgmt_key(wiphy, dev, link_id, key_idx);
}

static int skw_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_disassoc_request *req)
{
	bool tx = !req->local_state_change;
	const u8 *bssid = req->ap_addr;

	return skw_disassoc(wiphy, dev, bssid, req->reason_code, tx);
}

#else
static int skw_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
		u8 key_idx, bool pairwise, const u8 *mac_addr, void *cookie,
		void (*callback)(void *cookie, struct key_params *params))
{
	return skw_get_key(wiphy, dev, 0, key_idx, pairwise,
			mac_addr, cookie, callback);
}

static int skw_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
			u8 key_idx, bool pairwise,
			const u8 *addr, struct key_params *params)
{
	return skw_add_key(wiphy, dev, 0, key_idx, pairwise, addr, params);
}

static int skw_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
			u8 key_idx, bool pairwise, const u8 *addr)
{
	return skw_del_key(wiphy, dev, 0, key_idx, pairwise, addr);
}

static int skw_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			       u8 key_idx, bool unicast, bool multicast)
{
	return skw_set_default_key(wiphy, dev, 0, key_idx, unicast, multicast);
}

static int skw_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *dev, u8 key_idx)
{
	return skw_set_default_mgmt_key(wiphy, dev, 0, key_idx);
}

static int skw_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_disassoc_request *req)
{
	bool tx = !req->local_state_change;
	const u8 *bssid = (const u8 *)req->bss->bssid;

	return skw_disassoc(wiphy, dev, bssid, req->reason_code, tx);
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41)
static int skw_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev,
			unsigned int link_id)
{
	return skw_stop_ap(wiphy, dev, link_id);
}
#else
static int skw_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	return skw_stop_ap(wiphy, dev, 0);
}
#endif

#else /* Linux */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static int skw_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
		int link_id, u8 key_idx, bool pairwise,
		const u8 *mac_addr, void *cookie,
		void (*callback)(void *cookie, struct key_params *params))
{
	return skw_get_key(wiphy, dev, link_id, key_idx, pairwise,
			mac_addr, cookie, callback);
}

static int skw_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool pairwise,
			const u8 *addr, struct key_params *params)
{
	return skw_add_key(wiphy, dev, link_id, key_idx, pairwise, addr, params);
}

static int skw_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
			int link_id, u8 key_idx, bool pairwise, const u8 *addr)
{
	return skw_del_key(wiphy, dev, link_id, key_idx, pairwise, addr);
}

static int skw_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			       int link_id, u8 key_idx, bool unicast, bool multicast)
{
	return skw_set_default_key(wiphy, dev, link_id, key_idx, unicast, multicast);
}

static int skw_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *dev, int link_id, u8 key_idx)
{
	return skw_set_default_mgmt_key(wiphy, dev, link_id, key_idx);
}

static int skw_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_disassoc_request *req)
{
	bool tx = !req->local_state_change;
	const u8 *bssid = req->ap_addr;

	return skw_disassoc(wiphy, dev, bssid, req->reason_code, tx);
}

static int skw_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev,
		unsigned int link_id)
{
	return skw_stop_ap(wiphy, dev, link_id);
}
#else
static int skw_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
		u8 key_idx, bool pairwise, const u8 *mac_addr, void *cookie,
		void (*callback)(void *cookie, struct key_params *params))
{
	return skw_get_key(wiphy, dev, 0, key_idx, pairwise,
			mac_addr, cookie, callback);
}

static int skw_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
			u8 key_idx, bool pairwise,
			const u8 *addr, struct key_params *params)
{
	return skw_add_key(wiphy, dev, 0, key_idx, pairwise, addr, params);
}

static int skw_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
			u8 key_idx, bool pairwise, const u8 *addr)
{
	return skw_del_key(wiphy, dev, 0, key_idx, pairwise, addr);
}

static int skw_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			       u8 key_idx, bool unicast, bool multicast)
{
	return skw_set_default_key(wiphy, dev, 0, key_idx, unicast, multicast);
}

static int skw_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *dev, u8 key_idx)
{
	return skw_set_default_mgmt_key(wiphy, dev, 0, key_idx);
}

static int skw_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_disassoc_request *req)
{
	bool tx = !req->local_state_change;
	const u8 *bssid = (const u8 *)req->bss->bssid;

	return skw_disassoc(wiphy, dev, bssid, req->reason_code, tx);
}

static int skw_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	return skw_stop_ap(wiphy, dev, 0);
}
#endif

#endif /* Linux */

static struct cfg80211_ops skw_cfg80211_ops  = {
	.add_virtual_intf = skw_cfg80211_add_virtual_intf,
	.del_virtual_intf = skw_cfg80211_del_virtual_intf,
	.change_virtual_intf = skw_cfg80211_change_intf,
	.scan = skw_scan,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	.abort_scan = skw_abort_scan,
#endif
	.get_key = skw_cfg80211_get_key,
	.add_key = skw_cfg80211_add_key,
	.del_key = skw_cfg80211_del_key,
	.set_default_key = skw_cfg80211_set_default_key,
	.set_default_mgmt_key = skw_cfg80211_set_default_mgmt_key,
	.change_beacon = skw_change_beacon,
	.start_ap = skw_cfg80211_start_ap,
	.stop_ap = skw_cfg80211_stop_ap,
	.add_station = skw_cfg80211_add_station,
	.change_station = skw_cfg80211_change_station,
	.del_station = skw_cfg80211_del_station,
	.get_station = skw_cfg80211_get_station,
	.auth = skw_cfg80211_auth,
	.assoc = skw_cfg80211_assoc,
	.deauth = skw_cfg80211_deauth,
	.disassoc = skw_cfg80211_disassoc,
	.connect = skw_cfg80211_connect,
	.disconnect = skw_cfg80211_disconnect,
	.join_ibss = skw_join_ibss,
	.leave_ibss = skw_leave_ibss,
	.set_wiphy_params = skw_set_wiphy_params,
	.remain_on_channel = skw_remain_on_channel,
	.cancel_remain_on_channel = skw_cancel_roc,
	.mgmt_tx = skw_cfg80211_mgmt_tx,
	.sched_scan_start = skw_cfg80211_sched_scan_start,
	.sched_scan_stop = skw_cfg80211_sched_scan_stop,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0))
	.update_mgmt_frame_registrations = skw_mgmt_frame_register,
#else
	.mgmt_frame_register = skw_mgmt_frame_register,
#endif
	.set_power_mgmt = skw_set_power_mgmt,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	.set_cqm_rssi_config = skw_set_cqm_rssi_config,
	.set_cqm_rssi_range_config = skw_set_cqm_rssi_range_config,
#endif
	.start_p2p_device = skw_start_p2p_device,
	.stop_p2p_device = skw_stop_p2p_device,
	.set_mac_acl = skw_set_mac_acl,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	.set_qos_map = skw_set_qos_map,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	.add_tx_ts = skw_add_tx_ts,
	.del_tx_ts = skw_del_tx_ts,
#endif
	.tdls_mgmt = skw_cfg80211_tdls_mgmt,
	.tdls_oper = skw_cfg80211_tdls_oper,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	.tdls_channel_switch = skw_tdls_chn_switch,
	.tdls_cancel_channel_switch = skw_tdls_cancel_chn_switch,
#endif
	.suspend = skw_cfg80211_suspend,
	.resume = skw_cfg80211_resume,
	.set_wakeup = skw_set_wakeup,
	.probe_client = skw_probe_client,
	.dump_survey = skw_dump_survey,
	.set_monitor_channel = skw_set_monitor_channel,
	.change_bss = skw_change_bss,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
	.external_auth = skw_external_auth,
#endif
	.update_ft_ies = skw_update_ft_ies,
	.start_radar_detection = skw_cfg80211_start_radar_detection,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	.channel_switch = skw_channel_switch,
#endif
};

static void skw_regd_notifier(struct wiphy *wiphy,
			      struct regulatory_request *req)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_info(skw->idx, "regd: %s, initiator = %d, dfs_region: %d\n",
		req->alpha2, req->initiator, req->dfs_region);

	skw->dfs.region = req->dfs_region;

	if (!skw_set_wiphy_regd(wiphy, req->alpha2))
		skw_cmd_set_regdom(wiphy, req->alpha2);
}

struct wiphy *skw_alloc_wiphy(int priv_size)
{
#ifdef CONFIG_SWT6621S_STA_SME_EXT
	skw_cfg80211_ops.connect = NULL;
	skw_cfg80211_ops.disconnect = NULL;
#else
	skw_cfg80211_ops.auth = NULL;
	skw_cfg80211_ops.assoc = NULL;
	skw_cfg80211_ops.deauth = NULL;
	skw_cfg80211_ops.disassoc = NULL;
#endif

	return wiphy_new(&skw_cfg80211_ops, priv_size);
}

#ifdef CONFIG_PM
/* cfg80211 wowlan definitions */
#define SKW_WOWLAN_MAX_PATTERNS              15
#define SKW_WOWLAN_MIN_PATTERN_LEN           1
#define SKW_WOWLAN_MAX_PATTERN_LEN           255
#define SKW_WOWLAN_PKT_FILTER_ID_FIRST       201

static const struct wiphy_wowlan_support skw_wowlan_support = {
	.flags = WIPHY_WOWLAN_ANY |
		 WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_MAGIC_PKT,
	.n_patterns = SKW_WOWLAN_MAX_PATTERNS,
	.pattern_min_len = SKW_WOWLAN_MIN_PATTERN_LEN,
	.pattern_max_len = SKW_WOWLAN_MAX_PATTERN_LEN,
	.max_pkt_offset = SKW_WOWLAN_MAX_PATTERN_LEN,
};
#endif /* CONFIG_PM */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
struct skw_iftype_ext_cap iftype_ext_cap[NUM_NL80211_IFTYPES] = {
	{NL80211_IFTYPE_STATION,	{0}, 0},
	{NL80211_IFTYPE_AP,		{0}, 0},
	{NL80211_IFTYPE_P2P_GO,		{0}, 0},
#ifndef CONFIG_SWT6621S_LEGACY_P2P
	{NL80211_IFTYPE_P2P_DEVICE,	{0}, 0},
#endif
};

static struct skw_iftype_ext_cap *skw_get_iftype_ext_cap(u8 iftype)
{
	int i;
	struct skw_iftype_ext_cap *capab = NULL;

	for (i = 0; i < NUM_NL80211_IFTYPES; i++) {
		if (iftype_ext_cap[i].iftype == iftype)
			capab = &iftype_ext_cap[iftype];
	}

	return capab;
}

static void skw_setup_wiphy_iftype_ext_cap(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);
	struct wiphy_iftype_ext_capab *capab = NULL;
	struct skw_iftype_ext_cap *skw_ext_cap = NULL;

	skw->num_iftype_ext_capab  = 0;

	if (wiphy->interface_modes & (BIT(NL80211_IFTYPE_STATION))) {
		capab = &skw->iftype_ext_cap[NL80211_IFTYPE_STATION];
		capab->iftype = NL80211_IFTYPE_STATION;
		skw_ext_cap = skw_get_iftype_ext_cap(capab->iftype);
		capab->extended_capabilities = skw_ext_cap->ext_cap;
		capab->extended_capabilities_mask = skw_ext_cap->ext_cap;
		capab->extended_capabilities_len = skw_ext_cap->ext_cap_len;
		skw->num_iftype_ext_capab++;
	}

	if (wiphy->interface_modes & (BIT(NL80211_IFTYPE_AP))) {
		capab = &skw->iftype_ext_cap[NL80211_IFTYPE_AP];
		capab->iftype = NL80211_IFTYPE_AP;
		skw_ext_cap = skw_get_iftype_ext_cap(capab->iftype);
		capab->extended_capabilities = skw_ext_cap->ext_cap;
		capab->extended_capabilities_mask = skw_ext_cap->ext_cap;
		capab->extended_capabilities_len = skw_ext_cap->ext_cap_len;
		skw->num_iftype_ext_capab++;
	}

	skw->num_iftype_ext_capab  = 0; //Remove it after set the actual info
	wiphy->num_iftype_ext_capab = skw->num_iftype_ext_capab;
	wiphy->iftype_ext_capab = skw->iftype_ext_cap;
}
#endif

static void skw_sync_band_capa(struct skw_core *skw, struct ieee80211_supported_band *band,
				struct skw_chip_info *chip)
{
	u32 flags;
	u16 bit_rate;
	int i, mcs_map;
	int tx_chain = 0, rx_chain = 0;

	band->ht_cap.cap = chip->ht_capa;
	band->ht_cap.ht_supported = true;
	band->ht_cap.ampdu_factor = chip->ht_ampdu_param & 0x3;
	band->ht_cap.ampdu_density = (chip->ht_ampdu_param >> 2) & 0x7;

	for (i = 0; i < 4; i++) {
		mcs_map = (chip->ht_rx_mcs_maps >> (i * 8)) & 0xff;
		if (mcs_map) {
			rx_chain++;
			band->ht_cap.mcs.rx_mask[i] = mcs_map;
		}

		mcs_map = (chip->ht_tx_mcs_maps >> (i * 8)) & 0xff;
		if (mcs_map)
			tx_chain++;
	}

	if (chip->fw_bw_capa & SKW_BW_2GHZ_40M)
		bit_rate = rx_chain * 150; /* Mbps */
	else
		bit_rate = rx_chain * 72;  /* Mbps */

	band->ht_cap.mcs.rx_highest = cpu_to_le16(bit_rate);
	band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
	if (tx_chain != rx_chain) {
		band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_RX_DIFF;
		band->ht_cap.mcs.tx_params |= ((tx_chain - 1) << 2);
	}

	band->vht_cap.cap = chip->vht_capa;
	band->vht_cap.vht_supported = true;
	band->vht_cap.vht_mcs.tx_mcs_map = chip->vht_tx_mcs_maps;
	band->vht_cap.vht_mcs.rx_mcs_map = chip->vht_rx_mcs_maps;

	if (!chip->fw_bw_capa)
		return;

	/* set channel flags */
	for (flags = 0, i = 0; i < 32; i++) {
		if (!(chip->fw_bw_capa & BIT(i))) {
			switch (BIT(i)) {
			case SKW_BW_CAP_2G_20M:
			case SKW_BW_CAP_5G_20M:
				flags |= SKW_IEEE80211_CHAN_NO_20MHZ;
				break;

			case SKW_BW_CAP_2G_40M:
			case SKW_BW_CAP_5G_40M:
				flags |= IEEE80211_CHAN_NO_HT40;
				break;

			case SKW_BW_CAP_5G_80M:
				flags |= IEEE80211_CHAN_NO_80MHZ;
				break;

			case SKW_BW_CAP_5G_160M:
				flags |= IEEE80211_CHAN_NO_160MHZ;
				break;

			default:
				break;
			}
		}
	}

	skw_chip_dbg(skw->idx, "BW capa: 0x%x, flags: 0x%x\n", chip->fw_bw_capa, flags);

#ifdef SKW_SYNC_CHANNEL_FLAGS
	for (i = 0; i < band->n_channels; i++)
		band->channels[i].flags = flags;
#endif
}

int skw_setup_wiphy(struct wiphy *wiphy, struct skw_chip_info *chip)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	wiphy->mgmt_stypes = skw_mgmt_stypes;
#if 0
	wiphy->probe_resp_offload = NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
				NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 |
				NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P;
#endif

	wiphy->flags = WIPHY_FLAG_NETNS_OK |
			WIPHY_FLAG_4ADDR_AP |
			WIPHY_FLAG_4ADDR_STATION |
			WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD |
			WIPHY_FLAG_REPORTS_OBSS;

#ifdef CONFIG_SWT6621S_TDLS
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;
	wiphy->flags |= WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
#endif

#ifdef CONFIG_SWT6621S_OFFCHAN_TX
	wiphy->flags |= WIPHY_FLAG_OFFCHAN_TX;
#else
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	wiphy->max_num_csa_counters = 2;
#endif

	/* STA SME EXTERNAL */
	if (!test_bit(SKW_FLAG_STA_SME_EXTERNAL, &skw->flags))
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;

	/* AP SME INTERNAL */
	if (!test_bit(SKW_FLAG_SAP_SME_EXTERNAL, &skw->flags)) {
		wiphy->max_acl_mac_addrs = SKW_MAX_ACL_ENTRIES;
		wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
		wiphy->ap_sme_capa = 1;
	}

	wiphy->features = NL80211_FEATURE_SK_TX_STATUS |
			  NL80211_FEATURE_SAE |
			  NL80211_FEATURE_HT_IBSS |
			  NL80211_FEATURE_VIF_TXPOWER |
			  NL80211_FEATURE_USERSPACE_MPM |
			  NL80211_FEATURE_FULL_AP_CLIENT_STATE |
			  NL80211_FEATURE_INACTIVITY_TIMER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	//wiphy->features |= NL80211_FEATURE_TDLS_CHANNEL_SWITCH;
	wiphy->features |= NL80211_FEATURE_MAC_ON_CREATE;
#endif

#ifdef CONFIG_SWT6621S_SCAN_RANDOM_MAC
	wiphy->features |= SKW_WIPHY_FEATURE_SCAN_RANDOM_MAC;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_RRM);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_VHT_IBSS);

	//TODO:Add an function to initialize iftype_ext_cap
	skw_setup_wiphy_iftype_ext_cap(wiphy);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
	wiphy->support_mbssid = true;
#else
	wiphy->bss_priv_size = sizeof(struct skw_bss_priv);
	set_bit(SKW_FLAG_MBSSID_PRIV, &skw->flags);
#endif

	wiphy->interface_modes = BIT(NL80211_IFTYPE_ADHOC) |
				 BIT(NL80211_IFTYPE_STATION) |
				 BIT(NL80211_IFTYPE_AP) |
				 BIT(NL80211_IFTYPE_P2P_GO) |
				 BIT(NL80211_IFTYPE_P2P_CLIENT) |
				 BIT(NL80211_IFTYPE_MONITOR);

#ifndef CONFIG_SWT6621S_LEGACY_P2P
	wiphy->interface_modes |= BIT(NL80211_IFTYPE_P2P_DEVICE);
#endif

	BUILD_BUG_ON_MSG(SKW_EXTENDED_CAPA_LEN > sizeof(skw->ext_capa),
			 "SKW_EXTENDED_CAPA_LEN larger than buffer");
	wiphy->extended_capabilities = skw->ext_capa;
	wiphy->extended_capabilities_mask = skw->ext_capa;
	wiphy->extended_capabilities_len = SKW_EXTENDED_CAPA_LEN;

#if defined(CONFIG_PM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	wiphy->wowlan = &skw_wowlan_support;
#else
	wiphy->wowlan = skw_wowlan_support;
#endif
#endif

	skw_sync_band_capa(skw, &skw_band_2ghz, chip);
	wiphy->bands[NL80211_BAND_2GHZ] = &skw_band_2ghz;

	skw_chip_info(skw->idx, "2g_only:%d", chip->priv_2g_only);
	if (!chip->priv_2g_only) {
		skw_sync_band_capa(skw, &skw_band_5ghz, chip);
		wiphy->bands[NL80211_BAND_5GHZ] = &skw_band_5ghz;

#ifdef CONFIG_SWT6621S_6GHZ
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		wiphy->bands[NL80211_BAND_6GHZ] = &skw_band_6ghz;
#endif
#endif
	}

	wiphy->cipher_suites = skw_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(skw_cipher_suites);

	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->max_scan_ssids = chip->max_scan_ssids;
	wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN; /*2304*/
	wiphy->max_remain_on_channel_duration = 500;
	wiphy->max_sched_scan_ie_len = IEEE80211_MAX_DATA_LEN;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	wiphy->max_sched_scan_reqs = 1;
#endif
	wiphy->max_sched_scan_ssids = 10;
	wiphy->max_match_sets = 16;

	/* MCC support */
	wiphy->iface_combinations = skw_iface_combos;
	wiphy->n_iface_combinations = ARRAY_SIZE(skw_iface_combos);

	wiphy->addresses = skw->address;
	wiphy->n_addresses = ARRAY_SIZE(skw->address);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	wiphy->max_ap_assoc_sta = skw->fw.max_num_sta;
#endif

	wiphy->reg_notifier = skw_regd_notifier;

#ifdef CONFIG_SWT6621S_REGD_SELF_MANAGED

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	wiphy->regulatory_flags |= REGULATORY_WIPHY_SELF_MANAGED;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
	wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
	set_bit(SKW_FLAG_PRIV_REGD, &skw->flags);

#endif

	return wiphy_register(wiphy);
}
