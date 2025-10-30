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

#include <linux/ctype.h>
#include <linux/firmware.h>

#include "skw_util.h"
#include "skw_config.h"
#include "skw_log.h"
#include "skw_regd.h"

static struct skwifi_cfg *skw_cfg_match(struct skwifi_cfg *table, char *name)
{
	int i;
	struct skwifi_cfg *cfg = NULL;

	for (i = 0; table[i].name != NULL; i++) {
		if (!strcmp(name, table[i].name)) {
			cfg = &table[i];
			break;
		}
	}

	return cfg;
}

static long skw_bool_flag(unsigned long *flags, int nr, char *value)
{
	char *endp;
	long val = simple_strtol(value, &endp, 0);

	if (*endp != '\0')
		return -EINVAL;

	switch (val) {
	case 0:
		clear_bit(nr, flags);
		break;

	case 1:
		set_bit(nr, flags);
		break;

	default:
		break;
	}

	return val;
}

static void skw_get_mac_addr(char *buff, char *mac)
{
	long val;
	int i, j, idx;
	char chr, *endp;
	char dat[8], addr[6] = {0};
	int len = strlen(buff);

	for (i = 0, j = 0, idx = 0; i <= len; i++) {
		chr = buff[i];
		if (chr == ':')
			chr ='\0';

		if (j >= sizeof(dat))
			break;

		dat[j++] = chr;

		if (chr == '\0') {
			val = simple_strtol(dat, &endp, 16);
			if (val > 0xff)
				break;

			j = 0;
			addr[idx++] = val;
		}
	}

	if (idx == ETH_ALEN || is_valid_ether_addr(addr))
		skw_ether_copy(mac, addr);
}

static void skw_set_string(char *buff, int buff_sz, char *src)
{
	size_t len = strlen(src);

	if (len < buff_sz) {
		memcpy(buff, src, len);
		buff[len] = '\0';
	}
}

static int skw_read_line(struct skwifi_cfg_data *cfgd, char *buf, int buf_len)
{
	char chr;
	int mode = 0;
	int len = 0, total = 0, start = 0;
	bool do_save = false;

	memset(buf, 0x0, buf_len);
	start = cfgd->offset;

	while (cfgd->offset < cfgd->size) {
		chr = cfgd->data[cfgd->offset++];
		switch (chr) {
		case '\r':
		case '\n':
			chr = '\0';
			break;

		case '\t':
			chr = ' ';

			/* fall through */
			skw_fallthrough;
		case '#':
			mode = (mode == 0) ? chr : mode;
			break;

#if 0
		case '"':
			mode = (mode == '"') ? 0 : ((mode == 0) ? chr : mode);
			break;

		case '[':
			mode = (mode == 0) ? chr : mode;
			break;

		case ']':
			mode = (mode == '[') ? 0 : ((mode == 0) ? chr : mode);
			break;
#endif

		default:
			break;
		}

		do_save = false;

		switch (mode) {
		case '#':
			do_save = false;
			break;

#if 0
		case '"':
		case '[':
			do_save = true;
			break;
#endif

		default:
			if (chr != ' ')
				do_save = true;

			if (chr == ';')
				chr = '\0';

			break;
		}

		if (do_save) {
			if (len < buf_len)
				buf[len++] = chr;

			total++;
		}

		if (chr == '\0')
			break;
	}

	if (total > len)
		buf[0] = '\0';

	return cfgd->offset - start;
}

static int skw_line_parser(char *line, char *sec, char *key, char *value)
{
        char *chr;
        int len = strlen(line);

        if (!len)
                return SKW_LINE_COMMENT;

        /* config */
        if (line[0] == '[' && line[len - 1] == ']') {

                len = len - 2;
                if (len < 1)
                        return SKW_LINE_ERROR;

                memcpy(sec, &line[1], len);
                sec[len] = '\0';

                return SKW_LINE_CONFIG;
        }

        /* item */
        chr = strchr(line, '=');
        if (chr) {
                int key_len = chr - &line[0];
                int val_len = len - key_len;

                memcpy(key, &line[0], key_len);
                key[key_len] = '\0';

                memcpy(value, &chr[1], val_len);
                value[val_len] = '\0';

                return SKW_LINE_ITEM;
        }

        return SKW_LINE_ERROR;
}

