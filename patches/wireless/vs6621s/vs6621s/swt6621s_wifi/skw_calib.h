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

#ifndef __SKW_CALIB_H__
#define __SKW_CALIB_H__

#include <linux/ieee80211.h>
#include <net/cfg80211.h>

#define DPD_CHAN_CNT               70
#define DPD_RF_CNT                 2

struct skw_dpd {
	void *resource;
	int size;
};

#ifdef CONFIG_SWT6621S_CALIB_DPD
struct skw_rf_rxdpd_coeff {
	u32 low_dpd_lut_coeff[4][12];
	u32 high_dpd_lut_coeff[4][16];
} __packed;

struct skw_rf_rxdpd_train {
	u16 chan;
	u8 done:1;
	u8 rf_idx:3;
	u8 cbw:4;
	u8 dpd_rf_gain_max;
	u8 delta_gain_q2[8];
	u32 dpd_tpc_info[8];
	struct skw_rf_rxdpd_coeff dpd_coeff;
} __packed;

struct skw_rf_rxdpd_data {
	struct skw_rf_rxdpd_train data[DPD_CHAN_CNT][DPD_RF_CNT];
} __packed;

struct skw_rf_rxdpd_param {
	u32 size;
	struct skw_rf_rxdpd_train train[2];
} __packed;

int skw_dpd_set_coeff_params(struct wiphy *wiphy, struct net_device *ndev,
			     u8 chn, u8 center_chan,
			     u8 center_chan2, u8 bandwidth);

int skw_dpd_result_handler(struct skw_core *skw, void *buf, int len);
int skw_dpd_init(struct skw_dpd *dpd);
void skw_dpd_deinit(struct skw_dpd *dpd);
void skw_dpd_zero(struct skw_dpd *dpd);
#else
static inline int skw_dpd_set_coeff_params(struct wiphy *wiphy,
			struct net_device *ndev, u8 chn, u8 center_chan,
			u8 center_two_chan2, u8 bandwidth)
{
	return 0;
}

static inline int skw_dpd_result_handler(struct skw_core *skw, void *buf, int len)
{
	return 0;
}

static inline int skw_dpd_init(struct skw_dpd *dpd)
{
	return 0;
}

static inline void skw_dpd_deinit(struct skw_dpd *dpd)
{
}

static inline void skw_dpd_zero(struct skw_dpd *dpd)
{
}

#endif
#endif
