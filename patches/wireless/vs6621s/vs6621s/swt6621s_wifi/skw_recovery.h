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

#ifndef __SKW_RECOVERY_H__
#define __SKW_RECOVERY_H__

#define SKW_RECOVERY_TIMEOUT                    (msecs_to_jiffies(5000))

struct skw_recovery_ifdata {
	void *param;
	int size;
	u32 peer_map;
};

int skw_recovery_data_update(struct skw_iface *iface, void *param, int len);
void skw_recovery_data_clear(struct skw_iface *iface);
int skw_recovery_init(struct skw_core *skw);
void skw_recovery_deinit(struct skw_core *skw);

#endif
