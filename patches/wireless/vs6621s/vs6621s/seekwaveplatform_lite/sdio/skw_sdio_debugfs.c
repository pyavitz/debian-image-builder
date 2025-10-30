/*****************************************************************************
 * Copyright(c) 2020-2030  Seekwave Corporation.
 * SEEKWAVE TECH LTD..CO
 *Seekwave Platform the sdio log debug fs
 *FILENAME:skw_sdio_debugfs.c
 *DATE:2022-04-11
 *MODIFY:
 *
 **************************************************************************/

#include "skw_sdio_debugfs.h"
#include "skw_sdio_log.h"
#include "skw_sdio.h"

static struct proc_dir_entry *skw_sdio_proc_root = NULL;

static int skw_sdio_proc_show(struct seq_file *seq, void *v)
{
#define SKW_BSP_CONFIG_INT(conf)                                          \
	do {                                                          \
		seq_printf(seq, "%s=%d\n", #conf, conf);              \
	} while (0)

#define SKW_BSP_CONFIG_BOOL(conf)                                         \
	do {                                                          \
		if (IS_ENABLED(conf))                                 \
			seq_printf(seq, "%s=y\n", #conf);             \
		else                                                  \
			seq_printf(seq, "# %s is not set\n", #conf);  \
	} while (0)

#define SKW_BSP_CONFIG_STRING(conf)                                       \
	do {                                                          \
		seq_printf(seq, "%s=\"%s\"\n", #conf, conf);          \
	} while (0)

	seq_puts(seq, "\n");
	seq_printf(seq, "Kernel Version:  \t%s\n",
			UTS_RELEASE);
	seq_puts(seq, "\n");

	SKW_BSP_CONFIG_BOOL(CONFIG_SEEKWAVE_BSP_DRIVERS);
	SKW_BSP_CONFIG_BOOL(CONFIG_SKW_SDIOHAL);
	SKW_BSP_CONFIG_BOOL(CONFIG_SEEKWAVE_PLD_RELEASE);

	seq_puts(seq, "\n");

	return 0;
}

static int skw_sdio_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_sdio_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_sdio_profile_proc_fops = {
	.proc_open = skw_sdio_proc_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};
#else
static const struct file_operations skw_sdio_profile_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_sdio_proc_open,
	.read = seq_read,
	.release = single_release,
};
#endif

struct proc_dir_entry *skw_sdio_procfs_file(struct proc_dir_entry *parent,
				       const char *name, umode_t mode,
				       const void *fops, void *data)
{
	struct proc_dir_entry *dentry = parent ? parent : skw_sdio_proc_root;

	if (!dentry)
		return NULL;

	return proc_create_data(name, mode, dentry, fops, data);
}

int skw_sdio_proc_init_ex(const char *name, umode_t mode,
				       const void *fops, void *data)
{
	if (!skw_sdio_proc_root)
		return -1;
	skw_sdio_procfs_file(skw_sdio_proc_root, name, mode, fops, NULL);
	return 0;
}

int skw_sdio_proc_init(void)
{
	skw_sdio_proc_root = proc_mkdir("skwsdio", NULL);
	if (!skw_sdio_proc_root){
		pr_err("creat proc skwsdio failed\n");
		return -ENOMEM;
	}

	skw_sdio_procfs_file(skw_sdio_proc_root,"profile", 0666, &skw_sdio_profile_proc_fops, NULL);

	return 0;
}

void skw_sdio_proc_deinit(void)
{
	if (!skw_sdio_proc_root)
		return;
	proc_remove(skw_sdio_proc_root);
}

int skw_sdio_debugfs_init(void)
{
	skw_sdio_dbg("%s :traced\n", __func__);
	skw_sdio_proc_init();
	skw_sdio_create_debug_files();
	return 0;
}

void skw_sdio_debugfs_deinit(void)
{
	skw_sdio_dbg("%s :traced\n", __func__);
	skw_sdio_proc_deinit();
}