static void skw_parser(struct skwifi_cfg *table, const u8 *data, int size)
{
        int len;
	char line[SKW_LINE_BUFF_LEN];
	char config[SKW_LINE_BUFF_LEN];
	char key[SKW_LINE_BUFF_LEN];
	char value[SKW_LINE_BUFF_LEN];
	struct skwifi_cfg_data cfgd;
	struct skwifi_cfg *cfg = NULL;

	cfgd.offset = 0;
	cfgd.data = data;
	cfgd.size = size;

	len = sizeof(line);

	while (skw_read_line(&cfgd, line, len)) {

		switch(skw_line_parser(line, config, key, value)) {
		case SKW_LINE_COMMENT:
			break;

		case SKW_LINE_CONFIG:
			cfg = skw_cfg_match(table, config);
			break;

		case SKW_LINE_ITEM:
			if (!strlen(key) || !strlen(value))
				break;

			if (cfg && cfg->priv)
				cfg->parser(cfg, key, value);

			break;

		case SKW_LINE_ERROR:
			break;

		default:
			break;
		}
	}
}

static int skw_global_parser(struct skwifi_cfg *config, char *key, char *value)
{
	char *endp;
	struct skw_cfg_global *cfg = config->priv;

	if (!strcmp(key, "mac_addr")) {
		skw_get_mac_addr(value, cfg->mac);
	} else if (!strcmp(key, "dma_addr_align")) {
		cfg->dma_addr_align = simple_strtol(value, &endp, 0);
	} else if (!strcmp(key, "reorder_timeout")) {
		cfg->reorder_timeout = simple_strtol(value, &endp, 0);
	} else {
		skw_dbg("unsupport key: %s\n", key);
	}

	return 0;
}

static int skw_firmware_parser(struct skwifi_cfg *config, char *key, char *value)
{
	char *endp;
	struct skw_cfg_firmware *fw = config->priv;

	if (!strcmp(key, "beacon_timeout")) {
		fw->beacon_timeout = simple_strtol(value, &endp, 0);
	} else if (!strcmp(key, "cca_enable")) {
		skw_bool_flag(&fw->flags, SKW_CFG_FIRMWARE_CCA_ENABLE, value);
	} else if (!strcmp(key, "use_4addr")) {
		skw_bool_flag(&fw->flags, SKW_CFG_FIRMWARE_USE_4ADDR, value);
	} else {
		skw_dbg("unsupport key: %s\n", key);
	}

	return 0;
}

