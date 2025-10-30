/******************************************************************************
 *
 * Copyright(c) 2020-2030  Seekwave Corporation.
 *
 *****************************************************************************/
#ifndef __SKW_SDIO_DEBUGFS_H__
#define __SKW_SDIO_DEBUGFS_H__

#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/scatterlist.h>
#include <generated/utsrelease.h>
#include "boot_config.h"
#include "skw_boot.h"

int skw_sdio_debugfs_init(void);
void skw_sdio_debugfs_deinit(void);
struct proc_dir_entry *skw_sdio_procfs_file(struct proc_dir_entry *parent,
				       const char *name, umode_t mode,
				       const void *proc_fops, void *data);
int skw_sdio_proc_init_ex(const char *name, umode_t mode,
				       const void *fops, void *data);
#endif