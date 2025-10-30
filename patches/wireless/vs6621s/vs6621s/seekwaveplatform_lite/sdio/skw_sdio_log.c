/**************************************************************************
 * Copyright(c) 2020-2030  Seekwave Corporation.
 * SEEKWAVE TECH LTD..CO
 *
 *Seekwave Platform the sdio log debug fs
 *FILENAME:skw_sdio_log.c
 *DATE:2022-04-11
 *MODIFY:
 *Author:Jones.Jiang
 **************************************************************************/
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "skw_sdio.h"
#include "skw_sdio_log.h"
#include "skw_sdio_debugfs.h"

extern char firmware_version[];
static unsigned long skw_sdio_dbg_level;

unsigned long skw_sdio_log_level(void)
{
	return skw_sdio_dbg_level;
}

static void skw_sdio_set_log_level(int level)
{
	unsigned long dbg_level;

	dbg_level = skw_sdio_log_level() & 0xffff0000;
	dbg_level |= ((level << 1) - 1);

	xchg(&skw_sdio_dbg_level, dbg_level);
}

static void skw_sdio_enable_func_log(int func, bool enable)
{
	unsigned long dbg_level = skw_sdio_log_level();

	if (enable)
		dbg_level |= func;
	else
		dbg_level &= (~func);

	xchg(&skw_sdio_dbg_level, dbg_level);
}

