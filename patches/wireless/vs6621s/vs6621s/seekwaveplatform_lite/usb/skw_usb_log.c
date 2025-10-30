/*****************************************************************************
 * Copyright(c) 2020-2030  Seekwave Corporation.
 * SEEKWAVE TECH LTD..CO
 *
 *Seekwave Platform the usb log debug fs
 *FILENAME:skw_usb_log.c
 *DATE:2022-04-11
 *MODIFY:
 *Author:Jones.Jiang
 **************************************************************************/
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include "skw_usb_log.h"
#include "skw_usb.h"
#include "skw_usb_debugfs.h"

static unsigned long skw_usb_dbg_level;

extern char firmware_version[];
extern void reboot_to_change_bt_antenna_mode(char *mode);
extern void get_bt_antenna_mode(char *mode);
unsigned long skw_usb_log_level(void)
{
	return skw_usb_dbg_level;
}

static void skw_usb_set_log_level(int level)
{
	unsigned long dbg_level;

	dbg_level = skw_usb_log_level() & 0xffff0000;
	dbg_level |= ((level << 1) - 1);

	xchg(&skw_usb_dbg_level, dbg_level);
}

static void skw_usb_enable_func_log(int func, bool enable)
{
	unsigned long dbg_level = skw_usb_log_level();

	if (enable)
		dbg_level |= func;
	else
		dbg_level &= (~func);

	xchg(&skw_usb_dbg_level, dbg_level);
}