static int skw_config_intf(struct skw_cfg_intf *intf, char *value)
{
	int i, len;
	char *chr, buff[SKW_LINE_BUFF_LEN];
	int idx = 0, inst = 0;
	bool exit = false;
	char *endp, *pos = value;
	struct skw_cfg_interface *ifa = &intf->interface[0];

	/* <inst>,<mode>,<interface name>,<mac addr> */
	for (i = 0; i < 4 && *pos != '\0'; i++, pos = chr + 1) {
		chr = strchr(pos, ',');
		if (!chr) {
			exit = true;
			len = strlen(pos);
		} else {
			len = chr - pos;
		}

		if (!len) {
			idx++;
			continue;
		}

		memset(buff, 0x0, sizeof(buff));
		memcpy(buff, pos, len);

		switch (idx++) {
		case 0:
			inst = simple_strtol(buff, &endp, 0);
			if (inst >= SKW_NR_IFACE) {
				exit = true;
				break;
			}

			ifa = &intf->interface[inst];
			break;

		case 1:
			if (!strcmp(buff, "sta"))
				ifa->iftype = NL80211_IFTYPE_STATION;
			else if (!strcmp(buff, "sap"))
				ifa->iftype = NL80211_IFTYPE_AP;
			else if (!strcmp(buff, "go"))
				ifa->iftype = NL80211_IFTYPE_P2P_GO;
			else if (!strcmp(buff, "gc"))
				ifa->iftype = NL80211_IFTYPE_P2P_CLIENT;
			else if (!strcmp(buff, "p2p_dev"))
				ifa->iftype = NL80211_IFTYPE_P2P_DEVICE;
			else
				skw_dbg("invalid mode: %s\n", buff);

			break;

		case 2:
			if (strlen(buff) > IFNAMSIZ) {
				skw_dbg("invalid name: %s\n", buff);
				break;
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
            strncpy(ifa->name, buff, IFNAMSIZ - 1);
            ifa->name[IFNAMSIZ - 1] = '\0';
#else
			strlcpy(ifa->name, buff, IFNAMSIZ);
#endif

            break;

		case 3:
			skw_get_mac_addr(buff, ifa->mac);
			break;
		}

		if (exit)
			break;
	}

	skw_detail("inst: %d, mode: %d, name: %s, mac: %pM\n",
		inst, ifa->iftype, ifa->name, ifa->mac);

	return 0;
}

static int skw_intf_parser(struct skwifi_cfg *config, char *key, char *value)
{
	struct skw_cfg_intf *intf = config->priv;

	if (!strcmp(key, "intf")) {
		skw_config_intf(intf, value);
	} else {
		skw_dbg("unsupport key: %s\n", key);
	}

	return 0;
}

static int skw_calib_parser(struct skwifi_cfg *config, char *key, char *value)
{
	struct skw_cfg_calib *calib = config->priv;

	if (!strcmp(key, "append_bus_name")) {
		skw_bool_flag(&calib->flags, SKW_CFG_CALIB_APPEND_BUS_NAME, value);
	} else if (!strcmp(key, "append_module_id")) {
		skw_bool_flag(&calib->flags, SKW_CFG_CALIB_APPEND_MODULE_ID, value);
	} else if (!strcmp(key, "chip_alias_name")) {
		skw_set_string(calib->chip_alias_name, sizeof(calib->chip_alias_name), value);
	} else if (!strcmp(key, "project_name")) {
		skw_set_string(calib->project, sizeof(calib->project), value);
	} else {
		skw_dbg("unsupport key: %s\n", key);
	}

	return 0;
}

static int skw_regdom_parser(struct skwifi_cfg *config, char *key, char *value)
{
	struct skw_cfg_regd *regd = config->priv;

	if (!strcmp(key, "self_managed")) {
		skw_bool_flag(&regd->flags, SKW_CFG_REGD_SELF_MANAGED, value);
	} else if (!strcmp(key, "country")) {
		if (strlen(value) == 2) {
			memcpy(regd->country, value, 2);
			set_bit(SKW_CFG_REGD_DEFAULT_COUNTRY, &regd->flags);
		}
	} else if (!strcmp(key, "ignore_user_hint")) {
		skw_bool_flag(&regd->flags, SKW_CFG_REGD_IGNORE_USER_HINT, value);
	} else if (!strcmp(key, "ignore_country_ie")) {
		skw_bool_flag(&regd->flags, SKW_CFG_REGD_IGNORE_COUNTRY_IE, value);
	} else if (!strcmp(key, "outdoor")) {
		skw_bool_flag(&regd->flags, SKW_CFG_REGD_OUTDOOR, value);
	} else {
		skw_dbg("unsupport key: %s\n", key);
	}

	return 0;
}

static int skw_roam_parser(struct skwifi_cfg *config, char *key, char *value)
{
	return 0;
}

static int skw_band_parser(struct skwifi_cfg *config, char *key, char *value)
{
	return 0;
}

static int skw_mib_parser(struct skwifi_cfg *config, char *key, char *value)
{
	long id, val;
	struct list_head *head = NULL;
	char *at = NULL, *endp = NULL;
	struct skw_mib_data *md = NULL;
	struct skw_cfg_mib *mib = config->priv;
	enum SKW_MIB_TYPE type = SKW_TYPE_INVALID;

	at = strchr(key, '@');
	if (!at)
		return -EINVAL;

	id = simple_strtol(at + 1, &endp, 0);
	if (*endp != '\0')
		return -EINVAL;

#define SKW_IS_START_WITH(s,ref) (!memcmp(s, ref, strlen(ref)))
	if (SKW_IS_START_WITH(key, "init@"))
		head = &mib->init;
	else if (SKW_IS_START_WITH(key, "sta@"))
		head = &mib->sta;
	else if (SKW_IS_START_WITH(key, "sap@"))
		head = &mib->sap;
	else if (SKW_IS_START_WITH(key, "gc@"))
		head = &mib->gc;
	else if (SKW_IS_START_WITH(key, "go@"))
		head = &mib->go;
	else
		head = NULL;
#undef SKW_IS_START_WITH

	if (head == NULL)
		return -EINVAL;

#define SKW_IS_STRING(s,r) (strlen(s) == strlen(r) && !memcmp(s, r, strlen(r)))
	type = SKW_TYPE_INVALID;

	endp = strrchr(value, '/');
	if (!endp)
		return -EINVAL;

	if (SKW_IS_STRING(endp + 1, "string")) {
		val = (long)kmemdup_nul(value, endp - value, GFP_KERNEL);
		if (!val) {
			skw_dbg("value: %s\n", (char *)val);
			return -ENOMEM;
		}

		type = SKW_TYPE_STRING;
	} else {
		val = simple_strtol(value, &endp, 0);
		if (*endp != '/')
			return -EINVAL;

		if (SKW_IS_STRING(endp + 1, "s8")) {
			if (val >= -127 && val <= 128)
				type = SKW_TYPE_S8;
		} else if (SKW_IS_STRING(endp + 1, "s16")) {
			if (val >= -32767 && val <= 32768)
				type = SKW_TYPE_S16;
		} else if (SKW_IS_STRING(endp + 1, "s32")) {
			if (val >= -2147483647 && val <= 2147483648)
				type = SKW_TYPE_S32;
		} else if (SKW_IS_STRING(endp + 1, "u8")) {
			if (val >= 0 && val <= 0xff)
				type = SKW_TYPE_U8;
		} else if (SKW_IS_STRING(endp + 1, "u16")) {
			if (val >= 0 && val <= 0xffff)
				type = SKW_TYPE_U16;
		} else if (SKW_IS_STRING(endp + 1, "u32")) {
			if (val >= 0 && val <= 0xffffffff)
				type = SKW_TYPE_U32;
		} else if (SKW_IS_STRING(endp + 1, "bool")) {
			if (val == 0 ||  val == 1)
				type = SKW_TYPE_BOOL;
		} else {
			type = SKW_TYPE_INVALID;
		}
	}
#undef SKW_IS_STRING

	if (type == SKW_TYPE_INVALID) {
		skw_err("invalid value: %s\n", value);
		return -EINVAL;
	}

	md = SKW_ZALLOC(sizeof(struct skw_mib_data), GFP_KERNEL);
	if (!md) {
		if (type == SKW_TYPE_STRING)
			kfree((void *)val);

		skw_err("alloc failed, %s=%s\n", key, value);

		return -ENOMEM;
	}

	md->id = id;
	md->value = val;
	md->type = type;
	INIT_LIST_HEAD(&md->list);

	list_add_tail(&md->list, head);

	return 0;
}

void skw_config_set_mib(struct wiphy *wiphy, int inst, struct list_head *head)
{
	struct skw_mib_data *mib;

	list_for_each_entry(mib, head, list) {
		switch (mib->type) {
		case SKW_TYPE_S8:
		case SKW_TYPE_U8:
		case SKW_TYPE_BOOL:
			skw_set_mib_u8(wiphy, inst, mib->id, (u8)mib->value);
			break;

		case SKW_TYPE_S16:
		case SKW_TYPE_U16:
			skw_set_mib_u16(wiphy, inst, mib->id, (u16)mib->value);
			break;

		case SKW_TYPE_S32:
		case SKW_TYPE_U32:
			skw_set_mib_u32(wiphy, inst, mib->id, (u32)mib->value);
			break;

		case SKW_TYPE_STRING:
			skw_set_mib(wiphy, inst, mib->id, (void *)mib->value,
				strlen((char *)mib->value) + 1);
			break;

		default:
			break;
		}

		skw_detail("mib: %d, type: %d, data: 0x%lx\n",
			mib->id, mib->type, mib->value);
	}
}

static struct skwifi_cfg  g_cfg_table[] = {
	{
		.name = "global",
		.parser = skw_global_parser,
	},
	{
		.name = "interface",
		.parser = skw_intf_parser,
	},
	{
		.name = "calib",
		.parser = skw_calib_parser,
	},
	{
		.name = "regdom",
		.parser = skw_regdom_parser,
	},
	{
		.name = "roaming",
		.parser = skw_roam_parser,
	},
	{
		.name = "firmware",
		.parser = skw_firmware_parser,
	},
	{
		.name = "band",
		.parser = skw_band_parser,
	},
	{
		.name = "mib",
		.parser = skw_mib_parser,
	},
	{
		.name = NULL,
		.parser = NULL,
		.priv = NULL,
	}
};

static void skw_config_mib_free(struct list_head *head)
{
	 struct skw_mib_data *md, *tmp;

	 list_for_each_entry_safe(md, tmp, head, list) {
		 if (md->type == SKW_TYPE_STRING)
			 kfree((void *)md->value);

		 kfree(md);
	 }
}

void skw_load_config(struct device *dev, const char *name, struct skw_config *cfg)
{
	int i;
	const struct firmware *fw;

	if (skw_compat_request_firmware(&fw, name, dev))
		return;

	skw_dbg("%s\n", name);

	for (i = 0; g_cfg_table[i].name != NULL; i++) {
		if (!strcmp(g_cfg_table[i].name, "global"))
			g_cfg_table[i].priv = &cfg->global;
		else if (!strcmp(g_cfg_table[i].name, "interface"))
			g_cfg_table[i].priv = &cfg->intf;
		else if (!strcmp(g_cfg_table[i].name, "calib"))
			g_cfg_table[i].priv = &cfg->calib;
		else if (!strcmp(g_cfg_table[i].name, "regdom"))
			g_cfg_table[i].priv = &cfg->regd;
		else if (!strcmp(g_cfg_table[i].name, "roaming"))
			g_cfg_table[i].priv = &cfg->roam;
		else if (!strcmp(g_cfg_table[i].name, "firmware"))
			g_cfg_table[i].priv = &cfg->fw;
		else if (!strcmp(g_cfg_table[i].name, "band"))
			g_cfg_table[i].priv = &cfg->band;
		else if (!strcmp(g_cfg_table[i].name, "mib"))
			g_cfg_table[i].priv = &cfg->mib;
		else {
			g_cfg_table[i].priv = NULL;

			skw_warn("section: %s not support\n", g_cfg_table[i].name);
		}
	}

	skw_parser(g_cfg_table, fw->data, fw->size);

	release_firmware(fw);
}

void skw_config_init(struct skw_config *conf)
{
	/* global */
	conf->global.dma_addr_align = SKW_DATA_ALIGN_SIZE;
	conf->global.reorder_timeout = 100;

	/* interface */
	strcpy(conf->intf.interface[0].name, "wlan%d");
	conf->intf.interface[0].inst = SKW_INVALID_ID;
	conf->intf.interface[0].iftype = NL80211_IFTYPE_STATION;

	/* firmare */
	conf->fw.beacon_timeout = 6;

	/* band */
	set_bit(SKW_CFG_BAND_2GHZ, &conf->band.flags);
	set_bit(SKW_CFG_BAND_5GHZ, &conf->band.flags);
	conf->band.bw_2ghz = SKW_CHAN_WIDTH_40;

	/* regdom */
	conf->regd.country[0] = '0';
	conf->regd.country[1] = '0';

	/* calib */
	strcpy(conf->calib.project, "SEEKWAVE");

	INIT_LIST_HEAD(&conf->mib.init);
	INIT_LIST_HEAD(&conf->mib.sta);
	INIT_LIST_HEAD(&conf->mib.sap);
	INIT_LIST_HEAD(&conf->mib.go);
	INIT_LIST_HEAD(&conf->mib.gc);

#ifdef CONFIG_SWT6621S_RX_REORDER_TIMEOUT
	conf->global.reorder_timeout = CONFIG_SWT6621S_RX_REORDER_TIMEOUT;
#endif

#ifdef CONFIG_SWT6621S_STA_SME_EXT
	set_bit(SKW_CFG_FLAG_STA_EXT, &conf->global.flags);
#endif

#ifdef  CONFIG_SWT6621S_SAP_SME_EXT
	set_bit(SKW_CFG_FLAG_SAP_EXT, &conf->global.flags);
#endif

#ifdef CONFIG_SWT6621S_REPEATER_MODE
	set_bit(SKW_CFG_FLAG_REPEATER, &conf->global.flags);
#endif

#ifdef CONFIG_SWT6621S_LEGACY_P2P
	set_bit(SKW_CFG_FLAG_P2P_DEV, &conf->global.flags);

	strcpy(conf->intf.interface[3].name, "p2p%d");
	conf->intf.interface[3].inst = 3;
	conf->intf.interface[3].iftype = NL80211_IFTYPE_P2P_DEVICE;
#endif

#ifdef CONFIG_SWT6621S_REGD_SELF_MANAGED
	set_bit(SKW_CFG_REGD_SELF_MANAGED, &conf->regd.flags);
#endif

#ifdef CONFIG_SWT6621S_DEFAULT_COUNTRY
	if (strlen(CONFIG_SWT6621S_DEFAULT_COUNTRY) == 2 &&
	    is_skw_valid_reg_code(CONFIG_SWT6621S_DEFAULT_COUNTRY))
		memcpy(conf->regd.country,
		       CONFIG_SWT6621S_DEFAULT_COUNTRY, 2);
#endif

#ifdef CONFIG_SWT6621S_CALIB_APPEND_BUS_ID
	set_bit(SKW_CFG_CALIB_APPEND_BUS_NAME, &conf->calib.flags);
#endif

#ifdef CONFIG_SWT6621S_CALIB_APPEND_MODULE_ID
	set_bit(SKW_CFG_CALIB_APPEND_MODULE_ID, &conf->calib.flags);
#endif

#ifdef CONFIG_SWT6621S_CHIP_ID
	if (strlen(CONFIG_SWT6621S_CHIP_ID))
		strncpy(conf->calib.chip_alias_name, CONFIG_SWT6621S_CHIP_ID,
			sizeof(conf->calib.chip_alias_name) - 1);
#endif

#ifdef CONFIG_SWT6621S_PROJECT_NAME
	if (strlen(CONFIG_SWT6621S_PROJECT_NAME))
		strncpy(conf->calib.project, CONFIG_SWT6621S_PROJECT_NAME,
			sizeof(conf->calib.project) - 1);
#endif

#ifdef CONFIG_SWT6621S_WDS
	set_bit(SKW_CFG_FIRMWARE_USE_4ADDR, &conf->fw.flags);
#endif

	skw_load_config(NULL, SKW_CONFIG_FILE, conf);
}

void skw_config_deinit(struct skw_config *conf)
{
	 skw_config_mib_free(&conf->mib.init);
	 skw_config_mib_free(&conf->mib.sta);
	 skw_config_mib_free(&conf->mib.sap);
	 skw_config_mib_free(&conf->mib.gc);
	 skw_config_mib_free(&conf->mib.go);
}
