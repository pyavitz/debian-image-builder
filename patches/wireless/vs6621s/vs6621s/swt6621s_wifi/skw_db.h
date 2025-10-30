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

#ifndef __SKW_DB_H__
#define __SKW_DB_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
#define REG_RULE_EXT(start, end, bw, gain, eirp, dfs_cac, reg_flags) \
	REG_RULE(start, end, bw, gain, eirp, reg_flags)
#endif

#define SKW_RRF_NO_OFDM                 BIT(0)
#define SKW_RRF_NO_OUTDOOR              BIT(3)
#define SKW_RRF_DFS                     BIT(4)
#define SKW_RRF_NO_IR                   BIT(7)
#define SKW_RRF_AUTO_BW                 BIT(11)

extern int skw_regdb_size;
extern const struct ieee80211_regdomain *skw_regdb[];

#endif
