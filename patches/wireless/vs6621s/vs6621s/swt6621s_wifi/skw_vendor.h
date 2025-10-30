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

#ifndef __SKW_VENDOR_H__
#define __SKW_VENDOR_H__

#include <linux/version.h>

#define OUI_GOOGLE	                         0x001A11

/* Vendor Command ID */
#define SKW_VID_GET_VALID_CHANNELS               0x1009
#define SKW_VID_GET_FEATURE_SET                  0x100A
#define SKW_VID_SET_COUNTRY                      0x100E
#define SKW_VID_SET_LATENCY_MODE                 0x101B
#define SKW_VID_START_LOGGING                    0x1400
#define SKW_VID_GET_FIRMWARE_DUMP                0x1401
#define SKW_VID_GET_VERSION                      0x1403
#define SKW_VID_GET_RING_BUFFERS_STATUS          0x1404
#define SKW_VID_GET_RING_BUFFER_DATA             0x1405
#define SKW_VID_GET_LOGGER_FEATURE               0x1406
#define SKW_VID_GET_WAKE_REASON_STATS            0x140D
#define SKW_VID_GET_APF_CAPABILITIES             0x1800
#define SKW_VID_SELECT_TX_POWER_SCENARIO         0x1900
#define SKW_VID_GET_USABLE_CHANS                 0x2000

/* ATTR IE Start */

/* ATTR GET_VALID_CHANNELS */
#define SKW_ATTR_GET_VALID_CHANNELS              20
#define SKW_ATTR_VALID_CHANNELS_COUNT            36
#define SKW_ATTR_VALID_CHANNELS_LIST             37
#define SKW_GET_VALID_CHANNELS_RULES             38
/* ATTR GET_VALID_CHANNELS */

/* ATTR SET_COUNTRY */
#define SKW_ATTR_SET_COUNTRY                     5
#define SKW_SET_COUNTRY_RULES                    6
/* ATTR SET_COUNTRY */

/* ATTR GET_APF_CAPABILITIES */
#define SKW_ATTR_APF_VERSION                     0
#define SKW_ATTR_APF_MAX_LEN                     1
/* ATTR GET_APF_CAPABILITIES */

/* ATTR GET_VERSION */
#define SKW_ATTR_VERSION_DRIVER                  1
#define SKW_ATTR_VERSION_FIRMWARE                2
#define SKW_GET_VERSION_RULES                    3
/* ATTR GET_VERSION */

/* ATTR RING_BUFFERS_STATUS */
#define SKW_ATTR_RING_BUFFERS_STATUS             13
#define SKW_ATTR_RING_BUFFERS_COUNT              14
/* ATTR RING_BUFFERS_STATUS */

/* END OF ATTR IE */

