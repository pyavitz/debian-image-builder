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

#ifndef __SKW_CONFIG_H__
#define __SKW_CONFIG_H__

#include <linux/kernel.h>
#include <linux/etherdevice.h>

#define SKW_LINE_BUFF_LEN   128

enum SKW_LINE_TYPE {
        SKW_LINE_ERROR,
        SKW_LINE_COMMENT,
        SKW_LINE_CONFIG,
        SKW_LINE_ITEM
};

struct skwifi_cfg {
	char *name;
	int (*parser)(struct skwifi_cfg *cfg, char *key, char *data);
	void *priv;
};

struct skwifi_cfg_data {
	int offset;
	int size;
	const char *data;
};

#define SKW_CFG_FLAG_P2P_DEV                (0)
#define SKW_CFG_FLAG_STA_EXT                (1)
#define SKW_CFG_FLAG_SAP_EXT                (2)
#define SKW_CFG_FLAG_REPEATER               (3)
struct skw_cfg_global {
	unsigned long flags;
	u8 mac[ETH_ALEN];
	u8 dma_addr_align;
	u8 reorder_timeout;
};

#define SKW_CFG_INTF_FLAG_INVALID           0
#define SKW_CFG_INTF_FLAG_LEGACY            1
struct skw_cfg_interface {
	unsigned long flags;
	char name[IFNAMSIZ];
	u8 mac[ETH_ALEN];
	u8 iftype;
	u8 inst;
};

struct skw_cfg_intf {
	struct skw_cfg_interface interface[4];
};

#define SKW_CFG_REGD_SELF_MANAGED           0
#define SKW_CFG_REGD_DEFAULT_COUNTRY        1
#define SKW_CFG_REGD_IGNORE_USER_HINT       2
#define SKW_CFG_REGD_IGNORE_COUNTRY_IE      3
#define SKW_CFG_REGD_OUTDOOR                4
struct skw_cfg_regd {
	unsigned long flags;
	char country[2];
};

#define SKW_CFG_CALIB_APPEND_BUS_NAME       0
#define SKW_CFG_CALIB_APPEND_MODULE_ID      1
#define SKW_CFG_CALIB_PROJECT_NAME          2
struct skw_cfg_calib {
	unsigned long flags;
	char chip_alias_name[16];
	char project[16];
};


#define SKW_CFG_FIRMWARE_CCA_ENABLE        0
#define SKW_CFG_FIRMWARE_USE_4ADDR         1
struct skw_cfg_firmware {
	unsigned long flags;
	u8 beacon_timeout;
};

#define SKW_CFG_BAND_2GHZ                  0
#define SKW_CFG_BAND_5GHZ                  0
struct skw_cfg_band {
	unsigned long flags;
	u8 bw_2ghz;
	u8 bw_5ghz;
};

struct skw_cfg_roam {
};

enum SKW_MIB_TYPE {
	SKW_TYPE_INVALID,
	SKW_TYPE_S8,
	SKW_TYPE_U8,
	SKW_TYPE_S16,
	SKW_TYPE_U16,
	SKW_TYPE_S32,
	SKW_TYPE_U32,
	SKW_TYPE_BOOL,
	SKW_TYPE_STRING,
};

struct skw_mib_data {
	struct list_head list;
	long value;
	int id;
	enum SKW_MIB_TYPE type;
};

struct skw_cfg_mib {
       struct list_head init;
       struct list_head sta;
       struct list_head sap;
       struct list_head gc;
       struct list_head go;
};

struct skw_config {
	struct skw_cfg_global global;
	struct skw_cfg_intf intf;
	struct skw_cfg_calib calib;
	struct skw_cfg_regd regd;
	struct skw_cfg_firmware fw;
	struct skw_cfg_band band;
	struct skw_cfg_roam roam;
	struct skw_cfg_mib mib;
};

static inline bool skw_config_use_4addr(struct skw_config *cfg)
{
	return test_bit(SKW_CFG_FIRMWARE_USE_4ADDR, &cfg->fw.flags);
}

static inline bool skw_config_append_bus_name(struct skw_config *cfg)
{
	return test_bit(SKW_CFG_CALIB_APPEND_BUS_NAME, &cfg->calib.flags);
}

static inline bool skw_config_append_module_id(struct skw_config *cfg)
{
	return test_bit(SKW_CFG_CALIB_APPEND_MODULE_ID, &cfg->calib.flags);
}

void skw_load_config(struct device *dev, const char *name, struct skw_config *cfg);
void skw_config_set_mib(struct wiphy *wiphy, int inst, struct list_head *head);
void skw_config_init(struct skw_config *conf);
void skw_config_deinit(struct skw_config *conf);
#endif
