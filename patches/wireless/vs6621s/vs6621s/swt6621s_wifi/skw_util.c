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

#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/etherdevice.h>

#include "skw_core.h"
#include "skw_util.h"
#include "skw_cfg80211.h"

#ifdef CONFIG_PRINTK_TIME_FROM_ARM_ARCH_TIMER
#include <clocksource/arm_arch_timer.h>
u64 skw_local_clock(void)
{
	u64 ns;

	ns = arch_timer_read_counter() * 1000;
	do_div(ns, 24);

	return ns;
}
#else
u64 skw_local_clock(void)
{
	return local_clock();
}
#endif

#ifdef SKW_IMPORT_NS
struct file *skw_file_open(const char *path, int flags, int mode)
{
	struct file *fp = NULL;

	fp = filp_open(path, flags, mode);
	if (IS_ERR(fp)) {
		skw_err("open fail\n");
		return NULL;
	}

	return fp;
}

int skw_file_read(struct file *fp, unsigned char *data,
		size_t size, loff_t offset)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	return kernel_read(fp, data, size, &offset);
#else
	return kernel_read(fp, offset, data, size);
#endif
}

int skw_file_write(struct file *fp, unsigned char *data,
		size_t size, loff_t offset)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	return kernel_write(fp, data, size, &offset);
#else
	return kernel_write(fp, data, size, offset);
#endif
}

int skw_file_sync(struct file *fp)
{
	return vfs_fsync(fp, 0);
}

void skw_file_close(struct file *fp)
{
	filp_close(fp, NULL);
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

int skw_key_idx(unsigned long bitmap)
{
	static u8 idx[] = {0xff, 0x00, 0x01, 0xff,
			   0x02, 0xff, 0xff, 0xff,
			   0x03, 0xff, 0xff, 0xff,
			   0xff, 0xff, 0xff, 0xff};

	return idx[bitmap & 0xf];
}

char *skw_mgmt_name(u16 fc)
{
#define SKW_STYPE_STR(n) {case IEEE80211_STYPE_##n: return #n; }

	switch (fc) {
	SKW_STYPE_STR(ASSOC_REQ);
	SKW_STYPE_STR(ASSOC_RESP);
	SKW_STYPE_STR(REASSOC_REQ);
	SKW_STYPE_STR(REASSOC_RESP);
	SKW_STYPE_STR(PROBE_REQ);
	SKW_STYPE_STR(PROBE_RESP);
	SKW_STYPE_STR(BEACON);
	SKW_STYPE_STR(ATIM);
	SKW_STYPE_STR(DISASSOC);
	SKW_STYPE_STR(AUTH);
	SKW_STYPE_STR(DEAUTH);
	SKW_STYPE_STR(ACTION);

	default:
		break;
	}

#undef SKW_STYPE_STR

	return "UNDEFINE";
}

int skw_freq_to_chn(int freq)
{
	if (freq == 2484)
		return 14;
	else if (freq >= 2407 && freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq >= 5000 && freq <= 45000)
		return (freq - 5000) / 5;
	else if (freq >= 58320 && freq <= 64800)
		return (freq - 56160) / 2160;
	else
		return 0;
}

int skw_tlv_alloc(struct skw_tlv_conf *conf, int len, gfp_t gfp)
{
	int size;

	if (!conf)
		return -EINVAL;

	size = round_up(sizeof(struct skw_tlv) + sizeof(u16) + len, 8);

	conf->buff = SKW_ZALLOC(size, GFP_KERNEL);
	if (!conf->buff)
		return -ENOMEM;

	conf->total_len = 0;
	conf->buff_len = size;

	return 0;
}

void *skw_tlv_reserve(struct skw_tlv_conf *conf, int len)
{
	void *start = NULL;

	if (!conf || !conf->buff)
		return NULL;

	if (conf->total_len + len > conf->buff_len)
		return NULL;

	start = conf->buff + conf->total_len;
	conf->total_len += len;

	return start;
}

int skw_tlv_add(struct skw_tlv_conf *conf, int type, void *dat, int dat_len)
{
	struct skw_tlv *tlv;
	int len = sizeof(struct skw_tlv) + dat_len;

	tlv = skw_tlv_reserve(conf, len);
	if (!tlv) {
		skw_err("reserve len: %d failed, buff len: %d\n",
			len, conf->buff_len);

		return -ENOMEM;
	}

	tlv->type = type;
	tlv->len = dat_len;
	memcpy(tlv->value, dat, dat_len);

	return 0;
}

void skw_tlv_start(struct skw_tlv_conf *conf)
{
	conf->plen = skw_tlv_reserve(conf, sizeof(u16));
}

void skw_tlv_end(struct skw_tlv_conf *conf)
{
	if (conf->plen)
		*conf->plen = conf->total_len;
}

void skw_tlv_free(struct skw_tlv_conf *conf)
{
	if (conf) {
		SKW_KFREE(conf->buff);
		conf->total_len = 0;
		conf->buff_len = 0;
	}
}

int skw_set_mib(struct wiphy *wiphy, int inst, int mib, void *dat, int dat_len)
{
	int ret;
	struct skw_tlv_conf c;

	if (skw_tlv_alloc(&c, dat_len, GFP_KERNEL))
		return -ENOMEM;

	skw_tlv_start(&c);
	skw_tlv_add(&c, mib, dat, dat_len);
	skw_tlv_end(&c);

	ret = skw_msg_xmit(wiphy, inst, SKW_CMD_SET_MIB, c.buff,
			   c.total_len, NULL, 0);

	skw_tlv_free(&c);

	return ret;
}