/* Feature enums */
#define WIFI_FEATURE_INFRA                       0x1      // Basic infrastructure mode
#define WIFI_FEATURE_INFRA_5G                    0x2      // Support for 5 GHz Band
#define WIFI_FEATURE_HOTSPOT                     0x4      // Support for GAS/ANQP
#define WIFI_FEATURE_P2P                         0x8      // Wifi-Direct
#define WIFI_FEATURE_SOFT_AP                     0x10      // Soft AP
#define WIFI_FEATURE_GSCAN                       0x20      // Google-Scan APIs
#define WIFI_FEATURE_NAN                         0x40      // Neighbor Awareness Networking
#define WIFI_FEATURE_D2D_RTT                     0x80      // Device-to-device RTT
#define WIFI_FEATURE_D2AP_RTT                    0x100      // Device-to-AP RTT
#define WIFI_FEATURE_BATCH_SCAN                  0x200      // Batched Scan (legacy)
#define WIFI_FEATURE_PNO                         0x400      // Preferred network offload
#define WIFI_FEATURE_ADDITIONAL_STA              0x800      // Support for two STAs
#define WIFI_FEATURE_TDLS                        0x1000      // Tunnel directed link setup
#define WIFI_FEATURE_TDLS_OFFCHANNEL             0x2000      // Support for TDLS off channel
#define WIFI_FEATURE_EPR                         0x4000      // Enhanced power reporting
#define WIFI_FEATURE_AP_STA                      0x8000      // Support for AP STA Concurrency
#define WIFI_FEATURE_LINK_LAYER_STATS            0x10000     // Link layer stats collection
#define WIFI_FEATURE_LOGGER                      0x20000     // WiFi Logger
#define WIFI_FEATURE_HAL_EPNO                    0x40000     // WiFi PNO enhanced
#define WIFI_FEATURE_RSSI_MONITOR                0x80000     // RSSI Monitor
#define WIFI_FEATURE_MKEEP_ALIVE                 0x100000    // WiFi mkeep_alive
#define WIFI_FEATURE_CONFIG_NDO                  0x200000    // ND offload configure
#define WIFI_FEATURE_TX_TRANSMIT_POWER           0x400000    // Capture Tx transmit power levels
#define WIFI_FEATURE_CONTROL_ROAMING             0x800000    // Enable/Disable firmware roaming
#define WIFI_FEATURE_IE_WHITELIST                0x1000000   // Support Probe IE white listing
#define WIFI_FEATURE_SCAN_RAND                   0x2000000   // Support MAC & Probe Sequence Number randomization
#define WIFI_FEATURE_SET_TX_POWER_LIMIT          0x4000000   // Support Tx Power Limit setting
#define WIFI_FEATURE_USE_BODY_HEAD_SAR           0x8000000   // Support Using Body/Head Proximity for SAR
#define WIFI_FEATURE_DYNAMIC_SET_MAC             0x10000000  // Support changing MAC address without iface reset(down and up)
#define WIFI_FEATURE_SET_LATENCY_MODE            0x40000000  // Support Latency mode setting
#define WIFI_FEATURE_P2P_RAND_MAC                0x80000000  // Support P2P MAC randomization
#define WIFI_FEATURE_INFRA_60G                   0x100000000 // Support for 60GHz Band

/* Vendor Event ID */
#define SKW_NL80211_VENDOR_SUBCMD_MONITOR_RSSI   80
#define EXT_VENDOR_EVENT_BUF_SIZE                4096
enum skw_wlan_vendor_attr_rssi_monitoring {
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_INVALID = 0,
	/* Takes valid value from the enum
	 * skw_wlan_rssi_monitoring_control
	 * Unsigned 32-bit value enum skw_wlan_rssi_monitoring_control
	 */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL,
	/* Unsigned 32-bit value */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID,
	/* Signed 8-bit value in dBm */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI,
	/* Signed 8-bit value in dBm */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI,
	/* attributes to be used/received in callback */
	/* 6-byte MAC address used to represent current BSSID MAC address */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_BSSID,
	/* Signed 8-bit value indicating the current RSSI */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_RSSI,
	/* keep last */
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_AFTER_LAST,
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX =
	SKW_WLAN_VENDOR_ATTR_RSSI_MONITORING_AFTER_LAST - 1,
};

struct skw_ring_buff_status {
	u8 name[32];
	u32 flags;
	int ring_id;                  // unique integer representing the ring
	u32 ring_buffer_byte_size;    // total memory size allocated for the buffer
	u32 verbose_level;            // verbose level for ring buffer
	u32 written_bytes;            // number of bytes that was written to the buffer by driver
	u32 read_bytes;               // number of bytes that was read from the buffer by user land
	u32 written_records;          // number of records that was written to the buffer by driver
};

struct skw_usable_chan {
	u16 center_freq;
	u16 band_width;
	u16 iface_mode_mask;
	u16 flags;
	u32 resvd;
};

struct skw_usable_chan_req {
	u32 band_mask;
	u32 filter_mask;
	u32 iface_mode_mask;
	u32 flags;
	u32 resvd;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
void skw_vendor_init(struct wiphy *wiphy);
void skw_vendor_deinit(struct wiphy *wiphy);
#else
static inline void skw_vendor_init(struct wiphy *wiphy)
{
}

static inline void skw_vendor_deinit(struct wiphy *wiphy)
{
}
#endif

#endif