static int skw_sdio_log_show(struct seq_file *seq, void *data)
{
#define SKW_SDIO_LOG_STATUS(s) (level & (s) ? "enable" : "disable")

	int i;
	u32 level = skw_sdio_log_level();
	u8 *log_name[] = {"NONE", "ERROR", "WARNNING", "INFO", "DEBUG"};

	for (i = 0; i < 5; i++) {
		if (!(level & BIT(i)))
			break;
	}
	// if (i >= 5
	if (i < 5) {
		seq_printf(seq, "\nlog   level: %s\n", log_name[i]);
	} else {
	    seq_printf(seq, "\nlog   level: NONE\n");
	}
	seq_puts(seq, "\n");
	seq_printf(seq, "port0 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT0));
	seq_printf(seq, "port1 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT1));
	seq_printf(seq, "port2 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT2));
	seq_printf(seq, "port3 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT3));
	seq_printf(seq, "port4 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT4));
	seq_printf(seq, "port5 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT5));
	seq_printf(seq, "port6 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT6));
	seq_printf(seq, "port7 log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_PORT7));
	seq_printf(seq, "savelog  : %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_SAVELOG));
	seq_printf(seq, "dump  log: %s\n", SKW_SDIO_LOG_STATUS(SKW_SDIO_DUMP));

	return 0;
}

static int skw_sdio_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_sdio_log_show, inode->i_private);
}

static int skw_sdio_log_control(const char *cmd, bool enable)
{
	if (!strcmp("dump", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_DUMP, enable);
	else if (!strcmp("port0", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT0, enable);
	else if (!strcmp("port1", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT1, enable);
	else if (!strcmp("port2", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT2, enable);
	else if (!strcmp("port3", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT3, enable);
	else if (!strcmp("port4", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT4, enable);
	else if (!strcmp("port5", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT5, enable);
    else if (!strcmp("port6", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT6, enable);
	else if (!strcmp("port7", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_PORT7, enable);
    else if (!strcmp("savelog", cmd))
		skw_sdio_enable_func_log(SKW_SDIO_SAVELOG, enable);
	else if (!strcmp("debug", cmd))
		skw_sdio_set_log_level(SKW_SDIO_DEBUG);
	else if (!strcmp("info", cmd))
		skw_sdio_set_log_level(SKW_SDIO_INFO);
	else if (!strcmp("warn", cmd))
		skw_sdio_set_log_level(SKW_SDIO_WARNING);
	else if (!strcmp("error", cmd))
		skw_sdio_set_log_level(SKW_SDIO_ERROR);
	else
		return -EINVAL;

	return 0;
}

static ssize_t skw_sdio_log_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	int i, idx;
	char cmd[32];
	bool enable = false;

	for (idx = 0, i = 0; i < len; i++) {
		char c;

		if (get_user(c, buffer))
			return -EFAULT;

		switch (c) {
		case ' ':
			break;

		case ':':
			cmd[idx] = 0;
			if (!strcmp("enable", cmd))
				enable = true;
			else
				enable = false;

			idx = 0;
			break;

		case '|':
		case '\0':
		case '\n':
			cmd[idx] = 0;
			skw_sdio_log_control(cmd, enable);
			idx = 0;
			break;

		default:
			cmd[idx++] = c;
			idx %= 32;

			break;
		}

		buffer++;
	}

	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_sdio_log_proc_fops = {
	.proc_open = skw_sdio_log_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_sdio_log_write,
};
#else
static const struct file_operations skw_sdio_log_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_sdio_log_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_sdio_log_write,
};
#endif

static int skw_version_show(struct seq_file *seq, void *data)
{
	seq_printf(seq, "firmware info: %s\n", firmware_version );
	return 0;
}
static int skw_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_version_show, inode->i_private);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_version_proc_fops = {
	.proc_open = skw_version_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};
#else
static const struct file_operations skw_version_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_version_open,
	.read = seq_read,
	.release = single_release,
};
#endif

static int skw_port_statistic_show(struct seq_file *seq, void *data)
{
    char *statistic = kzalloc(2048, GFP_KERNEL);
    if (statistic == NULL) {
        printk(KERN_ERR "kzalloc statistic failed\n");
        return -ENOMEM;
    }
    skw_get_port_statistic(statistic, 2048);
	seq_printf(seq, "Statistic:\n %s\n", statistic);
	skw_get_assert_print_info(statistic, 2048);
	seq_printf(seq, "sdio last irqs information:\n%s", statistic);
	skw_get_sdio_debug_info(statistic, 2048);
	seq_printf(seq, "\nsdio debug information:\n%s", statistic);
	kfree(statistic);
        return 0;
}
static int skw_port_statistic_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_port_statistic_show, inode->i_private);
}


static int skw_config_show(struct seq_file *seq, void *data)
{
    char *config = kzalloc(2048, GFP_KERNEL);
    if (config == NULL) {
        printk(KERN_ERR "kzalloc statistic failed\n");
        return -ENOMEM;
    }

	skw_get_sdio_config(config, 2048);
	seq_printf(seq, "config:\n %s\n", config);
	kfree(config);
        return 0;
}

static int skw_config_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_config_show, inode->i_private);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_port_statistic_proc_fops = {
    .proc_open = skw_port_statistic_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};
#else
static const struct file_operations skw_port_statistic_proc_fops = {
    .owner = THIS_MODULE,
    .open = skw_port_statistic_open,
    .read = seq_read,
    .release = single_release,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_config_proc_fops = {
    .proc_open = skw_config_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};
#else
static const struct file_operations skw_config_proc_fops = {
    .owner = THIS_MODULE,
    .open = skw_config_open,
    .read = seq_read,
    .release = single_release,
};
#endif

static int skw_bluetooth_UART1_open(struct inode *inode, struct file *file)
{
        return single_open(file, NULL, inode->i_private);
}


static ssize_t skw_bluetooth_UART1_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	char cmd[32]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("enable", cmd, 6)) {
		memset(cmd, 0, sizeof(cmd));
		reboot_to_change_bt_uart1(cmd);
		printk("%s UART-HCI\n", cmd);
	}
	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_bluetooth_UART1_proc_fops = {
	.proc_open = skw_bluetooth_UART1_open,
	.proc_release = single_release,
	.proc_write = skw_bluetooth_UART1_write,
};
#else
static const struct file_operations skw_bluetooth_UART1_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_bluetooth_UART1_open,
	.release = single_release,
	.write = skw_bluetooth_UART1_write,
};
#endif

static int skw_bluetooth_antenna_show(struct seq_file *seq, void *data)
{
	char result[32];

	memset(result, 0, sizeof(result));
	get_bt_antenna_mode(result);
	if(strlen(result))
		seq_printf(seq, result);
        return 0;
}
static int skw_bluetooth_antenna_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_bluetooth_antenna_show, inode->i_private);
}


static ssize_t skw_bluetooth_antenna_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	char cmd[32]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("switch", cmd, 6)) {
		memset(cmd, 0, sizeof(cmd));
		reboot_to_change_bt_antenna_mode(cmd);
		printk("%s\n", cmd);
	}
	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_bluetooth_antenna_proc_fops = {
	.proc_open = skw_bluetooth_antenna_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_bluetooth_antenna_write,
};
#else
static const struct file_operations skw_bluetooth_antenna_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_bluetooth_antenna_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_bluetooth_antenna_write,
};
#endif

static int skw_recovery_debug_show(struct seq_file *seq, void *data)
{
	if (skw_sdio_recovery_debug_status())
		seq_printf(seq, "Disabled");
	else
		seq_printf(seq, "Enabled");
        return 0;
}
static int skw_recovery_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_recovery_debug_show, inode->i_private);
}


static ssize_t skw_recovery_debug_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	char cmd[16]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("disable", cmd, 7))
		skw_sdio_recovery_debug(1);
	else if (!strncmp("enable", cmd, 6))
		skw_sdio_recovery_debug(0);
	else if (!strncmp("logon", cmd, 5))
		skw_sdio_cp_log_disable(0);
	else if (!strncmp("logoff", cmd, 6))
		skw_sdio_cp_log_disable(1);
	else if (!strncmp("dump", cmd, 4))
		skw_sdio_cp_dumpmem();
	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_recovery_debug_proc_fops = {
	.proc_open = skw_recovery_debug_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_recovery_debug_write,
};
#else
static const struct file_operations skw_recovery_debug_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_recovery_debug_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_recovery_debug_write,
};
#endif

static int skw_wifi_serv_debug_show(struct seq_file *seq, void *data)
{
	if (skw_sdio_wifi_serv_debug_status())
		seq_printf(seq, "START");
	else
		seq_printf(seq, "STOP");
        return 0;
}
static int skw_wifi_serv_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_wifi_serv_debug_show, inode->i_private);
}


