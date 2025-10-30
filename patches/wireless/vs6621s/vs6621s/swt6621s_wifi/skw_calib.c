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

#include <linux/crc32c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "skw_core.h"
#include "skw_msg.h"
#include "skw_log.h"
#include "skw_cfg80211.h"
#include "skw_util.h"
#include "skw_calib.h"

static int skw_dpd_chn_to_index(u16 chn, u8 bw)
{
	int index = 0;

	switch (chn) {
	case 1 ... 5:
		/* these ch cp use ch3 instead */
		if (bw == 0)
			index = 2;
		else
			index = 67;
		break;

	case 6 ... 9:
		/* these ch cp use ch7 instead */
		if (bw == 0)
			index = 6;
		else
			index = 68;
		break;

	case 10 ... 14:
		/* these ch cp use ch11 instead */
		if (bw == 0)
			index = 10;
		else
			index = 69;
		break;

	case 36 ... 48:
		index = ((chn - 36) >> 1) + 14;
		break;

	case 52 ... 64:
		index = ((chn - 52) >> 1) + 21;
		break;

	case 100 ... 144:
		index = ((chn - 100) >> 1) + 28;
		break;

	case 149 ... 177:
		index = ((chn - 149) >> 1) + 51;
		break;

	default:
		index = -1;
		break;
	}

	return index;
}

int skw_dpd_set_coeff_params(struct wiphy *wiphy,
	struct net_device *ndev, u8 chn, u8 center_chan,
	u8 center_chan2, u8 bandwidth)
{

	int ret = 0;
	int index;
	struct skw_core *skw = NULL;
	struct skw_rf_rxdpd_data *para;
	struct skw_rf_rxdpd_param param;

	skw_dbg("chan: %d, center_chan: %d, center_chan2: %d, band width: %d\n",
		chn, center_chan, center_chan2, bandwidth);

	skw = wiphy_priv(wiphy);
	if (!skw) {
		skw_err("skw->dpd skw null");
		return -EINVAL;
	}

	para = skw->dpd.resource;
	if (!para) {
		skw_chip_err(skw->idx, "skw->dpd.resource null");
		return -EINVAL;
	}

	param.size = 2 * sizeof(struct skw_rf_rxdpd_train);

	index = skw_dpd_chn_to_index(center_chan, bandwidth);
	skw_chip_dbg(skw->idx, "ch_idx:%d\n", index);
	if (index < 0 || index > DPD_CHAN_CNT - 1) {
		skw_chip_err(skw->idx, "chn %d not found\n", center_chan);
		return -ERANGE;
	}

	memcpy(&param.train[0], &para->data[index][0],
		sizeof(struct skw_rf_rxdpd_train) * 2);

	if (center_chan <= 14) {
		param.train[0].chan = center_chan;
		param.train[1].chan = center_chan;
	}

	skw_hex_dump("dpdresultcmd", &param,
			sizeof(struct skw_rf_rxdpd_param), false);

	ret = skw_send_msg(wiphy, ndev, SKW_CMD_SET_DPD_RESULT,
		&param, sizeof(param), NULL, 0);
	if (ret)
		skw_chip_err(skw->idx, "Send dpd result failed, ret: %d", ret);

	return ret;
}

int skw_dpd_result_handler(struct skw_core *skw, void *buf, int len)
{
	int index;
	struct skw_rf_rxdpd_train *res = buf;
	struct skw_rf_rxdpd_data *para;

	if (res->done)
		skw_chip_dbg(skw->idx, "ch:%d rf:%d bw:%d done:%d\n", res->chan,
			res->rf_idx, res->cbw, res->done);

	if (res->rf_idx > DPD_RF_CNT - 1) {
		skw_chip_err(skw->idx, "invalid rf_idx: %d\n", res->rf_idx);

		skw_hw_assert(skw, false);
		return -ERANGE;
	}

	index = skw_dpd_chn_to_index(res->chan, res->cbw);
	if (index < 0 || index > DPD_CHAN_CNT - 1) {
		skw_chip_err(skw->idx, "chn %d not found\n", res->chan);
		return -ERANGE;
	}

	para = skw->dpd.resource;
	if (!para) {
		skw_chip_err(skw->idx, "skw->dpd.resource null");
		return -EINVAL;
	}

	if (para->data[index][res->rf_idx].chan ||
		para->data[index][res->rf_idx].cbw) {
		skw_hex_dump("havedata", &para->data[index][res->rf_idx],
				sizeof(struct skw_rf_rxdpd_train), true);
		skw_chip_err(skw->idx, "ch:%d rf:%d cbw:%d index:%d had data\n",
			res->chan, res->rf_idx, res->cbw, index);
		skw_hw_assert(skw, false);
		return -EBUSY;
	}

	memcpy(&para->data[index][res->rf_idx], res,
		sizeof(struct skw_rf_rxdpd_train));

	skw_hex_dump("data", &para->data[index][res->rf_idx],
			sizeof(struct skw_rf_rxdpd_train), false);
	return 0;
}

int skw_dpd_init(struct skw_dpd *dpd)
{
	struct skw_core *skw = container_of(dpd, struct skw_core, dpd);

	dpd->size = sizeof(struct skw_rf_rxdpd_data);

	dpd->resource = SKW_ZALLOC(dpd->size, GFP_KERNEL);
	if (!dpd->resource) {
		skw_chip_err(skw->idx, "malloc dpd resource failed, size: %d\n",
			dpd->size);
		return -ENOMEM;
	}

	return 0;
}

void skw_dpd_zero(struct skw_dpd *dpd)
{
	memset(dpd->resource, 0, dpd->size);
}

void skw_dpd_deinit(struct skw_dpd *dpd)
{
	SKW_KFREE(dpd->resource);
}
