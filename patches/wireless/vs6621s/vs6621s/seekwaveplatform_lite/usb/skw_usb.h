/*****************************************************************
 *Copyright (C) 2021 Seekwave Tech Inc.
 *Filename : skw_usb.h
 *Authors:seekwave platform
 *
 * This software is licensed under the terms of the the GNU
 * General Public License version 2, as published by the Free
 * Software Foundation, and may be copied, distributed, and
 * modified under those terms.
 *
 * This program is distributed in the hope that it will be usefull,
 * but without any warranty;without even the implied warranty of
 * merchantability or fitness for a partcular purpose. See the
 * GUN General Public License for more details.
 * **************************************************************/
#ifndef WCN_USB_H
#define WCN_USB_H

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include "../skwutil/skw_boot.h"

#define skwusb_log(fmt, args...) \
	pr_info("[SKW_USB]:" fmt, ## args)

#define skwusb_err(fmt, args...) \
	pr_err("[SKW_USB_ERR]:" fmt, ## args)

#define skwusb_data_pr(level, prefix_str, prefix_type, rowsize,\
		groupsize, buf, len, asscii)\
		do{if(loglevel) \
			print_hex_dump(level, prefix_str, prefix_type, rowsize,\
					groupsize, buf, len, asscii);\
		}while(0)


#define USB_RX_TASK_PRIO 90
#define SKW_CHIP_ID_LENGTH			16  //SV6160 chip id lenght

int skw_usb_recovery_debug(int disable);
int skw_usb_recovery_debug_status(void);
void reboot_to_change_bt_uart1(char *mode);
int skw_usb_debug_log_open(void);
int skw_usb_debug_log_close(void);

#endif /* WCN_USB_H */