static ssize_t skw_wifi_serv_debug_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	char cmd[16]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("start", cmd, 5))
		skw_sdio_wifi_serv_debug(1);
	else if (!strncmp("stop", cmd, 4))
		skw_sdio_wifi_serv_debug(0);

	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_sdio_wifi_serv_proc_fops = {
	.proc_open = skw_wifi_serv_debug_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_wifi_serv_debug_write,
};
#else
static const struct file_operations skw_sdio_wifi_serv_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_wifi_serv_debug_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_wifi_serv_debug_write,
};
#endif

static int skw_bt_serv_debug_show(struct seq_file *seq, void *data)
{
	if (skw_sdio_bt_serv_debug_status())
		seq_printf(seq, "START");
	else
		seq_printf(seq, "STOP");
        return 0;
}
static int skw_bt_serv_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, &skw_bt_serv_debug_show, inode->i_private);
}


static ssize_t skw_bt_serv_debug_write(struct file *fp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	char cmd[16]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("start", cmd, 5))
		skw_sdio_bt_serv_debug(1);
	else if (!strncmp("stop", cmd, 4))
		skw_sdio_bt_serv_debug(0);

	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_sdio_bt_serv_proc_fops = {
	.proc_open = skw_bt_serv_debug_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_bt_serv_debug_write,
};
#else
static const struct file_operations skw_sdio_bt_serv_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_bt_serv_debug_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_bt_serv_debug_write,
};
#endif

static int skw_sdio_wifi_show(struct seq_file *seq, void *data)
{
	if (skw_sdio_wifi_status())
		seq_printf(seq, "PowerOn");
	else
		seq_printf(seq, "PowerOff");
	return 0;
}
static int skw_sdio_wifi_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_sdio_wifi_show, inode->i_private);
}


static ssize_t skw_sdio_wifi_poweron(struct file *fp, const char __user *buffer,
                                  size_t len, loff_t *offset)
{
          char cmd[16]={0};

          if (len >= sizeof(cmd))
                  return -EINVAL;
          if (copy_from_user(cmd, buffer, len))
                  return -EFAULT;
          if (!strncmp("on", cmd, 2))
                  skw_sdio_wifi_power_on(1);
          else if (!strncmp("off", cmd, 3))
                skw_sdio_wifi_power_on(0);

          return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_sdio_wifi_proc_fops = {
	.proc_open = skw_sdio_wifi_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_sdio_wifi_poweron,
};
#else
static const struct file_operations skw_sdio_wifi_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_sdio_wifi_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_sdio_wifi_poweron,
};
#endif

void skw_sdio_log_level_init(void)
{
	skw_sdio_set_log_level(SKW_SDIO_INFO);

	skw_sdio_enable_func_log(SKW_SDIO_DUMP, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT0, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT1, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT2, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT3, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT4, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT5, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT6, false);
	skw_sdio_enable_func_log(SKW_SDIO_SAVELOG, false);
	skw_sdio_enable_func_log(SKW_SDIO_PORT7, false);
}
void skw_sdio_create_debug_files(void) 
{
	skw_sdio_proc_init_ex("log_level", 0666, &skw_sdio_log_proc_fops, NULL);
	skw_sdio_proc_init_ex("Version", 0666, &skw_version_proc_fops, NULL);
	skw_sdio_proc_init_ex("Statistic", 0666, &skw_port_statistic_proc_fops, NULL);
	skw_sdio_proc_init_ex("config", 0666, &skw_config_proc_fops, NULL);
	skw_sdio_proc_init_ex("debug", 0666, &skw_recovery_debug_proc_fops, NULL);
	skw_sdio_proc_init_ex("BT_ANT", 0666, &skw_bluetooth_antenna_proc_fops, NULL);
	skw_sdio_proc_init_ex("BT_UART1", 0666, &skw_bluetooth_UART1_proc_fops, NULL);
	skw_sdio_proc_init_ex("WiFi", 0666, &skw_sdio_wifi_proc_fops, NULL);
	skw_sdio_proc_init_ex("wifi_service", 0666, &skw_sdio_wifi_serv_proc_fops, NULL);
	skw_sdio_proc_init_ex("bt_service", 0666, &skw_sdio_bt_serv_proc_fops, NULL);
}