static int skw_usb_log_show(struct seq_file *seq, void *data)
{
#define SKW_USB_LOG_STATUS(s) (level & (s) ? "enable" : "disable")

	int i;
	u32 level = skw_usb_log_level();
	u8 *log_name[] = {"NONE", "ERROR", "WARNNING", "INFO", "DEBUG"};

	for (i = 0; i < 5; i++) {
		if (!(level & BIT(i)))
			break;
	}
	if (i >= 5)
		return 0;
	seq_printf(seq, "\nlog   level: %s\n", log_name[i]);

	seq_puts(seq, "\n");
	seq_printf(seq, "port0 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT0));
	seq_printf(seq, "port1 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT1));
	seq_printf(seq, "port2 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT2));
	seq_printf(seq, "port3 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT3));
	seq_printf(seq, "port4 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT4));
	seq_printf(seq, "port5 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT5));
	seq_printf(seq, "port6 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT6));
	seq_printf(seq, "port7 log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_PORT7));
	seq_printf(seq, "savelog  : %s\n", SKW_USB_LOG_STATUS(SKW_USB_SAVELOG));
	seq_printf(seq, "dump  log: %s\n", SKW_USB_LOG_STATUS(SKW_USB_DUMP));

	return 0;
}

static int skw_usb_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_usb_log_show, inode->i_private);
}

static int skw_usb_log_control(const char *cmd, bool enable)
{
	if (!strcmp("dump", cmd))
		skw_usb_enable_func_log(SKW_USB_DUMP, enable);
	else if (!strcmp("port0", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT0, enable);
	else if (!strcmp("port1", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT1, enable);
	else if (!strcmp("port2", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT2, enable);
	else if (!strcmp("port3", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT3, enable);
	else if (!strcmp("port4", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT4, enable);
	else if (!strcmp("port5", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT5, enable);
    else if (!strcmp("port6", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT6, enable);
	else if (!strcmp("port7", cmd))
		skw_usb_enable_func_log(SKW_USB_PORT7, enable);
    else if (!strcmp("savelog", cmd))
		skw_usb_enable_func_log(SKW_USB_SAVELOG, enable);
	else if (!strcmp("debug", cmd))
		skw_usb_set_log_level(SKW_USB_DEBUG);
	else if (!strcmp("info", cmd))
		skw_usb_set_log_level(SKW_USB_INFO);
	else if (!strcmp("warn", cmd))
		skw_usb_set_log_level(SKW_USB_WARNING);
	else if (!strcmp("error", cmd))
		skw_usb_set_log_level(SKW_USB_ERROR);
	else
		return -EINVAL;

	return 0;
}

static ssize_t skw_usb_log_write(struct file *fp, const char __user *buffer,
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
			skw_usb_log_control(cmd, enable);
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
static const struct proc_ops skw_usb_log_proc_fops = {
	.proc_open = skw_usb_log_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_usb_log_write,
};
#else
static const struct file_operations skw_usb_log_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_usb_log_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_usb_log_write,
};
#endif

static int skw_version_show(struct seq_file *seq, void *data)
{
	seq_printf(seq, "firmware info:\n %s\n", firmware_version );
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

	skw_get_port_statistic(statistic, 2048);
	seq_printf(seq, "Statistic:\n%s", statistic );
	kfree(statistic);
	return 0;
}

static int skw_port_statistic_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_port_statistic_show, inode->i_private);
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
		skw_usb_info("%s\n", cmd);
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

static int skw_USB_speed_show(struct seq_file *seq, void *data)
{
	char result[32];

	memset(result, 0, sizeof(result));
	get_USB_speed_mode(result);
	if(strlen(result))
		seq_printf(seq, result);
	return 0;
}
static int skw_USB_speed_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skw_USB_speed_show, inode->i_private);
}


static ssize_t skw_USB_speed_write(struct file *fp, const char __user *buffer,
                                size_t len, loff_t *offset)
{
	char cmd[32]={0};

	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (!strncmp("HIGH", cmd, 4)) {
		memset(cmd, 0, sizeof(cmd));
		reboot_to_change_USB_speed_mode(cmd);
		skw_usb_info("%s\n", cmd);
	}
	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_USB_speed_proc_fops = {
	.proc_open = skw_USB_speed_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_USB_speed_write,
};
#else
static const struct file_operations skw_USB_speed_proc_fops = {
	.owner = THIS_MODULE,
	.open = skw_USB_speed_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_USB_speed_write,
};
#endif

static int skwusb_recovery_debug_show(struct seq_file *seq, void *data)
{
    if (skw_usb_recovery_debug_status())
        seq_printf(seq, "Disabled");
    else
        seq_printf(seq, "Enabled");

    return 0;
}
static int skwusb_recovery_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, &skwusb_recovery_debug_show, inode->i_private);
}

static ssize_t skwusb_recovery_debug_write(struct file *fp, const char __user *buffer,
                size_t len, loff_t *offset)
{
    char cmd[16]={0};

    if (len >= sizeof(cmd))
        return -EINVAL;
    if (copy_from_user(cmd, buffer, len))
        return -EFAULT;
    if (!strncmp("disable", cmd, 7))
        skw_usb_recovery_debug(1);
    else if (!strncmp("enable", cmd, 6))
        skw_usb_recovery_debug(0);

    return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skwusb_recovery_debug_proc_fops = {
    .proc_open = skwusb_recovery_debug_open,
    .proc_read = seq_read,
    .proc_release = single_release,
    .proc_write = skwusb_recovery_debug_write,
};
#else
static const struct file_operations skwusb_recovery_debug_proc_fops = {
	.owner = THIS_MODULE,
    .open = skwusb_recovery_debug_open,
    .read = seq_read,
    .release = single_release,
    .write = skwusb_recovery_debug_write,
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
		skw_usb_info("%s UART-HCI\n", cmd);
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

void skw_usb_log_level_init(void)
{
	skw_usb_set_log_level(SKW_USB_INFO);

	skw_usb_enable_func_log(SKW_USB_DUMP, false);
	skw_usb_enable_func_log(SKW_USB_PORT0, false);
	skw_usb_enable_func_log(SKW_USB_PORT1, false);
	skw_usb_enable_func_log(SKW_USB_PORT2, false);
	skw_usb_enable_func_log(SKW_USB_PORT3, false);
	skw_usb_enable_func_log(SKW_USB_PORT4, false);
	skw_usb_enable_func_log(SKW_USB_PORT5, false);
	skw_usb_enable_func_log(SKW_USB_PORT6, false);
	skw_usb_enable_func_log(SKW_USB_SAVELOG, false);
	skw_usb_enable_func_log(SKW_USB_PORT7, false);
	skw_usb_proc_init_ex("log_level", 0666, &skw_usb_log_proc_fops, NULL);
	skw_usb_proc_init_ex("Version", 0666, &skw_version_proc_fops, NULL);
	skw_usb_proc_init_ex("Statistic", 0666, &skw_port_statistic_proc_fops, NULL);
	skw_usb_proc_init_ex("BT_ANT", 0666, &skw_bluetooth_antenna_proc_fops, NULL);
	skw_usb_proc_init_ex("recovery", 0666, &skwusb_recovery_debug_proc_fops, NULL);
	skw_usb_proc_init_ex("USB_SPEED", 0666, &skw_USB_speed_proc_fops, NULL);
	skw_usb_proc_init_ex("BT_UART1", 0666, &skw_bluetooth_UART1_proc_fops, NULL);
}
