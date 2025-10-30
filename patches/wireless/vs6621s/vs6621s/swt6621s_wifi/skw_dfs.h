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

#ifndef __SKW_DFS_H__
#define __SKW_DFS_H__

#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>

#define SKW_PRI_TOLERANCE                     16

#define SKW_DFS_FLAG_CAC_MODE                 1
#define SKW_DFS_FLAG_MONITOR_MODE             2

struct skw_pri_detector {
	u64 last_ts;
	u32 window_size;
	u32 count, max_count;

	struct list_head sequences;
	struct list_head pulses;
};

struct skw_radar_rule {
	u8 type_id;
	u8 width_min;
	u8 width_max;
	u16 pri_min;
	u16 pri_max;
	u8 nr_pri;
	u8 ppb;
	u8 ppb_thresh;
	u8 max_pri_tolerance;
	bool chirp;
};

struct skw_radar_cfg {
	const struct skw_radar_rule rule;
	struct skw_pri_detector pri;
};

struct skw_radar_info {
	int nr_cfg;
	struct skw_radar_cfg *cfgs;
};

enum SKW_DFS_ACTION {
	SKW_DFS_START_CAC = 1,
	SKW_DFS_STOP_CAC,
	SKW_DFS_START_MONITOR,
	SKW_DFS_STOP_MONITOR,
};

struct skw_cac_params {
	u8 chn;
	u8 center_chn1;
	u8 center_chn2;
	u8 band_width;

	u32 time_ms;
	u8 region;
} __packed;

struct skw_dfs_cac {
	u16 type;
	u16 len;
	struct skw_cac_params params;
};

struct skw_pulse_data {
	u64 chirp: 1;
	u64 rssi: 5;
	u64 width: 8;
	u64 ts: 24;
	u64 resv:26;
};

struct skw_radar_pulse {
	u8 nr_pulse;
	u8 resv;
	struct skw_pulse_data data[0];
} __packed;

struct skw_pulse_info {
	u64 ts;
	u16 freq;
	u8 width;
	s8 rssi;
	bool chirp;
};

struct skw_pulse_elem {
	struct list_head head;
	u64 ts;
};

struct skw_pri_sequence {
	struct list_head head;
	u32 pri;
	u32 dur;
	u32 count;
	u32 count_falses;
	u64 first_ts;
	u64 last_ts;
	u64 deadline_ts;
};

#ifdef CONFIG_SWT6621S_DFS_MASTER
int skw_dfs_chan_init(struct wiphy *wiphy, struct net_device *dev,
		      struct cfg80211_chan_def *chandef, u32 cac_time_ms);

int skw_dfs_add_pulse(struct wiphy *wiphy, struct net_device *dev,
			struct skw_pulse_info *pulse);

int skw_dfs_start_cac(struct wiphy *wiphy, struct net_device *dev);
int skw_dfs_stop_cac(struct wiphy *wiphy, struct net_device *ndev);
int skw_dfs_start_monitor(struct wiphy *wiphy, struct net_device *dev);
int skw_dfs_stop_monitor(struct wiphy *wiphy, struct net_device *dev);
int skw_dfs_init(struct wiphy *wiphy, struct net_device *dev);
int skw_dfs_deinit(struct wiphy *wiphy, struct net_device *dev);
#else
static inline int skw_dfs_chan_init(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_chan_def *chandef, u32 cac_time_ms)
{
	return -ENOTSUPP;
}

static inline int skw_dfs_add_pulse(struct wiphy *wiphy, struct net_device *dev,
				struct skw_pulse_info *pulse)
{
	return 0;
}

static inline int skw_dfs_start_cac(struct wiphy *wiphy, struct net_device *dev)
{
	return -ENOTSUPP;
}

static inline int skw_dfs_stop_cac(struct wiphy *wiphy, struct net_device *ndev)
{
	return 0;
}

static inline int skw_dfs_start_monitor(struct wiphy *wiphy, struct net_device *dev)
{
	return -ENOTSUPP;
}

static inline int skw_dfs_stop_monitor(struct wiphy *wiphy, struct net_device *dev)
{
	return 0;
}
static inline int skw_dfs_init(struct wiphy *wiphy, struct net_device *dev)
{
	return 0;
}

static inline int skw_dfs_deinit(struct wiphy *wiphy, struct net_device *dev)
{
	return 0;
}
#endif

#endif
