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

#include <generated/utsrelease.h>
#include "skw_core.h"
#include "skw_dentry.h"
#include "skw_compat.h"
#include "version.h"

static struct dentry *skw_debugfs_root;
static struct proc_dir_entry *skw_proc_root;

static int skw_proc_show(struct seq_file *seq, void *v)
{
#define SKW_CONFIG_INT(conf)    seq_printf(seq, "%s=%d\n", #conf, conf)
#define SKW_CONFIG_STRING(conf) seq_printf(seq, "%s=\"%s\"\n", #conf, conf)

#define SKW_CONFIG_BOOL(conf)                                         \
	do {                                                          \
		if (IS_ENABLED(conf))                                 \
			seq_printf(seq, "%s=y\n", #conf);             \
		else                                                  \
			seq_printf(seq, "# %s is not set\n", #conf);  \
	} while (0)


	seq_puts(seq, "\n");
	seq_printf(seq, "Kernel Version:  \t%s\n"
			"Wi-Fi Driver:    \t%s\n"
			"Wi-Fi Branch:    \t%s\n",
			UTS_RELEASE,
			SKW_VERSION,
			SKW_BRANCH);

	seq_puts(seq, "\n");

	SKW_CONFIG_BOOL(CONFIG_SWT6621S_STA_SME_EXT);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_SAP_SME_EXT);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_SCAN_RANDOM_MAC);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LEGACY_P2P);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_TX_WORKQUEUE);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_HIGH_PRIORITY);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_VENDOR);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_REGD_SELF_MANAGED);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_TDLS);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_DFS_MASTER);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_REPEATER_MODE);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_EDMA);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_OFFCHAN_TX);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_CALIB_DPD);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_CALIB_APPEND_BUS_ID);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_6GHZ);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_USB3_WORKAROUND);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LOG_ERROR);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LOG_WARN);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LOG_INFO);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LOG_DEBUG);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_LOG_DETAIL);
	SKW_CONFIG_BOOL(CONFIG_SWT6621S_SKB_RECYCLE);

#ifdef CONFIG_SWT6621S_RX_REORDER_TIMEOUT
	SKW_CONFIG_INT(CONFIG_SWT6621S_RX_REORDER_TIMEOUT);
#endif

#ifdef CONFIG_SWT6621S_PROJECT_NAME
	SKW_CONFIG_STRING(CONFIG_SWT6621S_PROJECT_NAME);
#endif

#ifdef CONFIG_SWT6621S_DEFAULT_COUNTRY
	SKW_CONFIG_STRING(CONFIG_SWT6621S_DEFAULT_COUNTRY);
#endif

#ifdef CONFIG_SWT6621S_CHIP_ID
	SKW_CONFIG_STRING(CONFIG_SWT6621S_CHIP_ID);
#endif

	seq_puts(seq, "\n");

	return 0;
}

static int skw_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_proc_fops = {
	.proc_open = skw_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations skw_proc_fops = {
	.open = skw_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

struct dentry *skw_debugfs_subdir(const char *name, struct dentry *parent)
{
	struct dentry *de, *pentry;

	pentry = parent ? parent : skw_debugfs_root;
	if (!pentry)
		return NULL;

	de = debugfs_create_dir(name, pentry);

	return IS_ERR(de) ? NULL : de;
}

struct dentry *skw_debugfs_file(struct dentry *parent,
				const char *name, umode_t mode,
				const struct file_operations *fops, void *data)
{
	struct dentry *de, *pentry;

	pentry = parent ? parent : skw_debugfs_root;
	if (!pentry)
		return NULL;

	de = debugfs_create_file(name, mode, pentry, data, fops);

	return IS_ERR(de) ? NULL : de;
}

struct proc_dir_entry *skw_procfs_subdir(const char *name,
				struct proc_dir_entry *parent)
{
	struct proc_dir_entry *dentry = parent ? parent : skw_proc_root;

	if (!dentry)
		return NULL;

	return proc_mkdir_data(name, 0, dentry, NULL);
}

struct proc_dir_entry *skw_procfs_file(struct proc_dir_entry *parent,
				       const char *name, umode_t mode,
				       const void *fops, void *data)
{
	struct proc_dir_entry *dentry = parent ? parent : skw_proc_root;

	if (!dentry)
		return NULL;

	return proc_create_data(name, mode, dentry, fops, data);
}

int skw_dentry_init(void)
{
	skw_proc_root = proc_mkdir("skwifid", NULL);
	if (!skw_proc_root)
		pr_err("creat proc skwifid failed\n");

	skw_procfs_file(skw_proc_root, "profile", 0, &skw_proc_fops, NULL);

	skw_debugfs_root = debugfs_create_dir("skwifid", NULL);
	if (IS_ERR(skw_debugfs_root))
		skw_debugfs_root = NULL;

	return 0;
}

void skw_dentry_deinit(void)
{
	debugfs_remove_recursive(skw_debugfs_root);
	proc_remove(skw_proc_root);
}
