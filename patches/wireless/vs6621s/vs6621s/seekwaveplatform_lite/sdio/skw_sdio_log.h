/******************************************************************************
 *
 * Copyright(c) 2020-2030  Seekwave Corporation.
 *
 *****************************************************************************/
#ifndef __SKW_SDIO_LOG_H__
#define __SKW_SDIO_LOG_H__

#define SKW_SDIO_ERROR    BIT(0)
#define SKW_SDIO_WARNING  BIT(1)
#define SKW_SDIO_INFO     BIT(2)
#define SKW_SDIO_DEBUG    BIT(3)

#define SKW_SDIO_CMD      BIT(16)
#define SKW_SDIO_EVENT    BIT(17)
#define SKW_SDIO_SCAN     BIT(18)
#define SKW_SDIO_TIMER    BIT(19)
#define SKW_SDIO_STATE    BIT(20)

#define SKW_SDIO_PORT0     BIT(21)
#define SKW_SDIO_PORT1     BIT(22)
#define SKW_SDIO_PORT2     BIT(23)
#define SKW_SDIO_PORT3     BIT(24)
#define SKW_SDIO_PORT4     BIT(25)
#define SKW_SDIO_PORT5     BIT(26)
#define SKW_SDIO_PORT6     BIT(27)
#define SKW_SDIO_PORT7     BIT(28)
#define SKW_SDIO_SAVELOG     BIT(29)
#define SKW_SDIO_DUMP     BIT(31)

unsigned long skw_sdio_log_level(void);

#define skw_sdio_log(level, fmt, ...) \
	do { \
		if (skw_sdio_log_level() & level) \
			pr_err(fmt,  ##__VA_ARGS__); \
	} while (0)

#define skw_sdio_port_log(port_num, fmt, ...) \
	do { \
		if (skw_sdio_log_level() &(SKW_SDIO_PORT0<<port_num)) \
			pr_err(fmt,  ##__VA_ARGS__); \
	} while (0)

#define skw_port_log(port_num,fmt, ...) \
	skw_sdio_log((SKW_SDIO_PORT0<<port_num), "[PORT_LOG] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_sdio_err(fmt, ...) \
	skw_sdio_log(SKW_SDIO_ERROR, "[SKWSDIO ERROR] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_sdio_warn(fmt, ...) \
	skw_sdio_log(SKW_SDIO_WARNING, "[SKWSDIO WARN] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_sdio_info(fmt, ...) \
	skw_sdio_log(SKW_SDIO_INFO, "[SKWSDIO INFO] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_sdio_dbg(fmt, ...) \
	skw_sdio_log(SKW_SDIO_DEBUG, "[SKWSDIO DBG] %s: "fmt, __func__, ##__VA_ARGS__)

#define skw_sdio_hex_dump(prefix, buf, len) \
	do { \
		if (skw_sdio_log_level() & SKW_SDIO_DUMP) { \
			u8 str[32] = {0};  \
			snprintf(str, sizeof(str), "[SKWSDIO DUMP] %s", prefix); \
			print_hex_dump(KERN_ERR, str, \
				DUMP_PREFIX_OFFSET, 16, 1, buf, len, true); \
		} \
	} while (0)
#if 0
#define skw_sdio_port_log(port_num, fmt, ...) \
	do { \
		if (skw_sdio_log_level() &(SKW_SDIO_PORT0<<port_num)) \
			pr_err("[PORT_LOG] %s:"fmt,__func__,  ##__VA_ARGS__); \
	} while (0)

#endif
void skw_sdio_log_level_init(void);
void skw_sdio_create_debug_files(void);
#endif

