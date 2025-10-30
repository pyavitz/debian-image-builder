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

#ifndef __SKW_WORK_H__
#define __SKW_WORK_H__

struct skw_timer {
	struct list_head list;
	unsigned long timeout;
	void (*cb)(void *data);
	void *id;
	void *data;
	const char *name;
};

int skw_add_timer_work(struct skw_core *skw, const char *name,
		       void (*cb)(void *dat), void *data,
		       unsigned long timeout, void *timer_id, gfp_t flags);
void skw_del_timer_work(struct skw_core *skw, void *timer_id);
void skw_timer_init(struct skw_core *skw);
void skw_timer_deinit(struct skw_core *skw);
#endif
