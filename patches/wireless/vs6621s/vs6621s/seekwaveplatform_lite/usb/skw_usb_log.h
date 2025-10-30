/******************************************************************************
 *
 * Copyright(c) 2020-2030  Seekwave Corporation.
 * DATE: 2022-07-18
 * MODIFY:
 * Author:junwei.jiang
 *
 *****************************************************************************/
#ifndef __SKW_USB_LOG_H__
#define __SKW_USB_LOG_H__

#define SKW_USB_ERROR    BIT(0)
#define SKW_USB_WARNING  BIT(1)
#define SKW_USB_INFO     BIT(2)
#define SKW_USB_DEBUG    BIT(3)

#define SKW_USB_CMD      BIT(16)
#define SKW_USB_EVENT    BIT(17)
#define SKW_USB_SCAN     BIT(18)
#define SKW_USB_TIMER    BIT(19)
#define SKW_USB_STATE    BIT(20)

#define SKW_USB_PORT0     BIT(21)
#define SKW_USB_PORT1     BIT(22)
#define SKW_USB_PORT2     BIT(23)
#define SKW_USB_PORT3     BIT(24)
#define SKW_USB_PORT4     BIT(25)
#define SKW_USB_PORT5     BIT(26)
#define SKW_USB_PORT6     BIT(27)
#define SKW_USB_PORT7     BIT(28)
#define SKW_USB_SAVELOG     BIT(29)
#define SKW_USB_DUMP     BIT(31)

unsigned long skw_usb_log_level(void);
void skw_usb_cp_log(int disable);
int skw_usb_cp_log_status(void);
void skw_get_port_statistic(char *buffer, int size);
void reboot_to_change_USB_speed_mode(char *mode);
void get_USB_speed_mode(char *mode);
void modem_notify_event(int event);
#define skw_usb_log(level, fmt, ...) \
	do { \
		if (skw_usb_log_level() & level) \
			pr_err(fmt,  ##__VA_ARGS__); \
	} while (0)

#define skw_usb_port_log(port_num, fmt, ...) \
	do { \
		if (skw_usb_log_level() &(SKW_USB_PORT0<<port_num)) \
			pr_err(fmt,  ##__VA_ARGS__); \
	} while (0)

#define skw_port_log(port_num,fmt, ...) \
	skw_usb_log((SKW_USB_PORT0<<port_num), "[PORT_LOG] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_usb_err(fmt, ...) \
	skw_usb_log(SKW_USB_ERROR, "[SKWUSB ERROR] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_usb_warn(fmt, ...) \
	skw_usb_log(SKW_USB_WARNING, "[SKWUSB WARN] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_usb_info(fmt, ...) \
	skw_usb_log(SKW_USB_INFO, "[SKWUSB INFO] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_usb_dbg(fmt, ...) \
	skw_usb_log(SKW_USB_DEBUG, "[SKWUSB DBG] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_usb_hex_dump(prefix, buf, len) \
	do { \
		if (skw_usb_log_level() & SKW_USB_DUMP) { \
			u8 str[32] = {0};  \
			snprintf(str, sizeof(str), "[SKWUSB DUMP] %s", prefix); \
			print_hex_dump(KERN_ERR, str, \
				DUMP_PREFIX_OFFSET, 16, 1, buf, len, true); \
		} \
	} while (0)
#if 0
#define skw_usb_port_log(port_num, fmt, ...) \
	do { \
		if (skw_usb_log_level() &(SKW_USB_PORT0<<port_num)) \
			pr_err("[PORT_LOG] %s:"fmt,__func__,  ##__VA_ARGS__); \
	} while (0)

#endif
void skw_usb_log_level_init(void);
#endif
