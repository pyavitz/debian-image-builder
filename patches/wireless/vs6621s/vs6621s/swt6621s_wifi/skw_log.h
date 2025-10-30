/* SPDX-License-Identifier: GPL-2.0 */

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

#ifndef __SKW_LOG_H__
#define __SKW_LOG_H__

#define SKW_ERROR              BIT(0)
#define SKW_WARN               BIT(1)
#define SKW_INFO               BIT(2)
#define SKW_DEBUG              BIT(3)
#define SKW_DETAIL             BIT(4)

#define SKW_CMD                BIT(16)
#define SKW_EVENT              BIT(17)
#define SKW_SCAN               BIT(18)
#define SKW_TIMER              BIT(19)
#define SKW_STATE              BIT(20)
#define SKW_WORK               BIT(21)
#define SKW_DFS                BIT(22)

#define SKW_DUMP               BIT(31)

#define SKW_LOG_TAG            "SKWIFI6621S"
#define SKW_TAG_NAME(name)     SKW_LOG_TAG " " #name

#define SKW_TAG_ERROR          SKW_TAG_NAME(ERROR)
#define SKW_TAG_WARN           SKW_TAG_NAME(WARN)
#define SKW_TAG_INFO           SKW_TAG_NAME(INFO)
#define SKW_TAG_DEBUG          SKW_TAG_NAME(DBG)
#define SKW_TAG_DETAIL         SKW_TAG_NAME(DETAIL)

#define SKW_TAG_CMD            SKW_TAG_NAME(CMD)
#define SKW_TAG_DATA           SKW_TAG_NAME(DATA)
#define SKW_TAG_EVENT          SKW_TAG_NAME(EVENT)
#define SKW_TAG_SCAN           SKW_TAG_NAME(SCAN)
#define SKW_TAG_TIMER          SKW_TAG_NAME(TIMER)
#define SKW_TAG_STATE          SKW_TAG_NAME(STATE)
#define SKW_TAG_WORK           SKW_TAG_NAME(WORK)
#define SKW_TAG_DUMP           SKW_TAG_NAME(DUMP)
#define SKW_TAG_LOCAL          SKW_TAG_NAME(LOCAL)

unsigned long skw_log_level(void);


#define skw_data_path_log(fmt, ...) \
	do { \
		if ((skw_log_level() & SKW_DEBUG)) \
			printk_ratelimited("[%s] %s: "fmt, \
				SKW_TAG_DATA, __func__, ##__VA_ARGS__); \
	} while (0)

#define skw_log(level, fmt, ...) \
	do { \
		if (skw_log_level() & level) \
			pr_err(fmt,  ##__VA_ARGS__); \
	} while (0)

#define skw_err(fmt, ...) \
	skw_log(SKW_ERROR, "[%s] %s: "fmt, SKW_TAG_ERROR, __func__, ##__VA_ARGS__)

#define skw_warn(fmt, ...) \
	skw_log(SKW_WARN, "[%s] %s: "fmt, SKW_TAG_WARN, __func__, ##__VA_ARGS__)

#define skw_info(fmt, ...) \
	skw_log(SKW_INFO, "[%s] %s: "fmt, SKW_TAG_INFO, __func__, ##__VA_ARGS__)

#define skw_dbg(fmt, ...) \
	skw_log(SKW_DEBUG, "[%s] %s: "fmt, SKW_TAG_DEBUG, __func__, ##__VA_ARGS__)

#define skw_detail(fmt, ...) \
	skw_log(SKW_DETAIL, "[%s] %s: "fmt, SKW_TAG_DETAIL, __func__, ##__VA_ARGS__)

//=========================print chip idx start================================
#define skw_chip_data_path_log(chip, fmt, ...) \
	do { \
		if ((skw_log_level() & SKW_DEBUG)) \
			printk_ratelimited("[chip%d][%s] %s: "fmt, \
				, chip, SKW_TAG_DATA, __func__, ##__VA_ARGS__); \
	} while (0)

#define skw_chip_log(chip, level, fmt, ...) \
			do { \
				if (skw_log_level() & level) \
					pr_err("[chip%d]"fmt, chip, ##__VA_ARGS__); \
			} while (0)

#define skw_chip_err(chip,fmt, ...) \
	skw_chip_log(chip, SKW_ERROR, "[%s] %s: "fmt, SKW_TAG_ERROR, __func__, ##__VA_ARGS__)

#define skw_chip_warn(chip,fmt, ...) \
	skw_chip_log(chip, SKW_WARN, "[%s] %s: "fmt, SKW_TAG_WARN, __func__, ##__VA_ARGS__)

#define skw_chip_info(chip,fmt, ...) \
	skw_chip_log(chip, SKW_INFO, "[%s] %s: "fmt, SKW_TAG_INFO, __func__, ##__VA_ARGS__)

#define skw_chip_dbg(chip,fmt, ...) \
	skw_chip_log(chip, SKW_DEBUG, "[%s] %s: "fmt, SKW_TAG_DEBUG, __func__, ##__VA_ARGS__)

#define skw_chip_detail(chip,fmt, ...) \
	skw_chip_log(chip, SKW_DETAIL, "[%s] %s: "fmt, SKW_TAG_DETAIL, __func__, ##__VA_ARGS__)
//=========================print chip idx end================================


#define skw_hex_dump(prefix, buf, len, force)                                    \
	do {                                                                     \
		if ((skw_log_level() & SKW_DUMP) || force) {                     \
			print_hex_dump(KERN_ERR, "["SKW_TAG_DUMP"] "prefix" - ", \
				DUMP_PREFIX_OFFSET, 16, 1, buf, len, true);      \
		}                                                                \
	} while (0)

void skw_dump_frame(u8 *frame, u16 frame_len);
void skw_del_dbg_iface(void);

void skw_log_level_init(void);
void skw_log_level_deinit(void);
#endif
