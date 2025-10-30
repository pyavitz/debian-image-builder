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

#include <linux/skbuff.h>
#include <net/cfg80211.h>

#include "skw_core.h"
#include "skw_timer.h"
#include "skw_msg.h"
#include "skw_mlme.h"

static int skw_timer_show(struct seq_file *seq, void *data)
{
	struct skw_timer *timer;
	struct skw_core *skw = seq->private;

	seq_printf(seq, "count: %d\n", skw->timer_data.count);

	if (!skw->timer_data.count)
		return 0;

	spin_lock_bh(&skw->timer_data.lock);
	list_for_each_entry(timer, &skw->timer_data.list, list) {
		seq_puts(seq, "\n");

		seq_printf(seq, "name: %s\n"
				"id: 0x%p\n"
				"time left: %u ms\n",
				timer->name,
				timer->id,
				jiffies_to_msecs(timer->timeout - jiffies));
	}

	spin_unlock_bh(&skw->timer_data.lock);

	return 0;
}

static int skw_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_timer_show, inode->i_private);
}

static const struct file_operations skw_timer_fops = {
	.owner = THIS_MODULE,
	.open = skw_timer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
static void skw_timer_work(struct timer_list *data)
#else
static void skw_timer_work(unsigned long data)
#endif
{
	struct skw_core *skw;
	struct skw_timer *timer, *next;
	LIST_HEAD(timeout_list);

	skw = container_of((void *)data, struct skw_core, timer_data.timer);

	spin_lock_bh(&skw->timer_data.lock);
	list_for_each_entry_safe(timer, next, &skw->timer_data.list, list) {
		if (time_before(jiffies, timer->timeout))
			break;

		list_move(&timer->list, &timeout_list);
	}

	spin_unlock_bh(&skw->timer_data.lock);

	while (!list_empty(&timeout_list)) {
		timer = list_first_entry(&timeout_list, struct skw_timer, list);
		skw_chip_log(skw->idx, SKW_TIMER, "[%s] %s: %s(id: 0x%p)\n",
			SKW_TAG_TIMER, __func__, timer->name, timer->id);
		list_del(&timer->list);

		timer->cb(timer->data);
		skw->timer_data.count--;

		SKW_KFREE(timer);
	}

	spin_lock_bh(&skw->timer_data.lock);
	timer = list_first_entry_or_null(&skw->timer_data.list, struct skw_timer, list);
	if (timer)
		mod_timer(&skw->timer_data.timer, timer->timeout);

	spin_unlock_bh(&skw->timer_data.lock);
}

static bool skw_timer_id_exist(struct skw_core *skw, void *id)
{
	bool result = false;
	struct skw_timer *timer;

	spin_lock_bh(&skw->timer_data.lock);

	list_for_each_entry(timer, &skw->timer_data.list, list) {
		if (id == timer->id) {
			result = true;
			break;
		}
	}

	spin_unlock_bh(&skw->timer_data.lock);

	return result;
}

int skw_add_timer_work(struct skw_core *skw, const char *name,
		       void (*cb)(void *dat), void *data,
		       unsigned long timeout, void *timer_id, gfp_t flags)
{
	struct skw_timer *timer, *node;
	struct list_head *head;

	if (!timer_id || !cb || !name)
		return -EINVAL;

	skw_chip_log(skw->idx, SKW_TIMER, "[%s] %s: %s(id: 0x%p), time out = %ld\n",
		SKW_TAG_TIMER, __func__, name, timer_id, timeout);

	if (skw_timer_id_exist(skw, timer_id)) {
		skw_warn("id: 0x%p exist\n", timer_id);
		SKW_BUG_ON(1);

		return -EINVAL;
	}

	timer = SKW_ZALLOC(sizeof(*timer), flags);
	if (!timer)
		return -ENOMEM;

	INIT_LIST_HEAD(&timer->list);

	timer->name = name;
	timer->cb = cb;
	timer->data = data;
	timer->id = timer_id;
	timer->timeout = msecs_to_jiffies(timeout) + jiffies + 1;

	spin_lock_bh(&skw->timer_data.lock);
	head = &skw->timer_data.list;

	list_for_each_entry(node, &skw->timer_data.list, list) {
		if (time_before_eq(timer->timeout, node->timeout)) {
			head = &node->list;
			break;
		}
	}

	list_add(&timer->list, head);

	skw->timer_data.count++;
	node = list_first_entry(&skw->timer_data.list, struct skw_timer, list);

	mod_timer(&skw->timer_data.timer, node->timeout);
	spin_unlock_bh(&skw->timer_data.lock);

	return 0;
}

void skw_del_timer_work(struct skw_core *skw, void *timer_id)
{
	struct skw_timer *timer;

	skw_chip_log(skw->idx, SKW_TIMER, "[%s] %s: id: 0x%p\n",
		SKW_TAG_TIMER, __func__, timer_id);

	spin_lock_bh(&skw->timer_data.lock);
	list_for_each_entry(timer, &skw->timer_data.list, list) {
		if (timer->id == timer_id) {
			list_del(&timer->list);
			skw->timer_data.count--;
			SKW_KFREE(timer);
			break;
		}
	}

	timer = list_first_entry_or_null(&skw->timer_data.list, struct skw_timer, list);
	if (timer)
		mod_timer(&skw->timer_data.timer, timer->timeout);

	spin_unlock_bh(&skw->timer_data.lock);
}

void skw_timer_init(struct skw_core *skw)
{
	// skw->timer_work.timeout = LONG_MAX;
	skw->timer_data.count = 0;

	INIT_LIST_HEAD(&skw->timer_data.list);
	spin_lock_init(&skw->timer_data.lock);

	// fixme:
	// timer_setup(&skw->timer_data.timer, skw->timer_data.timer_work, 0);
	skw_compat_setup_timer(&skw->timer_data.timer, skw_timer_work);

	skw_debugfs_file(skw->dentry, "timer", 04444, &skw_timer_fops, skw);
}

void skw_timer_deinit(struct skw_core *skw)
{
	LIST_HEAD(flush_list);

	del_timer(&skw->timer_data.timer);

	spin_lock_bh(&skw->timer_data.lock);
	list_replace_init(&skw->timer_data.list, &flush_list);
	spin_unlock_bh(&skw->timer_data.lock);

	while (!list_empty(&flush_list)) {
		struct skw_timer *timer = list_first_entry(&flush_list,
			struct skw_timer, list);

		list_del(&timer->list);

		skw_chip_log(skw->idx, SKW_TIMER, "[%s] %s: name: %s\n",
			SKW_TAG_TIMER, __func__, timer->name);

		SKW_KFREE(timer);
	}
}
