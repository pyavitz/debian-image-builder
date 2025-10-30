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

#include <linux/string.h>
#include <linux/ctype.h>
#include <net/iw_handler.h>
#include <linux/udp.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <net/cfg80211-wext.h>
#include <linux/inet.h>
#include "skw_core.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_tx.h"
#include "skw_iw.h"
#include "skw_log.h"
#include "skw_core_dbg.h"

static int skw_iw_commit(struct net_device *dev, struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static int skw_iw_get_name(struct net_device *dev, struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static int skw_iw_set_freq(struct net_device *dev, struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static int skw_iw_get_freq(struct net_device *dev, struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static int skw_iw_set_mode(struct net_device *dev, struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static int skw_iw_get_mode(struct net_device *dev, struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");

	return 0;
}

static struct iw_statistics *skw_get_wireless_stats(struct net_device *dev)
{
	skw_dbg("traced\n");

	return NULL;
}

static const iw_handler skw_iw_standard_handlers[] = {
	IW_HANDLER(SIOCSIWCOMMIT, (iw_handler)skw_iw_commit),
	IW_HANDLER(SIOCGIWNAME, (iw_handler)skw_iw_get_name),
	IW_HANDLER(SIOCSIWFREQ, (iw_handler)skw_iw_set_freq),
	IW_HANDLER(SIOCGIWFREQ, (iw_handler)skw_iw_get_freq),
	IW_HANDLER(SIOCSIWMODE,	(iw_handler)skw_iw_set_mode),
	IW_HANDLER(SIOCGIWMODE,	(iw_handler)skw_iw_get_mode),
#ifdef CONFIG_CFG80211_WEXT_EXPORT
	IW_HANDLER(SIOCGIWRANGE, (iw_handler)cfg80211_wext_giwrange),
	IW_HANDLER(SIOCSIWSCAN,	(iw_handler)cfg80211_wext_siwscan),
	IW_HANDLER(SIOCGIWSCAN,	(iw_handler)cfg80211_wext_giwscan),
#endif
};

#ifdef CONFIG_WEXT_PRIV

#define SKW_SET_LEN_64                  64
#define SKW_SET_LEN_128                 128
#define SKW_SET_LEN_256                 256
#define SKW_SET_LEN_512                 512
#define SKW_GET_LEN_512                 512
#define SKW_SET_LEN_1024                1024
#define SKW_GET_LEN_1024                1024
#define SKW_KEEP_BUF_SIZE               1024
#define SKW_IW_WOW_BUF_SIZE             1024
#define SKW_IW_OFFLOAD_KEEP_TCP_SIZE    1024

/* max to 16 commands */
#define SKW_IW_PRIV_SET                (SIOCIWFIRSTPRIV + 1)
#define SKW_IW_PRIV_GET                (SIOCIWFIRSTPRIV + 3)
#define SKW_IW_PRIV_AT                 (SIOCIWFIRSTPRIV + 5)
#define SKW_IW_PRIV_80211MODE          (SIOCIWFIRSTPRIV + 6)
#define SKW_IW_PRIV_GET_80211MODE      (SIOCIWFIRSTPRIV + 7)
#define SKW_IW_PRIV_KEEP_ALIVE         (SIOCIWFIRSTPRIV + 8)
#define SKW_IW_PRIV_WOW_FILTER         (SIOCIWFIRSTPRIV + 9)
#define SKW_IW_PRIV_OFFLOAD_TCP       (SIOCIWFIRSTPRIV + 10)

#define SKW_IW_PRIV_LAST               SIOCIWLASTPRIV

static struct skw_keep_active_setup kp_set = {0,};
static u8 skw_wow_flted[256];

static int skw_keep_alive_add_checksum(struct skw_keep_active_rule_data *rule, u32 len, u8 hw_flag)
{
	u8 *buff = rule->payload;
	struct skw_tx_desc_conf *desc_conf;
	u8 *ptr = buff;
	struct iphdr *ip;
	struct udphdr *udp;
	__sum16 sum;
	__wsum sum1;
	u32 udp_len;

	ptr += sizeof(struct ethhdr);
	ip = (struct iphdr *)ptr;
	ip->check = 0;
	ip->check = cpu_to_le16(ip_compute_csum(ip, 20));

	ptr += sizeof(struct iphdr);
	udp = (struct udphdr *)ptr;
	udp->check = 0;

	udp_len = len - sizeof(struct ethhdr)
		 - sizeof(struct iphdr);

	if (hw_flag) {
		sum1 = 0;
		desc_conf = (struct skw_tx_desc_conf *)&rule->is_chksumed;
		desc_conf->csum = 1;

		if (IPPROTO_TCP == ip->protocol)
			desc_conf->ip_prot = 1;
		else
			desc_conf->ip_prot = 0;

		desc_conf->l4_hdr_offset = sizeof(struct iphdr) + sizeof(struct ethhdr);

		sum = (u16)~csum_tcpudp_magic(ip->saddr, ip->daddr,
				udp_len, IPPROTO_UDP, sum1);
		udp->check = cpu_to_le16(sum);

		skw_dbg("tx_desc_conf do:%d prot:%d offset:%d \n", desc_conf->csum, desc_conf->ip_prot, desc_conf->l4_hdr_offset);
	} else {
		sum1 = csum_partial(ptr, udp_len, 0);

		sum = csum_tcpudp_magic(ip->saddr, ip->daddr,
				udp_len, IPPROTO_UDP, sum1);
		udp->check = cpu_to_le16(sum);
	}

	skw_dbg("hw:%d chsum %x %x  ip:%x %x sum1:%x udp_len:%d\n", hw_flag, ip->check, udp->check,
		 ip->saddr, ip->daddr, sum1, udp_len);
	return 0;
}

static int skw_keep_active_rule_save(struct skw_core *skw,
	 struct skw_keep_active_rule *kp, u8 idx, u8 en, u32 flags)
{
	int ret;

	if (!skw || idx >= SKW_KEEPACTIVE_RULE_MAX) {
		ret = -EFAULT;
		return ret;
	}

	if (kp) {
		if (kp_set.rule[idx])
			SKW_KFREE(kp_set.rule[idx]);

		kp_set.rule[idx] = SKW_ZALLOC(kp->payload_len
			 + sizeof(*kp), GFP_KERNEL);
		memcpy(kp_set.rule[idx], kp, kp->payload_len + sizeof(*kp));

		if (SKW_KEEPALIVE_ALWAYS_FLAG & flags)
			kp_set.rule[idx]->always = 1;
		else
			kp_set.rule[idx]->always = 0;

		if ((SKW_KEEPALIVE_NEEDCHKSUM_FLAG & flags) &&
			!(SKW_KEEPALIVE_FWCHKSUM_FLAG & flags)) {
			skw_keep_alive_add_checksum(kp_set.rule[idx]->data,
					kp_set.rule[idx]->payload_len
					- sizeof(struct skw_keep_active_rule_data), false);
			kp_set.rule[idx]->data->is_chksumed = 0;
		} else if ((SKW_KEEPALIVE_NEEDCHKSUM_FLAG & flags) &&
				(SKW_KEEPALIVE_FWCHKSUM_FLAG & flags))
			skw_keep_alive_add_checksum(kp_set.rule[idx]->data,
					kp_set.rule[idx]->payload_len
					- sizeof(struct skw_keep_active_rule_data), true);
		else
			kp_set.rule[idx]->data->is_chksumed = 0;

		kp_set.flags[idx] = flags;
	}

	if (en)
		kp_set.en_bitmap |= BIT(idx);
	else
		kp_set.en_bitmap &= ~BIT(idx);

	skw_dbg("enable bitmap 0x%x\n", kp_set.en_bitmap);
	skw_hex_dump("kpsave", &kp_set, sizeof(kp_set), false);

	return 0;
}

static int skw_keep_active_disable_cmd(struct net_device *ndev)
{
	struct skw_spd_action_param spd;
	int ret = 0;

	spd.sub_cmd = ACTION_DIS_KEEPALIVE;
	spd.len = 0;

	skw_hex_dump("dpdis:", &spd, sizeof(spd), true);
	ret = skw_send_msg(ndev->ieee80211_ptr->wiphy, ndev,
			 SKW_CMD_SET_SPD_ACTION, &spd, sizeof(spd), NULL, 0);
	if (ret)
		skw_err("failed, ret: %d\n", ret);

	return ret;
}

static int skw_keep_active_append_cmd(struct net_device *ndev,
		 struct skw_core *skw, u32 idx_map, u32 idx)
{
	int ret = 0;
	u32 rules = 0;
	int total, fixed, len = 0, offset = 0;
	struct skw_spd_action_param *spd = NULL;
	struct skw_keep_active_param *kp_param = NULL;

	fixed = sizeof(struct skw_spd_action_param) +
		 sizeof(struct skw_keep_active_param);
	total = fixed + SKW_KEEPACTIVE_CMD_BUF_MAX;

	spd = SKW_ZALLOC(total, GFP_KERNEL);
	if (!spd) {
		skw_err("malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	kp_param = (struct skw_keep_active_param *)((u8 *)spd
			+ sizeof(*spd));
	offset = fixed;

	while (idx_map) {
		idx = ffs(idx_map) - 1;

		if (!kp_set.rule[idx]) {
			skw_err("rule exception\n");
			break;
		}

		if (offset + sizeof(struct skw_keep_active_rule)
			 + kp_set.rule[idx]->payload_len > total)
			break;

		memcpy((u8 *)spd + offset, kp_set.rule[idx],
				 sizeof(struct skw_keep_active_rule)
				 + kp_set.rule[idx]->payload_len);

		offset += sizeof(struct skw_keep_active_rule)
			+ kp_set.rule[idx]->payload_len;

		if (++rules > (SKW_KEEPACTIVE_RULE_MAX >> 1))
			break;

		SKW_CLEAR(idx_map, BIT(idx));
	}

	kp_param->rule_num = rules;
	spd->len = offset - sizeof(struct skw_spd_action_param);
	len = offset;

	spd->sub_cmd = ACTION_EN_KEEPALIVE_APPEND;

	skw_dbg("len:%d rule num:%d\n", len, rules);
	if (rules) {
		skw_hex_dump("actvapd:", spd, len, true);
		ret = skw_send_msg(ndev->ieee80211_ptr->wiphy, ndev,
				SKW_CMD_SET_SPD_ACTION, spd, len, NULL, 0);
		if (ret)
			skw_err("failed, ret: %d\n", ret);
	}

	SKW_KFREE(spd);
	return ret;
}

static int skw_keep_active_cmd(struct net_device *ndev, struct skw_core *skw,
		 u8 en, u32 flags)
{
	int ret = 0, append = 0;
	u32 idx_map, idx, rules = 0;
	int total, fixed, len = 0, offset = 0;
	struct skw_spd_action_param *spd = NULL;
	struct skw_keep_active_param *kp_param = NULL;

	fixed = sizeof(struct skw_spd_action_param) +
		 sizeof(struct skw_keep_active_param);
	total = fixed + SKW_KEEPACTIVE_CMD_BUF_MAX;

	spd = SKW_ZALLOC(total, GFP_KERNEL);
	if (!spd) {
		skw_err("malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	kp_param = (struct skw_keep_active_param *)((u8 *)spd
			+ sizeof(*spd));
	offset = fixed;
	idx_map = kp_set.en_bitmap;

	while (idx_map) {
		idx = ffs(idx_map) - 1;
		SKW_CLEAR(idx_map, BIT(idx));

		if (!kp_set.rule[idx]) {
			skw_err("rule exception\n");
			break;
		}

		if (offset + sizeof(struct skw_keep_active_rule)
			 + kp_set.rule[idx]->payload_len > total)
			break;

		memcpy((u8 *)spd + offset, kp_set.rule[idx],
				 sizeof(struct skw_keep_active_rule)
				 + kp_set.rule[idx]->payload_len);

		offset += sizeof(struct skw_keep_active_rule)
			+ kp_set.rule[idx]->payload_len;

		if (++rules >= (SKW_KEEPACTIVE_RULE_MAX >> 1)) {
			append++;
			break;
		}
	}

	kp_param->rule_num = rules;
	spd->len = offset - sizeof(struct skw_spd_action_param);
	len = offset;

	ret = skw_keep_active_disable_cmd(ndev);

	skw_dbg("len:%d rule num:%d\n", len, rules);
	if (rules) {
		spd->sub_cmd = ACTION_EN_KEEPALIVE;
		skw_hex_dump("actv:", spd, len, true);
		ret = skw_send_msg(ndev->ieee80211_ptr->wiphy, ndev,
				SKW_CMD_SET_SPD_ACTION, spd, len, NULL, 0);
		if (ret)
			skw_err("failed, ret: %d\n", ret);
	}

	SKW_KFREE(spd);

	if (append)
		ret = skw_keep_active_append_cmd(ndev, skw, idx_map, idx);

	return ret;
}

//iwpriv wlan0 keep_alive idx=0,en=1,period=1000,flags=0/1,
//pkt=7c:7a:3c:81:e5:72:00:0b
static int skw_keep_active_set(struct net_device *dev, u8 *param, int len)
{
	int result_len = 0;
	u8 *ch, *result_val;
	char *hex = NULL;
	char *tmp_hex = NULL;
	u8 idx, en = 0, get_pkt = 0, send_cnt = 1;
	u32 flags = 0;
	u8 keep_alive[SKW_KEEPACTIVE_LENGTH_MAX];
	struct skw_keep_active_rule *kp =
		(struct skw_keep_active_rule *)keep_alive;
	int pos = 0, ret = 0;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = iface->skw;

	memset(kp, 0, sizeof(*kp));

	kp->send_cnt = SKW_KEEPACTIVE_RULE_SEND_CNT_DEF;
	hex = param;

	hex = strstr(hex, "idx=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("idx param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("idx param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &idx);
		if (ret) {
			skw_err("idx param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("idx not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex) {
		ret = -EBADF;
		goto error;
	}

	hex = strstr(hex, "en=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("en param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("en param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &en);
		if (ret) {
			skw_err("en param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("en not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex)
		goto done;

	hex = strstr(hex, "period=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("period param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("period param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou32(ch, 0, &kp->keep_interval);
		if (ret) {
			skw_err("period param\n");
			ret = -EINVAL;
			goto error;
		}
	}

	if (!hex)
		goto done;

	tmp_hex = hex;
	hex = strstr(tmp_hex, "send_cnt=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("send cnt\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("send cnt\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &send_cnt);
		if (ret) {
			skw_err("send cnt invalid value\n");
			ret = -EINVAL;
			goto error;
		}

		kp->send_cnt = send_cnt;
		if (kp->send_cnt <= 0) {
			kp->send_cnt = SKW_KEEPACTIVE_RULE_SEND_CNT_DEF;
			skw_err("send cnt is bigger than %d",
				SKW_KEEPACTIVE_RULE_SEND_CNT_MAX);
		}

		if (kp->send_cnt > SKW_KEEPACTIVE_RULE_SEND_CNT_MAX) {
			kp->send_cnt = SKW_KEEPACTIVE_RULE_SEND_CNT_MAX;
			skw_err("send cnt is bigger than %d",
				SKW_KEEPACTIVE_RULE_SEND_CNT_MAX);
		}
	} else
		hex = tmp_hex;

	hex = strstr(hex, "flags=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("flags param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("flags param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou32(ch, 0, &flags);
		if (ret) {
			skw_err("flags param\n");
			ret = -EINVAL;
			goto error;
		}
	}

	if (!hex)
		goto done;

	hex = strstr(hex, "pkt=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("pkt param\n");
			ret = -EFAULT;
			goto error;
		}

		result_val = kp->data->payload;
		while (1) {
			u8 temp = 0;
			char *cp = strchr(hex, ':');

			if (cp) {
				*cp = 0;
				cp++;
			}

			ret = kstrtou8(hex, 16, &temp);
			if (ret) {
				skw_err("pkt param\n");
				ret = -EINVAL;
				goto error;
			}

			if (temp > 255) {
				skw_err("pkt param\n");
				ret = -ERANGE;
				goto error;
			}

			result_val[pos] = temp;
			result_len++;
			pos++;

			if (!cp)
				break;

			if (result_len + sizeof(*kp) +
				sizeof(struct skw_keep_active_rule_data) >=
				SKW_KEEPACTIVE_LENGTH_MAX) {
				skw_err("len overload\n");
				ret = -ENOSPC;
				goto error;
			}

			hex = cp;
		}
		get_pkt = 1;
	}

	kp->payload_len = result_len + sizeof(struct skw_keep_active_rule_data);

done:
	skw_dbg("idx:%d en:%d pr:%d cnt:%d pkt:%d len:%d\n", idx, en,
		 kp->keep_interval, kp->send_cnt, get_pkt, result_len);
	skw_hex_dump("kp", kp, sizeof(*kp) + kp->payload_len, false);

	if (!(kp->keep_interval && get_pkt))
		kp = NULL;

	ret = skw_keep_active_rule_save(skw, kp, idx, en, flags);
	if (ret) {
		skw_err("save rule\n");
		goto error;
	}

	ret = skw_keep_active_cmd(dev, skw, en, flags);
	if (ret) {
		skw_err("send rule\n");
		goto error;
	}

	return 0;

error:
	skw_err("error:%d\n", ret);
	return ret;
}

//iwpriv wlan0 wow_filter idx=0,pattern=6+7c:7a:3c:81:e5:72#20+!ee:66#50+*3b:27:26:5e:2a
//iwpriv wlan0 wow_filter disable
//iwpriv wlan0 wow_filter enable/enable_whitelist
//iwpriv wlan0 wow_filter "list idx=0"
static struct skw_wow_rules_set wow_rules_set = {0, 0, 0};
static struct skw_wow_user_rules wow_user_rules = {0,};

static inline int ffs64(u64 x)
{
	int r = 1;

	if (!x)
		return 0;

	if (!(x & 0xffffffffffffff)) {
		x >>= 56;
		r += 56;
	}

	if (!(x & 0xffffffffffff)) {
		x >>= 48;
		r += 48;
	}

	if (!(x & 0xffffffffff)) {
		x >>= 40;
		r += 40;
	}

	if (!(x & 0xffffffff)) {
		x >>= 32;
		r += 32;
	}

	if (!(x & 0xffffff)) {
		x >>= 24;
		r += 24;
	}

	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}

	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}

	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}

	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}

	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}

	return r;
}

static int skw_send_wow_filter_append_cmd(struct net_device *ndev,
	struct skw_core *skw, u64 idx_map)
{
	int ret = 0, append = 0;
	u8 idx, rules = 0;
	int total, fixed, offset = 0;
	struct skw_spd_action_param *spd = NULL;
	struct skw_wow_input_param *wow_param = NULL;
	struct skw_wow_rule *send_rule = NULL;

	fixed = sizeof(struct skw_spd_action_param) +
		 sizeof(struct skw_wow_input_param);
	total = fixed + SKW_WOW_RULE_CMD_BUF_MAX; //SKW_MAX_WOW_RULE_NUM_ONE_TIME * struct skw_wow_rule

	spd = SKW_ZALLOC(total, GFP_KERNEL);
	if (!spd) {
		skw_err("malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	wow_param = (struct skw_wow_input_param *)((u8 *)spd
			+ sizeof(*spd));
	offset = fixed;

	while (idx_map) {
		idx = ffs64(idx_map) - 1;

		if (offset + sizeof(struct skw_wow_input_param)
			+ wow_rules_set.rules[idx].len > total)
			break;

		spd->sub_cmd = ACTION_EN_WOW_APPEND;
		wow_param->wow_flags = wow_rules_set.wow_flags;
		memcpy(&wow_param->rules[rules], &wow_rules_set.rules[idx],
			sizeof(struct skw_wow_rule));

		send_rule = &wow_param->rules[rules];
		skw_hex_dump("wow_param->rules",  &wow_param->rules[rules], sizeof(*send_rule), false);

		SKW_CLEAR(idx_map, (1ULL << idx));

		if (++rules >= SKW_MAX_WOW_RULE_NUM_ONE_TIME) {
			append++;
			break;
		}
	}
	wow_param->rule_num = rules;
	spd->len = sizeof(struct skw_wow_input_param) +
		sizeof(struct skw_wow_rule) * wow_param->rule_num;

	skw_dbg("wow_param->rule_num:%d len:%d %d\n", wow_param->rule_num, spd->len, total);

	skw_hex_dump("append spd", spd, total, false);

	ret = skw_msg_xmit(ndev->ieee80211_ptr->wiphy, 0,
		 SKW_CMD_SET_SPD_ACTION, spd, total, NULL, 0);
	if (ret)
		skw_err("failed, ret: %d\n", ret);
	else {
		memset(skw_wow_flted, 0, sizeof(skw_wow_flted));
		//memcpy(skw_wow_flted, param, len);   //TBD
	}

	if (append)
		ret = skw_send_wow_filter_append_cmd(ndev, skw, idx_map);

	SKW_KFREE(spd);

	return ret;
}

u8 skw_wow_filter_remap_idx(u8 count)
{
	u64 idx_map = 0;
	u8 idx = 0, rules = 0;

	if (count >= SKW_MAX_WOW_RULE_NUM)
		return 0xff;

	idx_map = wow_rules_set.en_bitmap;

	while (idx_map) {
		idx = ffs64(idx_map) - 1;
		SKW_CLEAR(idx_map, (1ULL << idx));

		if (rules++ == count)
			break;
	}

	skw_dbg("to idx:%d\n", idx);
	return idx;
}

static int skw_send_wow_filter_cmd(struct net_device *ndev, struct skw_core *skw)
{
	int ret = 0, append = 0;
	u64 idx_map  = 0;
	u8 idx, rules = 0;
	int total, fixed, offset = 0;
	struct skw_spd_action_param *spd = NULL;
	struct skw_wow_input_param *wow_param = NULL;
	struct skw_wow_rule *send_rule = NULL;

	fixed = sizeof(struct skw_spd_action_param) +
		 sizeof(struct skw_wow_input_param);
	total = fixed + SKW_WOW_RULE_CMD_BUF_MAX;

	spd = SKW_ZALLOC(total, GFP_KERNEL);
	if (!spd) {
		skw_err("malloc failed, size: %d\n", total);
		return -ENOMEM;
	}

	wow_param = (struct skw_wow_input_param *)((u8 *)spd
			+ sizeof(*spd));
	offset = fixed;

	idx_map = wow_rules_set.en_bitmap;

	while (idx_map) {
		idx = ffs64(idx_map) - 1;

		if (offset + sizeof(struct skw_wow_input_param)
			+ wow_rules_set.rules[idx].len > total)
			break;

		spd->sub_cmd = ACTION_EN_WOW;
		wow_param->wow_flags = wow_rules_set.wow_flags;
		memcpy(&wow_param->rules[rules], &wow_rules_set.rules[idx],
			sizeof(struct skw_wow_rule));

		send_rule = &wow_param->rules[rules];

		skw_hex_dump("wow_param->rules",  &wow_param->rules[rules], sizeof(*send_rule), false);
		SKW_CLEAR(idx_map, (1ULL << idx));

		if (++rules >= SKW_MAX_WOW_RULE_NUM_ONE_TIME) {
			append++;
			break;
		}
	}
	wow_param->rule_num = rules;
	spd->len = sizeof(struct skw_wow_input_param) +
		sizeof(struct skw_wow_rule) * wow_param->rule_num;

	skw_dbg("wow_param->rule_num:%d len:%d total:%d\n",  wow_param->rule_num, spd->len, total);

	skw_hex_dump("spd", spd, sizeof(struct skw_spd_action_param) + spd->len, false);

	ret = skw_msg_xmit(ndev->ieee80211_ptr->wiphy, 0,
		 SKW_CMD_SET_SPD_ACTION, spd, total, NULL, 0);
	if (ret)
		skw_err("failed, ret: %d\n", ret);
	else {
		memset(skw_wow_flted, 0, sizeof(skw_wow_flted));
		//memcpy(skw_wow_flted, param, len);   //TBD
	}

	if (append)
		ret = skw_send_wow_filter_append_cmd(ndev, skw, idx_map);

	SKW_KFREE(spd);

	return ret;
}

int skw_wow_filter_set(struct net_device *ndev, u8 *param, int len, char *resp, int *extra_len)
{
	u8 *ch, *result_val, rule_idx = 0;
	char *hex, *tmp_ch, *ptr;
	struct skw_wow_rule *rule = NULL;
	int pos = 0, ret = 0, offset, result_len = 0;
	struct skw_pkt_pattern *ptn;
	u8 *data;
	u32 temp, resp_len = 0;
	char help[] = "Usage:['list idx=0']|[disable]|[idx=0,pattern=6+10#23+!11][enable/enable_whitelist]";
	struct skw_iface *iface = netdev_priv(ndev);
	struct skw_core *skw = iface->skw;
	char t_rule[SKW_MAX_WOW_USER_RULE_LEN] = {0};

	data = SKW_ZALLOC(SKW_IW_WOW_BUF_SIZE, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memcpy(data, param, len);
	hex = data;
	ptr = hex;

	if (!strncmp(hex, "list idx=", strlen("list idx="))) {
		if (len <= strlen("list idx=")) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				"list cmd");
			ret = -EFAULT;
			goto free;
		}

		hex += strlen("list idx=");

		ret = kstrtou8(hex, 0, &rule_idx);
		if (ret) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				"list cmd idx param");
			skw_err("idx param\n");
			ret = -EINVAL;
			goto free;
		}

		if (rule_idx >= SKW_MAX_WOW_RULE_NUM) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				"list cmd idx param  is too big");
			skw_err("rule_idx param:%d is too big\n", rule_idx);
			ret = -EINVAL;
			goto free;
		}

		if (!((1ULL << rule_idx) & wow_user_rules.en_bitmap)) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				"list cmd idx param  is unset");
			skw_err("rule_idx param:%d is unset\n", rule_idx);
			ret = -EINVAL;
			goto free;
		}

		resp_len = sprintf(resp, "List: %s\n", wow_user_rules.rules[rule_idx]);
		goto free;
	}

	if (!strcmp(hex, "disable")) {
		if (len != sizeof("disable")) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				 "dis cmd");
			ret = -EFAULT;
			goto free;
		}
		ret = skw_wow_disable(ndev->ieee80211_ptr->wiphy);
		if (!ret) {
			memset(skw_wow_flted, 0, sizeof(skw_wow_flted));
			memcpy(skw_wow_flted, param, len);
			memset(&wow_rules_set, 0, sizeof(wow_rules_set));
			memset(&wow_user_rules, 0, sizeof(wow_user_rules));
		}

		goto free;
	}

	//Add wow filter by idx
	if (!strncmp(hex, "idx=", strlen("idx="))) {
		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("idx param\n");
			resp_len = sprintf(resp, "ERROR: %s\n",
				 "idx param");
			ret = -ERANGE;
			goto free;
		}

		tmp_ch = strsep((char **)&ch, "=");
		if ((tmp_ch == NULL) || (strlen(tmp_ch) == 0)) {
			skw_err("idx param\n");
			resp_len = sprintf(resp, "ERROR: %s\n",
				 "idx param");
			ret = -EFAULT;
			goto free;
		}

		ret = kstrtou8(ch, 0, &rule_idx);
		if (ret) {
			skw_err("idx param\n");
			resp_len = sprintf(resp, "ERROR: %s\n",
				 "idx param");
			ret = -EINVAL;
			goto free;
		}

		if (rule_idx >= SKW_MAX_WOW_RULE_NUM) {
			resp_len = sprintf(resp, "ERROR: %s\n",
				 "idx param is too big");
			skw_err("rule_idx param:%d is too big\n", rule_idx);
			ret = -EINVAL;
			goto free;
		}

		//Store the temp rule
		memset(t_rule, 0, SKW_MAX_WOW_USER_RULE_LEN);
		strncpy(t_rule, param, min_t(int, SKW_MAX_WOW_USER_RULE_LEN, len));

		//Translate the wow filter to skw rules
		rule = &wow_rules_set.rules[rule_idx];
		result_len = 0;

		ret = strncmp(hex, "pattern=", strlen("pattern="));
		if (!ret) {
			hex += strlen("pattern=");
			result_val = rule->rule;

			while (hex < ptr + len - 1) {
				ret = sscanf(hex, "%d+%02x",
					&offset, &temp);
				if (ret != 2) {
					ret = sscanf(hex, "%d+!%02x",
						&offset, &temp);
					if (ret != 2) {
						ret = sscanf(hex, "%d+*%02x",
							&offset, &temp);
						if (ret != 2) {
							resp_len = sprintf(resp,
								"ERROR: %s\n",
								"match char + +*");
							ret = -EINVAL;
							goto free;
						}
					}
				}

				if (offset > ETH_DATA_LEN) {
					resp_len = sprintf(resp,
						"ERROR: offset:%d over limit\n",
						offset);
					ret = -EINVAL;
					goto free;
				}

				ptn = (struct skw_pkt_pattern *)result_val;
				result_val += sizeof(*ptn);
				result_len += sizeof(*ptn);

				if (result_len >= sizeof(rule->rule)) {
					resp_len = sprintf(resp,
						"ERROR: %s\n",
						"ptn over limit\n");
					ret = -ERANGE;
					goto free;
				}

				ptn->type_offset = PAT_TYPE_ETH;
				ptn->offset = offset;

				ch = (u8 *)strsep(&hex, "+");
				if ((ch == NULL) || (strlen(ch) == 0)) {
					resp_len = sprintf(resp,
						"ERROR: %s\n",
						"match char +\n");
					ret = -EINVAL;
					goto free;
				}

				if (hex[0] == '!') {
					ptn->op = PAT_OP_TYPE_DIFF;
					ch = strsep(&hex, "!");
				} else if (hex[0] == '*') {
					ptn->op = PAT_OP_TYPE_CONTINUE_MATCH;
					ch = strsep(&hex, "*");
				}

				pos = 0;
				while (hex < ptr + len - 1) {
					char *cp;

					if (!hex) {
						ret = -EINVAL;
						goto free;
					}

					if (isxdigit(hex[0]) &&
						isxdigit(hex[1]) &&
						(sscanf(hex, "%2x", &temp)
							== 1)) {
					} else {
						resp_len = sprintf(resp,
							"ERROR: match char %c%c end\n",
							hex[0], hex[1]);
						ret = -EINVAL;
						goto free;
					}

					result_val[pos] = temp;
					result_len++;
					pos++;

					if (result_len >= sizeof(rule->rule)) {
						resp_len = sprintf(resp,
							"ERROR: %s\n",
							"size over limit\n");
						ret = -ERANGE;
						goto free;
					}

					if (hex[2] == ',' || hex[2] == '#')
						break;
					else if (hex[2] == '\0') {
						hex += 2;
						break;
					} else  if (hex[2] != ':') {
						resp_len = sprintf(resp,
							"ERROR: char data %c\n",
							hex[2]);
						ret = -EINVAL;
						goto free;
					}

					cp = strchr(hex, ':');
					if (cp) {
						*cp = 0;
						cp++;
					}

					hex = cp;
				}

				result_val += pos;
				ptn->len = pos;

				if (hex[2] == ',') {
					hex += 2;
					break;
				} else if (hex[2] == '#')
					ch = strsep(&hex, "#");
			}
		} else {
			resp_len = sprintf(resp, "ERROR: %s\n",
				"match char pattern=\n");
			ret = -EINVAL;
			goto free;
		}

		rule->len = result_len;
		wow_rules_set.en_bitmap |= (1ULL << rule_idx);
		skw_hex_dump("rule", rule, sizeof(*rule), false);

		//Store the rule
		wow_user_rules.en_bitmap |=  (1ULL << rule_idx);
		memset(wow_user_rules.rules[rule_idx], 0, SKW_MAX_WOW_USER_RULE_LEN);
		strncpy(wow_user_rules.rules[rule_idx], t_rule, SKW_MAX_WOW_USER_RULE_LEN);

		ret = 0;

		goto free;
	}
	//Make sure enable/enable_whitelist is the final cmd
	ret = strncmp(hex, "enable", strlen("enable"));
	if (ret) {
		resp_len = sprintf(resp, "ERROR\n");
		ret = -EFAULT;
		goto free;
	}

	if (len != sizeof("enable") && len != sizeof("enable_whitelist")) {
		skw_err("Error: enable");
		resp_len = sprintf(resp, "Error: enable\n");
		ret = EINVAL;
		goto free;
	}

	hex += strlen("enable");
	ret = strncmp(hex, "_whitelist", strlen("_whitelist"));
	if (!ret)
		hex += strlen("_whitelist");
	else
		wow_rules_set.wow_flags |= SKW_WOW_BLACKLIST_FILTER;

	//Send the rules to FW
	ret = skw_send_wow_filter_cmd(ndev, skw);
	if (ret) {
		resp_len = sprintf(resp, "Error: send filter cmd\n");
		skw_err("Error: send filter cmd");
	}

free:

	if (ret)
		resp_len += sprintf(resp + resp_len, " %s\n", help);
	else
		resp_len += sprintf(resp + resp_len, "%s\n", "OK");

	*extra_len = resp_len;

	SKW_KFREE(data);
	return 0;
}

//#define CONFIG_SKW_TCP_OFFLOAD_SUPPORT 1
#ifdef CONFIG_SKW_TCP_OFFLOAD_SUPPORT
static struct skw_tcp_keep_active_offload_set skw_tcp_offload = {0};

void skw_rx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
	struct skw_tcp_keepalive_info *tcpinfo;
	struct ethhdr *eth = eth_hdr(skb);
	unsigned int tcphoff;
	struct iphdr *ip;
	u8 ip_proto;
	int j;

	tcpinfo = skw_tcp_offload.tcpinfo;

	if (eth->h_proto == htons(ETH_P_IP)) {
		ip = (struct iphdr *)(skb->data);
		tcphoff = ip->ihl * 4;

		ip_proto = ip->protocol;

		for (j = 0; j < SKW_MAX_TCPKEEP_OFFLOAD_INFO_NUM; j++) {
			if (ip->protocol == IPPROTO_TCP &&
			  ip->saddr == tcpinfo[j].ipaddr_peer &&
			  ip->daddr == iface->sta.ipv4 &&
			  iface->id == tcpinfo[j].inst_id) {
				struct tcphdr *tcph;

				tcph = (struct tcphdr *)(skb->data + tcphoff);

				if (ntohs(tcph->dest) == tcpinfo[j].port_self &&
					ntohs(tcph->source) == tcpinfo[j].port_peer) {
					tcpinfo[j].seqnum_self = ntohl(tcph->ack_seq);
					skw_ether_copy(tcpinfo[j].peer_addr, eth->h_source);
				}
			}
		}
	}
}

void skw_tx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
	struct skw_tcp_keepalive_info *tcpinfo;
	struct ethhdr *eth = eth_hdr(skb);
	u16 eth_type = ntohs(eth->h_proto);
	int j;

	tcpinfo = skw_tcp_offload.tcpinfo;

	if (eth_type == ETH_P_IP) {
		struct iphdr *ip = ip_hdr(skb);
		u8 ip_proto = ip->protocol;

		for (j = 0; j < SKW_MAX_TCPKEEP_OFFLOAD_INFO_NUM; j++) {
			if (ip_proto == IPPROTO_TCP &&
			 ip->daddr == tcpinfo[j].ipaddr_peer &&
			 ip->saddr == iface->sta.ipv4 &&
			 iface->id == tcpinfo[j].inst_id) {
				struct tcphdr *tcph = tcp_hdr(skb);

				if (ntohs(tcph->dest) == tcpinfo[j].port_peer &&
					ntohs(tcph->source) == tcpinfo[j].port_self) {
					tcpinfo[j].acknum_self = ntohl(tcph->ack_seq);
				}
			}
		}
	}
}

u32 skw_fill_tcpalive_offload_info(struct wiphy *wiphy, u8 *tcp_off)
{
	int j, tcp_tlv_data_len = 0;
	struct skw_tcp_keepalive_offload_tlv off_tlv = {0};

	tcp_tlv_data_len += sizeof(off_tlv);

	for (j = 0; j < SKW_MAX_TCPKEEP_OFFLOAD_INFO_NUM; j++) {
		if (atomic_read(&skw_tcp_offload.enabled[j])) {
			memcpy(tcp_off + tcp_tlv_data_len, &skw_tcp_offload.tcpinfo[j],
					sizeof(struct skw_tcp_keepalive_info));
			tcp_tlv_data_len += sizeof(struct skw_tcp_keepalive_info);
			off_tlv.tcpinfo_num++;
		}
	}

	memcpy(tcp_off, &off_tlv, sizeof(off_tlv));
	skw_hex_dump("off_tlv", tcp_off, tcp_tlv_data_len, false);

	return tcp_tlv_data_len;
}

int skw_offload_tcp_set(struct net_device *ndev, char *param, int len, char *resp, int *extra_len)
{
	u8 *ch, en = 0, idx;
	char *hex = NULL;
	struct skw_tcp_keepalive_info keep;
	int ret = 0, temp1, temp2, resp_len = 0;
	struct skw_iface *iface = netdev_priv(ndev);
	char help[] = "Usage:offload_tcp idx=1,en=1,interval=1500,probe=100:3,port=18888#192.168.0.166:52222";

	hex = param;

	hex = strstr(hex, "idx=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("idx param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("idx param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &idx);
		if (ret) {
			skw_err("idx param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("idx not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex) {
		ret = -EBADF;
		goto error;
	}

	if (idx >= SKW_MAX_TCPKEEP_OFFLOAD_INFO_NUM) {
		skw_err("tcpinfo_num exceed\n");
		resp_len = sprintf(resp,
			"ERROR: %s\n",
			"tcpinfo_num exceed\n");
		ret = -EINVAL;
		goto error;
	}

	hex = strstr(hex, "en=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("en param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"en param\n");;
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("en param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"en param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &en);
		if (ret) {
			skw_err("en param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"en param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("en not found\n");
		resp_len = sprintf(resp,
			"ERROR: %s\n",
			"en not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex)
		goto done;

	hex = strstr(hex, "interval=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("interval param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"interval param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("interval param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"interval param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou16(ch, 0, &keep.interval_idle);
		if (ret) {
			skw_err("interval param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"interval param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("interval not found\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"interval not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex) {
		ret = -EBADF;
		goto error;
	}

	hex = strstr(hex, "probe=");
	if (hex) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("probe param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe param\n");
			ret = -EFAULT;
			goto error;
		}

		ch = strsep(&hex, ":");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("probe param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou16(ch, 0, &keep.probe_interval);
		if (ret) {
			skw_err("probe param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe param\n");
			ret = -EINVAL;
			goto error;
		}

		ch = strsep(&hex, ",");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("probe param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe param\n");
			ret = -ERANGE;
			goto error;
		}

		ret = kstrtou8(ch, 0, &keep.probe_count);
		if (ret) {
			skw_err("probe param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe param\n");
			ret = -EINVAL;
			goto error;
		}
	} else {
		skw_err("probe not found\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"probe not found\n");
		ret = -EFAULT;
		goto error;
	}

	if (!hex) {
		ret = -EBADF;
		goto error;
	}

	ret = strncmp(hex, "port=", strlen("port="));
	if (!ret) {
		ch = strsep(&hex, "=");
		if ((ch == NULL) || (strlen(ch) == 0)) {
			skw_err("port param\n");
			resp_len = sprintf(resp,
				"ERROR: %s\n",
				"port param\n");
			ret = -EFAULT;
			goto error;
		}

		while (hex < param + len - 1) {
			ret = sscanf(hex, "%d#%d",
				&temp1, &temp2);
			if (ret != 2) {
				ret = -EINVAL;
				goto error;
			}


			ch = strsep(&hex, "#");
			if ((ch == NULL) || (strlen(ch) == 0)) {
				resp_len = sprintf(resp,
					"ERROR: %s\n",
					"match char #\n");
				ret = -EINVAL;
				goto error;
			}

			ret = kstrtou16(ch, 0, &keep.port_self);
			if (ret) {
				skw_err("port_self param\n");
				resp_len = sprintf(resp,
					"ERROR: %s\n",
					"port_self param\n");
				ret = -EINVAL;
				goto error;
			}

			ch = strsep(&hex, ":");
			if ((ch == NULL) || (strlen(ch) == 0)) {
				resp_len = sprintf(resp,
					"ERROR: %s\n",
					"match char :\n");
				ret = -EINVAL;
				goto error;
			}

			keep.ipaddr_peer = in_aton(ch);
			if (keep.ipaddr_peer == 0) {
				skw_err("Invalid IPv4: %s", ch);
				resp_len = sprintf(resp,
					"ERROR:Invalid IPv4: %s\n", ch);
				goto error;
			}

			ch = strsep(&hex, "+");

			if ((ch == NULL) || (strlen(ch) == 0)) {
				resp_len = sprintf(resp,
					"ERROR: %s\n",
					"match char #\n");
				ret = -EINVAL;
				goto error;
			}

			ret = kstrtou16(ch, 0, &keep.port_peer);
			if (ret) {
				skw_err("port_peer param\n");
				resp_len = sprintf(resp,
					"ERROR: %s\n",
					"port_peer param\n");
				ret = -EINVAL;
				goto error;
			}

			break;
		}
	} else {
		resp_len = sprintf(resp, "ERROR: %s\n",
			"port=\n");
		ret = -EINVAL;
		goto error;
	}

	keep.inst_id = iface->id;
done:
	if (en) {
		atomic_set(&skw_tcp_offload.enabled[idx], 1);
		memcpy(&skw_tcp_offload.tcpinfo[idx], &keep, sizeof(struct skw_tcp_keepalive_info));
		skw_hex_dump("kp:", &keep, sizeof(struct skw_tcp_keepalive_info), false);
	} else {
		atomic_set(&skw_tcp_offload.enabled[idx], 0);
		memset(&skw_tcp_offload.tcpinfo[idx], 0, sizeof(struct skw_tcp_keepalive_info));
	}

	skw_hex_dump("skw_tcp_offload:", &skw_tcp_offload, sizeof(skw_tcp_offload), false);

error:
	if (ret)
		resp_len += sprintf(resp + resp_len, " %s\n", help);
	else
		resp_len += sprintf(resp + resp_len, "%s\n", "OK");

	*extra_len = resp_len;

	return ret;
}
#else
void skw_rx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
}

void skw_tx_dump_tcp_data(struct skw_iface *iface, struct sk_buff *skb)
{
}

u32 skw_fill_tcpalive_offload_info(struct wiphy *wiphy, u8 *tcp_off)
{
	return 0;
}
#endif

static int skw_iwpriv_keep_alive(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	char *param = NULL;
	char help[] = "ERROR useage:[idx=0,en=0/1,period=100,flags=0/1,pkt=7c:11]";
	int ret = 0;

	WARN_ON(wrqu->data.length > SKW_KEEP_BUF_SIZE);

	param = SKW_ZALLOC(SKW_KEEP_BUF_SIZE, GFP_KERNEL);
	if (!param) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, wrqu->data.pointer, SKW_KEEP_BUF_SIZE)) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		ret = -EFAULT;
		goto free;
	}

	skw_dbg("cmd: 0x%x, (len: %d)\n",
		info->cmd, wrqu->data.length);
	param[SKW_KEEP_BUF_SIZE - 1] = '\0';
	skw_hex_dump("param:", param, SKW_KEEP_BUF_SIZE, false);

	ret = skw_keep_active_set(dev, param, SKW_KEEP_BUF_SIZE);
	if (ret)
		memcpy(extra, help, sizeof(help));
	else
		memcpy(extra, "OK", sizeof("OK"));

	wrqu->data.length = SKW_GET_LEN_512;

	skw_dbg("resp: %s\n", extra);

free:
	SKW_KFREE(param);

out:
	return ret;
}

static int skw_iwpriv_wow_filter(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	char *param;
	int ret;
	int extra_len;

	WARN_ON(wrqu->data.length > SKW_IW_WOW_BUF_SIZE);

	param = SKW_ZALLOC(SKW_IW_WOW_BUF_SIZE, GFP_KERNEL);
	if (!param) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, wrqu->data.pointer, SKW_IW_WOW_BUF_SIZE)) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		ret = -EFAULT;
		goto free;
	}

	skw_hex_dump("flt", param, SKW_IW_WOW_BUF_SIZE, false);

	ret = skw_wow_filter_set(dev, param,
		min_t(int, SKW_IW_WOW_BUF_SIZE, (int)wrqu->data.length),
		extra, &extra_len);

	wrqu->data.length = extra_len;

free:
	SKW_KFREE(param);

out:
	return ret;
}

static int skw_iwpriv_offload_keepalive(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	char *param;
	int ret;
	int extra_len;

	WARN_ON(wrqu->data.length > SKW_IW_OFFLOAD_KEEP_TCP_SIZE);

	param = SKW_ZALLOC(SKW_IW_OFFLOAD_KEEP_TCP_SIZE, GFP_KERNEL);
	if (!param) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, wrqu->data.pointer, SKW_IW_OFFLOAD_KEEP_TCP_SIZE)) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		ret = -EFAULT;
		goto free;
	}

	skw_hex_dump("ofl", param, SKW_IW_OFFLOAD_KEEP_TCP_SIZE, false);

#ifdef CONFIG_SKW_TCP_OFFLOAD_SUPPORT
	ret = skw_offload_tcp_set(dev, param,
		min_t(int, SKW_IW_OFFLOAD_KEEP_TCP_SIZE, (int)wrqu->data.length),
		extra, &extra_len);
#else
		extra_len = sprintf(extra,
			"ERROR: %s\n",
			"macro not defined\n");
		ret = -EINVAL;
#endif

	wrqu->data.length = extra_len;

free:
	SKW_KFREE(param);

out:
	return ret;
}

static int skw_send_at_cmd(struct skw_core *skw, char *cmd, int cmd_len,
			char *buf, int buf_len)
{
	int ret, len, resp_len, offset;
	char *command, *resp;

	len = round_up(cmd_len, 4);
	if (len > SKW_SET_LEN_256)
		return -E2BIG;

	command = SKW_ZALLOC(SKW_SET_LEN_512, GFP_KERNEL);
	if (!command) {
		ret = -ENOMEM;
		goto out;
	}

	offset = (long)command & 0x7;
	if (offset) {
		offset = 8 - offset;
		skw_detail("command: 0x%p, offset: %d\n", command, offset);
	}

	resp_len = round_up(buf_len, skw->hw_pdata->align_value);
	resp = SKW_ZALLOC(resp_len, GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto fail_alloc_resp;
	}

	ret = skw_uart_open(skw);
	if (ret < 0)
		goto failed;

	memcpy(command + offset, cmd, cmd_len);
	ret = skw_uart_write(skw, command + offset, len);
	if (ret < 0)
		goto failed;

	ret = skw_uart_read(skw, resp, resp_len);
	if (ret < 0)
		goto failed;

	memcpy(buf, resp, buf_len);
	ret = 0;

failed:
	SKW_KFREE(resp);

fail_alloc_resp:
	SKW_KFREE(command);
out:
	if (ret < 0)
		skw_err("failed: ret: %d\n", ret);

	return ret;
}

static int skw_iwpriv_mode(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int i;
	char param[32] = {0};
	struct skw_iface *iface = (struct skw_iface *)netdev_priv(dev);

	struct skw_iw_wireless_mode {
		char *name;
		enum skw_wireless_mode mode;
	} modes[] = {
		{"11B", SKW_WIRELESS_11B},
		{"11G", SKW_WIRELESS_11G},
		{"11A", SKW_WIRELESS_11A},
		{"11N", SKW_WIRELESS_11N},
		{"11AC", SKW_WIRELESS_11AC},
		{"11AX", SKW_WIRELESS_11AX},
		{"11G_ONLY", SKW_WIRELESS_11G_ONLY},
		{"11N_ONLY", SKW_WIRELESS_11N_ONLY},

		/*keep last*/
		{NULL, 0}
	};

	WARN_ON(sizeof(param) < wrqu->data.length);

	if (copy_from_user(param, wrqu->data.pointer, sizeof(param))) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		return -EFAULT;
	}

	param[sizeof(param) - 1] = '\0';
	skw_dbg("cmd: 0x%x, %s(len: %d)\n",
		info->cmd, param, wrqu->data.length);

	for (i = 0; modes[i].name; i++) {
		if (!strcmp(modes[i].name, (char *)param)) {
			iface->extend.wireless_mode = modes[i].mode;
			return 0;
		}
	}

	return -EINVAL;
}

static int skw_iwpriv_get_mode(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	skw_dbg("traced\n");
	return 0;
}

static int skw_iwpriv_help(struct skw_iface *iface, void *param, char *args,
			char *resp, int resp_len)
{
	int len = 0;
	struct skw_iwpriv_cmd *cmd = param;

	len = sprintf(resp, " %s:\n", cmd->help_info);
	cmd++;

	while (cmd->handler) {
		len += sprintf(resp + len, "%-4.4s %s\n", "", cmd->help_info);
		cmd++;
	}

	return 0;
}

static int skw_iwpriv_set_bandcfg(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	u16 res;
	int ret;

	if (args == NULL)
		return -EINVAL;

	ret = kstrtou16(args, 10, &res);
	if (!ret && res < 3) {
		if (res == 0)
			iface->extend.scan_band_filter = 0;
		else if (res == 1)
			iface->extend.scan_band_filter = BIT(NL80211_BAND_2GHZ);
		else if (res == 2)
			iface->extend.scan_band_filter = BIT(NL80211_BAND_5GHZ);

		sprintf(resp, "ok");
	} else
		sprintf(resp, "failed");

	return ret;
}

static int skw_iwpriv_get_bandcfg(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	if (!iface->extend.scan_band_filter)
		sprintf(resp, "bandcfg=%s", "Auto");
	else if (iface->extend.scan_band_filter & BIT(NL80211_BAND_2GHZ))
		sprintf(resp, "bandcfg=%s", "2G");
	else if (iface->extend.scan_band_filter & BIT(NL80211_BAND_5GHZ))
		sprintf(resp, "bandcfg=%s", "5G");

	return 0;
}

static int skw_iwpriv_set_max_ppdu_dur(struct skw_iface *iface, void *param,
	char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL;
	struct skw_max_ppdu_dur max_ppdu_dur = {0};

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	p = strchr(args, ',');
	if (!p) {
		skw_err("idx not found\n");
		return -ENOTSUPP;
	}

	skw_dbg("idx found %s\n", p - 1);

	max_ppdu_dur.idx = simple_strtol(p - 1, NULL, 10);
	if (max_ppdu_dur.idx < 0 || max_ppdu_dur.idx > 5) {
		skw_err("idx out of range\n");
		return -EINVAL;
	}

	p = p + strlen(",");
	if (!p) {
		skw_err("mppdu dur not found\n");
		return -ENOTSUPP;
	}

	skw_dbg("mppdu dur found %s\n", p);
	max_ppdu_dur.max_ppdu_dur = simple_strtol(p, NULL, 10);
	switch (max_ppdu_dur.idx) {
	case 0:
		if (max_ppdu_dur.max_ppdu_dur > 0xFFFF) {
			max_ppdu_dur.max_ppdu_dur = 0xFFFF;
			skw_warn("The max ppdu dur of idx 0 is 0xFFFF\n");
		}

		break;

	case 1:
		if (max_ppdu_dur.max_ppdu_dur > 0x7FFF) {
			max_ppdu_dur.max_ppdu_dur = 0x7FFF;
			skw_warn("The max ppdu dur of idx 1 is 0x7FFF\n");
		}

		break;

	case 2:
	case 3:
	case 4:
	case 5:
		if (max_ppdu_dur.max_ppdu_dur > 0x1FFF) {
			max_ppdu_dur.max_ppdu_dur = 0x1FFF;
			skw_warn("The max ppdu dur of idx 1 is 0x1FFF\n");
		}

		break;

	default:
		break;
	}

	skw_dbg("max ppdu dur idx %d, dur %d\n", max_ppdu_dur.idx, max_ppdu_dur.max_ppdu_dur);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_MAX_PPDU_DUR,
			  &max_ppdu_dur, sizeof(struct skw_max_ppdu_dur));
	if (!ret )
		sprintf(resp, "set max ppdu dur ok ");
	else
		sprintf(resp, "set max ppdu dur failed");

	return ret;
}

static u16 skw_iwpriv_convert_string_to_u (char *ptr, u8 length)
{
	char *buffer = NULL;
	unsigned long num = 0;
	int ret = 0;

	buffer = kmalloc(length + 1, GFP_KERNEL);
	if (!buffer) {
		skw_err("mem alloc failed\n");
		return -ENOMEM;
	}
	memcpy(buffer, ptr, length);
	buffer[length] = '\0';

	ret = kstrtoul(buffer, 0, &num);
	if (ret == 0) {
		if (num <= 0xFFFF) {
			kfree(buffer);
			return (u16)num;
		} else if (num <= 0xFF) {
			kfree(buffer);
			return (u8)num;
		} else {
			skw_err("tred beyond u8 and u16\n");
			kfree(buffer);
			return -ERANGE;
		}
	} else {
		skw_err("transfer fail\n");
		kfree(buffer);
		return -EINVAL;
	}
}

static u32 skw_iwpriv_convert_string_to_u32(char *ptr, unsigned int length)
{
	if (ptr == NULL || length <= 0) {
		skw_err("Invalid input parameters");
		return -EINVAL;
	}

	if (length >= 2 && ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
		char *buffer = NULL;
		unsigned long num = 0;
		int ret = 0;

		buffer = kmalloc(length + 1, GFP_KERNEL);
		if (!buffer) {
			skw_err("mem alloc failed\n");
			return -ENOMEM;
		}
		memcpy(buffer, ptr, length);
		buffer[length] = '\0';

		ret = kstrtoul(buffer, 16, &num);
		if (ret == 0) {
			if (num <= 0xFFFFFFFF) {
				kfree(buffer);
				return (u32)num;
			} else {
				skw_err("value exceeds u32 range");
				kfree(buffer);
				return -ERANGE;
			}
		} else {
			skw_err("hex conversion failed");
			kfree(buffer);
			return -EINVAL;
		}
	} else {
		skw_err("not a valid hex string");
		return -EINVAL;
	}
}

int skw_iwpriv_set_edca_params(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_edca_param_s edca_params = {0};
	u8 parmas_len = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_dbg(" not set edca params\n");
			edca_params.enable = 0;
			goto SETPARA;
		}

		skw_err(" need more edca params\n");
		goto SETFAIL;

	} else if (parmas_len > 1) {
		p = strchr(args, ',');
		if (!p) {
			skw_dbg("flag not found\n");
			return -ENOTSUPP;
		}

		skw_dbg("params found %s\n", p - 1);

		edca_params.enable = simple_strtol(p - 1, NULL, 10);
		if (edca_params.enable != 0 && edca_params.enable != 1) {
			skw_err("flag error %d\n", edca_params.enable);
			return -EINVAL;
		}

		if (edca_params.enable == 0) {
			skw_err("not set edca params\n");
			goto SETPARA;
		}

		skw_dbg("start set edca params ...\n");
	}

	/* parse AC_BE parametes */
	/** AciAifn **/
	//p = p + strlen(",");
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BE AciAifn not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_best_effort.aci_aifn = simple_strtol(p, NULL, 10);
	edca_params.ac_best_effort.aci_aifn = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_best_effort.aci_aifn > 0xFF) {
		skw_warn("AC_BE AciAifn limit is 0xFF\n");
		edca_params.ac_best_effort.aci_aifn = 0xFF;
	}

	skw_dbg("AC_BE Set AciAifn %d\n", edca_params.ac_best_effort.aci_aifn);

	//return 0;
	/** EcWminEcWmax **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BE EcWminEcWmax not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_best_effort.ec_wmin_wmax = simple_strtol(p, NULL, 10);
	edca_params.ac_best_effort.ec_wmin_wmax = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_best_effort.ec_wmin_wmax > 0xFF) {
		skw_dbg("AC_BE EcWminEcWmax limit is 0xFF\n");
		edca_params.ac_best_effort.ec_wmin_wmax = 0xFF;
	}

	skw_dbg("AC_BE Set EcWminEcWmax %d\n", edca_params.ac_best_effort.ec_wmin_wmax);

	/** TxopLimit **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BE TxopLimit not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_best_effort.txop_limit = simple_strtol(p, NULL, 10);
	edca_params.ac_best_effort.txop_limit = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_best_effort.txop_limit > 0xFFFF) {
		skw_dbg("AC_BE TxopLimit limit is 0xFF\n");
		edca_params.ac_best_effort.txop_limit = 0xFFFF;
	}

	skw_dbg("AC_BE Set TxopLimit %d\n", edca_params.ac_best_effort.txop_limit);

	/* parse AC_BG parametes */
	/** AciAifn **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BG AciAifn not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_background.aci_aifn = simple_strtol(p, NULL, 10);
	edca_params.ac_background.aci_aifn = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_background.aci_aifn > 0xFF) {
		skw_warn("AC_BG AciAifn limit is 0xFF\n");
		edca_params.ac_background.aci_aifn = 0xFF;
	}

	skw_dbg("AC_BG Set AciAifn %d\n", edca_params.ac_background.aci_aifn);

	/** EcWminEcWmax **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BG EcWminEcWmax not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_background.ec_wmin_wmax = simple_strtol(p, NULL, 10);
	edca_params.ac_background.ec_wmin_wmax = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_background.ec_wmin_wmax > 0xFF) {
		skw_warn("AC_BG EcWminEcWmax limit is 0xFF\n");
		edca_params.ac_background.ec_wmin_wmax = 0xFF;
	}

	skw_dbg("AC_BG Set EcWminEcWmax %d\n", edca_params.ac_background.ec_wmin_wmax);

	/** TxopLimit **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_BG TxopLimit not found\n");
		return -ENOTSUPP;
	}

	//edca_params.ac_best_effort.txop_limit = simple_strtol(p, NULL, 10);
	edca_params.ac_background.txop_limit = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_background.txop_limit > 0xFFFF) {
		skw_warn("AC_BG TxopLimit limit is 0xFF\n");
		edca_params.ac_background.txop_limit = 0xFFFF;
	}

	skw_dbg("AC_BG Set TxopLimit %d\n", edca_params.ac_best_effort.txop_limit);

	/* parse AC_VIDEO parametes */
	/** AciAifn **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_VIDEO AciAifn not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_video.aci_aifn = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_video.aci_aifn > 0xFF) {
		skw_warn("AC_VIDEO AciAifn limit is 0xFF\n");
		edca_params.ac_video.aci_aifn = 0xFF;
	}

	skw_dbg("AC_VIDEO Set AciAifn %d\n", edca_params.ac_video.aci_aifn);

	/** EcWminEcWmax **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_VIDEO EcWminEcWmax not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_video.ec_wmin_wmax = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_video.ec_wmin_wmax > 0xFF) {
		skw_warn("AC_VIDEO EcWminEcWmax limit is 0xFF\n");
		edca_params.ac_video.ec_wmin_wmax = 0xFF;
	}

	skw_dbg("AC_VIDEO Set EcWminEcWmax %d\n", edca_params.ac_video.ec_wmin_wmax);

	/** TxopLimit **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_VIDEO TxopLimit not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_video.txop_limit = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_video.txop_limit > 0xFFFF) {
		skw_warn("AC_VIDEO TxopLimit limit is 0xFF\n");
		edca_params.ac_video.txop_limit = 0xFFFF;
	}

	skw_dbg("AC_VIDEO Set TxopLimit %d\n", edca_params.ac_video.txop_limit);

	/* parse AC_VOICE parametes */
	/** AciAifn **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_VOICE AciAifn not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_voice.aci_aifn = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_voice.aci_aifn > 0xFF) {
		skw_warn("AC_VOICE AciAifn limit is 0xFF\n");
		edca_params.ac_voice.aci_aifn = 0xFF;
	}

	skw_dbg("AC_VOICE Set AciAifn %d\n", edca_params.ac_voice.aci_aifn);

	/** EcWminEcWmax **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("AC_VOICE EcWminEcWmax not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_voice.ec_wmin_wmax = skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (edca_params.ac_voice.ec_wmin_wmax > 0xFF) {
		skw_warn("AC_VOICE EcWminEcWmax limit is 0xFF\n");
		edca_params.ac_voice.ec_wmin_wmax = 0xFF;
	}

	skw_dbg("AC_VOICE Set EcWminEcWmax %d\n", edca_params.ac_voice.ec_wmin_wmax);

	/** TxopLimit **/
	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("AC_VOICE TxopLimit not found\n");
		return -ENOTSUPP;
	}

	edca_params.ac_voice.txop_limit = simple_strtol(p + 1, NULL, 10);
	if (edca_params.ac_voice.txop_limit > 0xFFFF) {
		skw_warn("AC_VOICE TxopLimit limit is 0xFF\n");
		edca_params.ac_voice.txop_limit = 0xFFFF;
	}

	skw_dbg("AC_VOICE Set TxopLimit %d\n", edca_params.ac_voice.txop_limit);

SETPARA:
	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_EDCA_PARAM,
			  &edca_params, sizeof(struct skw_edca_param_s));
	if (!ret)
		sprintf(resp, "set edca params ok ");
	else
		sprintf(resp, "set edca params failed");

	return ret;

SETFAIL:

	sprintf(resp, "set edca params failed");

	return -EINVAL;
}

static int skw_iwpriv_set_cca_thre_nowifi(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL;
	struct skw_cca_thre_nowifi nowifi = {0};
	int temp_val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	p = args;
	if (!p) {
		skw_err("nowifi not found\n");
		return -ENOTSUPP;
	} else {
		skw_err("nowifi found %s\n", p);
		temp_val = simple_strtol(p, NULL, 10);
		if (temp_val >= 0 || temp_val < -255) {
			skw_warn("The max CCA Thre NOWIFI is 0xFF\n");
			return -EINVAL;
		}
		nowifi.val = 0 - temp_val;
	}

	skw_dbg("nowifi found %s\n", p);

	temp_val = simple_strtol(p, NULL, 10);
	if (temp_val >= 0 || temp_val < -255) {
		skw_warn("The max CCA Thre NOWIFI is 0xFF\n");
		return -EINVAL;
	}

	nowifi.val = 0 - temp_val;

	skw_dbg("Set CCA Thre NOWIFI %d\n", nowifi.val);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id,
			SKW_MIB_SET_CCA_THRE_NOWIFI, &nowifi,
			sizeof(struct skw_cca_thre_nowifi));
	if (!ret)
		sprintf(resp, "set CCA Thre NOWIFI ok ");
	else
		sprintf(resp, "set CCA Thre NOWIFI failed");

	return ret;
}

static int skw_iwpriv_set_cca_thre_11b(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL;
	struct skw_cca_thre_11b cca_11b = {0};
	int temp_val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	p = args;
	if (!p) {
		skw_err("11b not found\n");
		return -ENOTSUPP;
	} else {
		skw_err("11b found %s\n", p);
		temp_val = simple_strtol(p, NULL, 10);
		if (temp_val >= 0 || temp_val < -255) {
			skw_warn("The max CCA Thre 11b is 0xFF\n");
			return -EINVAL;
		}
		cca_11b.val = 0 - temp_val;
	}

	skw_dbg("11b found %s\n", p);

	temp_val = simple_strtol(p, NULL, 10);
	if (temp_val >= 0 || temp_val < -255) {
		skw_warn("The max CCA Thre 11b is 0xFF\n");
		return -EINVAL;
	}

	cca_11b.val = 0 - temp_val;

	skw_dbg("Set CCA Thre 11b %d\n", cca_11b.val);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_CCA_THRE_11B,
			&cca_11b, sizeof(struct skw_cca_thre_11b));
	if (!ret)
		sprintf(resp, "set CCA Thre 11b ok ");
	else
		sprintf(resp, "set CCA Thre 11b failed");

	return ret;
}

static int skw_iwpriv_set_cca_thre_ofdm(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL;
	struct skw_cca_thre_ofdm ofdm = {0};
	int temp_val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	p = args;
	if (!p) {
		skw_err("ofdm not found\n");
		return -ENOTSUPP;
	} else {
		skw_err("ofdm found %s\n", p);
		temp_val = simple_strtol(p, NULL, 10);
		if (temp_val >= 0 || temp_val < -255) {
			skw_warn("The max CCA Thre ofdm is 0xFF\n");
			return -EINVAL;
		}
		ofdm.val = 0 - temp_val;
	}

	skw_dbg("ofdm found %s\n", p);

	temp_val = simple_strtol(p, NULL, 10);
	if (temp_val >= 0 || temp_val < -255) {
		skw_warn("The max CCA Thre ofdm is 0xFF\n");
		return -EINVAL;
	}

	ofdm.val = 0 - temp_val;

	skw_dbg("Set CCA Thre OFDM %d\n", ofdm.val);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_CCA_THRE_OFDM,
			  &ofdm, sizeof(struct skw_cca_thre_ofdm));
	if (!ret)
		sprintf(resp, "set CCA Thre ofdm ok ");
	else
		sprintf(resp, "set CCA Thre ofdm failed");

	return ret;
}

static int skw_is_value_in_enum(u8 value)
{
	switch (value) {
	case LEGA_11B_SHORT_2M:
	case LEGA_11B_SHORT_55M:
	case LEGA_11B_SHORT_11M:
	case LEGA_11B_LONG_1M:
	case LEGA_11B_LONG_2M:
	case LEGA_11B_LONG_55M:
	case LEGA_11B_LONG_11M:
	case OFDM_6M:
	case OFDM_9M:
	case OFDM_12M:
	case OFDM_18M:
	case OFDM_24M:
	case OFDM_36M:
	case OFDM_48M:
	case OFDM_54M:
	case HT_MCS_0:
	case HT_MCS_1:
	case HT_MCS_2:
	case HT_MCS_3:
	case HT_MCS_4:
	case HT_MCS_5:
	case HT_MCS_6:
	case HT_MCS_7:
	case HT_MCS_8:
	case HT_MCS_9:
	case HT_MCS_10:
	case HT_MCS_11:
	case HT_MCS_12:
	case HT_MCS_13:
	case HT_MCS_14:
	case HT_MCS_15:
	case HT_MCS_16:
	case HT_MCS_17:
	case HT_MCS_18:
	case HT_MCS_19:
	case HT_MCS_20:
	case HT_MCS_21:
	case HT_MCS_22:
	case HT_MCS_23:
	case HT_MCS_24:
	case HT_MCS_25:
	case HT_MCS_26:
	case HT_MCS_27:
	case HT_MCS_28:
	case HT_MCS_29:
	case HT_MCS_30:
	case HT_MCS_31:
	case VHT_MCS_0:
	case VHT_MCS_1:
	case VHT_MCS_2:
	case VHT_MCS_3:
	case VHT_MCS_4:
	case VHT_MCS_5:
	case VHT_MCS_6:
	case VHT_MCS_7:
	case VHT_MCS_8:
	case VHT_MCS_9:
	case HE_MCS_0:
	case HE_MCS_1:
	case HE_MCS_2:
	case HE_MCS_3:
	case HE_MCS_4:
	case HE_MCS_5:
	case HE_MCS_6:
	case HE_MCS_7:
	case HE_MCS_8:
	case HE_MCS_9:
	case HE_MCS_10:
	case HE_MCS_11:
	case ER_NDCM_1SS_242TONE_MCS0:
	case ER_NDCM_1SS_242TONE_MCS1:
	case ER_NDCM_1SS_242TONE_MCS2:
	case ER_NDCM_1SS_106TONE_MCS0:
	case ER_DCM_1SS_242TONE_MCS0:
	case ER_DCM_1SS_242TONE_MCS1:
	case ER_DCM_1SS_106TONE_MCS0:
	case NER_DCM_1SS_MCS0:
	case NER_DCM_1SS_MCS1:
	case NER_DCM_1SS_MCS3:
	case NER_DCM_1SS_MCS4:
	case NER_DCM_2SS_MCS0:
	case NER_DCM_2SS_MCS1:
	case NER_DCM_2SS_MCS3:
	case NER_DCM_2SS_MCS4:
		return 1;

	default:
		return 0;
	}
}

static int skw_iwpriv_set_force_rts_rate(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_force_rts_rate rts_rate = {0};
	u8 parmas_len = 0, val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	/* parse rts rate flag */
	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_warn(" not force set rts rate\n");
			rts_rate.enable = 0;
			goto SETPARA;
		}

		skw_warn(" need more rate params\n");
		goto SETFAIL;

	} else if (parmas_len > 1) {
		p = strchr(args, ',');
		if (!p) {
			skw_err("flag not found\n");
			return -ENOTSUPP;
		}

		skw_dbg("params found %s\n", p - 1);

		rts_rate.enable = simple_strtol(p - 1, NULL, 10);
		if (rts_rate.enable != 0 && rts_rate.enable != 1) {
			skw_err("flag error %d\n", rts_rate.enable);
			return -EINVAL;
		}

		if (rts_rate.enable == 0) {
			skw_err("not set rts_rate params\n");
			goto SETPARA;
		}

		skw_err("start set rts_rate params ...\n");
	}

	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("rts rate 2.4G not found\n");
		return -ENOTSUPP;
	}

	val = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (!skw_is_value_in_enum(val)) {
		skw_warn("invalid 2.4G rate %d\n", val);
		return -EINVAL;
	}

	rts_rate.rts_rate_24G = val;

	skw_dbg("rts rate 2.4G: %d\n", rts_rate.rts_rate_24G);

	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("rts rate 5G not found\n");
		return -ENOTSUPP;
	}

	val = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (!skw_is_value_in_enum(val)) {
		skw_warn("invalid 5G rate %d\n", val);
		return -EINVAL;
	}

	rts_rate.rts_rate_5G = val;
	skw_dbg("rts_rate_5G: %d\n", rts_rate.rts_rate_5G);

SETPARA:
	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_FORCE_RTS_RATE,
			  &rts_rate, sizeof(struct skw_force_rts_rate));
	if (!ret)
		sprintf(resp, "set force rts rate ok ");
	else
		sprintf(resp, "set force rts rate failed");

	return ret;

SETFAIL:
	sprintf(resp, "set force rts rate failed");

	return -EINVAL;
}

static int skw_iwpriv_set_force_rx_rsp_rate(struct skw_iface *iface, void *param,
	char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_force_rx_rsp_rate rate = {0};
	u8 parmas_len = 0, val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("skw_iwpriv_set_force_rts_rate args %s\n", args);

	/* parse rts rate flag */
	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_warn(" not force set rx rsp rate\n");
			goto SETPARA;
		} else if (ret == 1) {
			skw_warn(" need more rate params\n");
			goto SETFAIL;
		} else {
			goto SETFAIL;
		}
	} else if (parmas_len > 1) {
		p = strchr(args, ',');
		if (!p) {
			skw_err("flag not found\n");
			return -ENOTSUPP;
		}

		skw_dbg("params found %s\n", p - 1);

		rate.enable = simple_strtol(p - 1, NULL, 10);
		if (rate.enable != 0 && rate.enable != 1) {
			skw_err("flag error %d\n", rate.enable);
			return -EINVAL;
		}

		if (rate.enable == 0) {
			skw_err("not set rx rsp rate params\n");
			goto SETPARA;
		}

		skw_dbg("start set rx rsp rate params ...\n");
	}

	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("11b long not found\n");
		return -ENOTSUPP;
	}

	val = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (!skw_is_value_in_enum(val)) {
		skw_warn("invalid 11b long rate %d\n", val);
		return -EINVAL;
	}

	rate.rx_rsp_rate_11b_long = val;
	skw_dbg("11b long rate: %d\n", rate.rx_rsp_rate_11b_long);

	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("11b short not found\n");
		return -ENOTSUPP;
	}

	val = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (!skw_is_value_in_enum(val)) {
		skw_warn("invalid 11b short rate %d\n", val);
		return -EINVAL;
	}

	rate.rx_rsp_rate_11b_short = val;
	skw_dbg("11b short rate: %d\n", rate.rx_rsp_rate_11b_short);

	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("ofdm not found\n");
		return -ENOTSUPP;
	}

	val = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	if (!skw_is_value_in_enum(val)) {
		skw_warn("invalid ofdm rate %d\n", val);
		return -EINVAL;
	}

	rate.rx_rsp_rate_ofdm = val;
	skw_dbg("ofdm: %d\n", rate.rx_rsp_rate_ofdm);

SETPARA:
	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_FORCE_RX_RSP_RATE,
			  &rate, sizeof(struct skw_force_rx_rsp_rate));
	if (!ret)
		sprintf(resp, "set force rx rsp rate ok ");
	else
		sprintf(resp, "set force rx rsp rate failed");

	return ret;

SETFAIL:
	sprintf(resp, "set force rts rate failed");

	return -EINVAL;
}

static int skw_iwpriv_set_scan_time(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_set_scan_time time = {0};
	u8 parmas_len = 0;
	u16 val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args: %s\n", args);

	/* parse rts rate flag */
	parmas_len = strlen(args);

	p = args;
	q = strchr(args, ',');
	if (!q) {
		skw_err("parameter error\n");
		return -ENOTSUPP;
	}

	skw_dbg("params found %s\n", p - 1);
	val = skw_iwpriv_convert_string_to_u(p, q - p);
	if (val > 0xFF) {
		skw_warn("active dwell limit to 0xFF\n");
		time.active_dwell_time = 0xFF;
	} else {
		time.active_dwell_time = val;
	}

	skw_dbg("active dwell time: %d\n", time.active_dwell_time);

	p = q + strlen(",");
	if (!p) {
		skw_warn("bypass active scan auto time flag not found\n");
		return -ENOTSUPP;
	}

	val = simple_strtol(p, NULL, 10);
	if (val != 0 && val != 1) {
		skw_err("flag error %d\n", val);
		return -EINVAL;
	}

	time.bypass_active_acan_auto_time = val;

	skw_dbg("bypass active scan auto time: %d\n", time.bypass_active_acan_auto_time);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_SCAN_TIME,
			  &time, sizeof(struct skw_set_scan_time));
	if (!ret)
		sprintf(resp, "set scan time ok ");
	else
		sprintf(resp, "set scan time failed");

	return ret;
}

static int skw_iwpriv_set_tcpd_wakeup_host(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_set_tcpd_wakeup_host set_flag = {0};
	u8 parmas_len = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("set_tcpd_wakeup_host args %s\n", args);

	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_dbg("flag: %d\n", ret);
			set_flag.enable = 0;
		} else if (ret == 1) {
			skw_dbg("flag: %d\n", ret);
			set_flag.enable = 1;
		} else {
			goto SETFAIL;
		}
	} else if (parmas_len > 1) {
		skw_err("params error\n");
		goto SETFAIL;
	}

	skw_dbg("wakeup host: %d\n", set_flag.enable);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_TCP_DISCONN_WAKEUP_HOST,
			  &set_flag, sizeof(struct skw_set_tcpd_wakeup_host));
	if (!ret)
		sprintf(resp, "set tcp disc wakeup host ok ");
	else
		sprintf(resp, "set tcp disc wakeup host failed");

	return ret;

SETFAIL:
	sprintf(resp, "set tcp disc wakeup host failed(param error)");

	return ret;
}

static int skw_iwpriv_set_rc_min_rate(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_set_rate_control_min_rate rate = {0};
	u8 parmas_len = 0, val = 0;
	char *p = NULL;

	if (!args)
		return -EINVAL;

	skw_dbg("set_rc_min_rate args %s\n", args);

	parmas_len = strlen(args);
	if (!parmas_len)
		return -EINVAL;

	p = args;
	val = (u8)skw_iwpriv_convert_string_to_u(p, parmas_len);
	if (!skw_is_value_in_enum(val)) {
		skw_dbg("val not valid %d\n", val);
		goto SETFAIL;
	}

	rate.rstrict_min_rate = val;
	skw_dbg("rc min rate: %d\n", rate.rstrict_min_rate);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_RATE_CTRL_MIN_RATE,
			  &rate, sizeof(struct skw_set_rate_control_min_rate));
	if (!ret)
		sprintf(resp, "set rc min rate ok ");
	else
		sprintf(resp, "set set rc min rate failed");

	return ret;

SETFAIL:
	sprintf(resp, "set rc min rate failed (value not support)");

	return ret;
}

static int skw_iwpriv_set_rc_rate_change(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_set_rate_control_rate_change rate = {0};

	if (!args)
		return -EINVAL;

	skw_dbg("set_rc_rate_change args %s\n", args);

	//1/5 up rate class num
	p = args;
	q = strchr(args, ',');
	if (!p || !q) {
		skw_err("up rate class num not found\n");
		return -ENOTSUPP;
	}

	rate.up_rate_class_num = (u8)skw_iwpriv_convert_string_to_u(p, q - p);

	skw_dbg("up_rate_class_num: %d\n", rate.up_rate_class_num);

	//2/5 down rate class num
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("down rate class num not found\n");
		return -ENOTSUPP;
	}

	rate.down_rate_class_num = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("down_rate_class_num: %d\n", rate.down_rate_class_num);

	//3/5 hw rty limit
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("hw rty limit not found\n");
		return -ENOTSUPP;
	}

	rate.hw_rty_limit = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	skw_dbg("hw_rty_limit: %d\n", rate.hw_rty_limit);

	//4/5 per rate hw rty limit
	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("per rate hw rty limit not found\n");
		return -ENOTSUPP;
	}

	rate.per_rate_hw_rty_limit = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("per rate hw_rty_limit: %d\n", rate.per_rate_hw_rty_limit);

	//5/5 per rate probe hw rty limit
	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("per rate probe hw rty limit not found\n");
		return -ENOTSUPP;
	}

	rate.per_rate_probe_hw_rty_limit = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("per rate probe hw_rty_limit: %d\n", rate.per_rate_probe_hw_rty_limit);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_RATE_CTRL_RATE_CHANGE_PARAM,
			  &rate, sizeof(struct skw_set_rate_control_rate_change ));
	if (!ret)
		sprintf(resp, "set force rx rsp rate ok ");
	else
		sprintf(resp, "set force rx rsp rate failed");

	return ret;
}

static int skw_iwpriv_set_rc_spe_rate(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_set_rate_control_special_rate rate = {0};
	u8 parmas_len = 0, val = 0;
	char *p = NULL;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (!parmas_len)
		return -EINVAL;

	p = args;
	val = (u8)skw_iwpriv_convert_string_to_u(p, parmas_len);
	if (!skw_is_value_in_enum(val)) {
		skw_dbg("val not valid %d\n", val);
		goto SETFAIL;
	}

	rate.special_frm_rate = val;
	skw_dbg("rc special rate: %d\n", rate.special_frm_rate);

	ret = skw_set_mib(iface->wdev.wiphy, iface->id, SKW_MIB_SET_RATE_CTRL_SPECIAL_FRM_RATE,
			  &rate, sizeof(struct skw_set_rate_control_special_rate));
	if (!ret)
		sprintf(resp, "set rc special rate ok ");
	else
		sprintf(resp, "set rc special rate failed");

	return ret;

SETFAIL:
	sprintf(resp, "set rc spe rate failed (value not support)");

	return ret;
}

static int skw_generic_tlv_operation(struct wiphy *wiphy, struct net_device *dev,
		void *param, size_t param_len, enum SKW_MIB_ID tlv_type)
{
	int ret = 0;
	u16 *plen;
	struct skw_tlv_conf conf;
	struct skw_tlv_get_assign_addr_rsp resp = {0};

	ret = skw_tlv_alloc(&conf, 512, GFP_KERNEL);
	if (ret) {
		skw_err("alloc tlv failed\n");
		return ret;
	}

	plen = skw_tlv_reserve(&conf, 2);
	if (!plen) {
		skw_err("reserve tlv failed\n");
		skw_tlv_free(&conf);
		return -ENOMEM;
	}

	if (skw_tlv_add(&conf, tlv_type, param, param_len)) {
		skw_err("add tlv failed\n");
		skw_tlv_free(&conf);
		return -EINVAL;
	}

	if (conf.total_len) {
		*plen = conf.total_len;
		if (tlv_type == SKW_MIB_GET_ASSIGN_ADDR_VAL_E) {
			ret = skw_send_msg(wiphy, dev, SKW_CMD_SET_MIB, conf.buff,
					conf.total_len, &resp, sizeof(resp));

			if (ret) {
				skw_err("failed, ret: %d\n", ret);
				goto SETFAIL;
			}
			skw_dbg("read done val: 0x%08x\n", resp.val);
		} else {
			ret = skw_send_msg(wiphy, dev, SKW_CMD_SET_MIB, conf.buff,
					conf.total_len, NULL, 0);
			if (ret) {
				skw_err("failed, ret: %d\n", ret);
				goto SETFAIL;
			}
		}
	}
SETFAIL:
	skw_tlv_free(&conf);
	return ret;
}

static int skw_set_tx_lifetime(struct wiphy *wiphy, struct net_device *dev,
			struct skw_tlv_set_tx_lifetime *lifetime)
{
	return skw_generic_tlv_operation(wiphy, dev, lifetime,
			sizeof(struct skw_tlv_set_tx_lifetime),
			SKW_MIB_SET_TX_LIFETIME);
}

static int skw_iwpriv_set_tx_lifetime(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_tlv_set_tx_lifetime lftm = {0};
	u8 parmas_len = 0;
	char *p = NULL;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (!parmas_len)
		return -EINVAL;

	p = args;
	lftm.lifetime = skw_iwpriv_convert_string_to_u(p, parmas_len);

	skw_dbg("set tx lifetime: %d\n", lftm.lifetime);

	ret = skw_set_tx_lifetime(iface->wdev.wiphy, iface->ndev, &lftm);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

static int skw_set_tx_retry_cnt(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_retry_cnt *rtycnt)
{
	return skw_generic_tlv_operation(wiphy, dev, rtycnt,
			sizeof(struct skw_tlv_set_retry_cnt),
			SKW_MIB_SET_TX_RETRY_CNT);
}

static int skw_iwpriv_set_tx_retry_cnt(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_tlv_set_retry_cnt cnt = {0};
	u8 parmas_len = 0;
	char *p = NULL;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (!parmas_len)
		return -EINVAL;

	p = args;
	cnt.rtycnt = (u8)skw_iwpriv_convert_string_to_u(p, parmas_len);

	skw_dbg("set tx retry cnt: %d\n", cnt.rtycnt);

	ret = skw_set_tx_retry_cnt(iface->wdev.wiphy, iface->ndev, &cnt);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

static int skw_set_tx_rts_thrd(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_tx_rts_thrd *thrd)
{
	return skw_generic_tlv_operation(wiphy, dev, thrd,
			sizeof(struct skw_tlv_set_tx_rts_thrd),
			SKW_MIB_SET_TX_RTS_THRD);
}

static int skw_iwpriv_set_tx_rts_thrd(struct skw_iface *iface, void *param,
	char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_tlv_set_tx_rts_thrd thrd = {0};
	u8 parmas_len = 0;
	char *p = NULL;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (!parmas_len)
		return -EINVAL;

	p = args;
	thrd.rts_thrd = skw_iwpriv_convert_string_to_u(p, parmas_len);

	skw_dbg("set tx rts thrd: %d\n", thrd.rts_thrd);

	ret = skw_set_tx_rts_thrd(iface->wdev.wiphy, iface->ndev, &thrd);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

static int skw_set_rx_sepcial_80211_frame(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_rx_special_80211_frame *frm)
{
	return skw_generic_tlv_operation(wiphy, dev, frm,
			sizeof(struct skw_tlv_set_rx_special_80211_frame),
			SKW_MIB_SET_FORCE_RX_SPECIAL_80211FRAME);
}

static int skw_iwpriv_set_rx_sepcial_80211_frame(struct skw_iface *iface, void *param,
	char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_tlv_set_rx_special_80211_frame frm = {0};
	u8 parmas_len = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("set_rx_sepcial_80211_frame args %s\n", args);

	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_warn("disable rx\n");
			frm.en = 0;
			goto SETPARA;
		} else if (ret == 1) {
			skw_warn("need more params\n");
			goto SETFAIL;
		} else {
			goto SETFAIL;
		}
	} else if (parmas_len > 1) {
		p = args;
		q = strchr(args, ',');
		if (!p || !q) {
			skw_err("flag not found\n");
			return -ENOTSUPP;
		}

		frm.en = (u8)skw_iwpriv_convert_string_to_u(p, q - p);
		if (frm.en != 0 && frm.en != 1) {
			skw_err("flag error %d\n", frm.en);
			return -EINVAL;
		}

		if (frm.en == 0) {
			skw_warn("disable rx\n");
			frm.en = 0;
			goto SETPARA;
		}

		skw_err("start set type and sub type params ...\n");
	}

	p = q;
	q = strchr(p + 1, ',');
	if (!p || !q) {
		skw_err("param error\n");
		return -ENOTSUPP;
	}

	frm.type = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("type: %d\n", frm.type);

	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("sub type not found\n");
		return -ENOTSUPP;
	}

	frm.sub_type = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("sub type: %d\n", frm.sub_type);

SETPARA:
	ret = skw_set_rx_sepcial_80211_frame(iface->wdev.wiphy, iface->ndev, &frm);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;

SETFAIL:

	sprintf(resp, "set failed");

	return -EINVAL;
}

static int skw_set_rx_update_nav(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_rx_update_nav *nav)
{
	return skw_generic_tlv_operation(wiphy, dev, nav,
			sizeof(struct skw_tlv_set_rx_update_nav),
			SKW_MIB_SET_FORCE_RX_UPDATE_NAV);
}

static int skw_iwpriv_set_rx_update_nav(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_tlv_set_rx_update_nav nav = {0};
	int temp_val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args %s\n", args);

	p = args;
	q = strchr(args, ',');
	if(!p || !q)
	{
		skw_err("parameter error\n");
		return -ENOTSUPP;
	} else {
		skw_dbg("params found %s\n", p);
		temp_val = simple_strtol(p, NULL, 10);
		if (temp_val >= 0 || temp_val < -255) {
			skw_warn("parameter error %d\n", temp_val);
			return -EINVAL;
		}
		nav.intra_rssi = 0x80 | (0 - temp_val);
	}
	skw_dbg("intra rssi: %d\n", nav.intra_rssi);

	p = q;
	q = strchr(p + 1, ',');
	if(!p || !p)
	{
		skw_err("parameter error\n");
		return -ENOTSUPP;
	} else {
		skw_dbg("params found %s\n", p + 1);
		temp_val = simple_strtol(p + 1, NULL, 10);
		if (temp_val >= 0 || temp_val < -255) {
			skw_warn("parameter error %d\n", temp_val);
			return -EINVAL;
		}
		nav.basic_rssi = 0x80 | (0 - temp_val);
	}
	skw_dbg("basic rssi: %d\n", nav.basic_rssi);

	p = q;
	p = q + strlen(",");
	if(!p)
	{
		skw_warn("nav max time not found\n");
		return -ENOTSUPP;
	} else {
		 nav.nav_max_time = simple_strtol(p, NULL, 10);
	}
	skw_dbg("nav max time: %d\n", nav.nav_max_time);

	ret = skw_set_rx_update_nav(iface->wdev.wiphy, iface->ndev, &nav);

	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

//TLV 70
static int skw_set_apgo_timap(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_apgo_timap *timap)
{
	return skw_generic_tlv_operation(wiphy, dev, timap,
			sizeof(struct skw_tlv_set_apgo_timap),
			SKW_MIB_SET_APGO_TIMMAP);
}

static int skw_iwpriv_set_apgo_timap(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_tlv_set_apgo_timap timap = {0};

	if (!args)
		return -EINVAL;

	skw_dbg("set_apgo_timap args %s\n", args);

	p = args;
	q = strchr(args, ',');
	if (!p || !q) {
		skw_err("parameter error\n");
		return -ENOTSUPP;
	}

	timap.dtimforce0 = (u8)skw_iwpriv_convert_string_to_u(p, q - p);
	skw_dbg("dtim force0: %d\n", timap.dtimforce0);

	p = q;
	q = strchr(p + 1, ',');
	if (!p || !p) {
		skw_err("parameter error\n");
		return -ENOTSUPP;
	}

	timap.dtimforce1 = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);

	skw_dbg("dtim force1: %d\n", timap.dtimforce1);

	p = q;
	q = strchr(p + 1, ',');
	if (!p || !p) {
		skw_err("parameter error\n");
		return -ENOTSUPP;
	}

	timap.timforce0 = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	skw_dbg("tim force0: %d\n", timap.timforce0);

	p = q;
	q = strchr(p + 1, ',');
	if (!p) {
		skw_err("parameter error\n");
		return -ENOTSUPP;
	}

	timap.timforce1 = (u8)skw_iwpriv_convert_string_to_u(p + 1, q - p - 1);
	skw_dbg("tim force1: %d\n", timap.timforce1);

	ret = skw_set_apgo_timap(iface->wdev.wiphy, iface->ndev, &timap);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

//TLV 71
static int skw_set_dbdc_disable(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_dbdc_disable *flag)
{
	return skw_generic_tlv_operation(wiphy, dev, flag,
			sizeof(struct skw_tlv_set_dbdc_disable),
			SKW_MIB_SET_DBDC_DISABLE);
}

static int skw_iwpriv_set_dbdc_disable(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	struct skw_tlv_set_dbdc_disable set_flag = {0};
	u8 parmas_len = 0;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (parmas_len == 1) {
		ret = simple_strtol(args, NULL, 10);
		if (!ret) {
			skw_dbg("flag: %d\n", ret);
			set_flag.disable = 0;
		} else if (ret == 1) {
			skw_dbg("flag: %d\n", ret);
			set_flag.disable = 1;
		} else {
			goto SETFAIL;
		}
	} else if (parmas_len > 1) {
		skw_err("params error\n");
		goto SETFAIL;
	}

	skw_dbg("dbdc disable: %d\n", set_flag.disable);

	ret = skw_set_dbdc_disable(iface->wdev.wiphy, iface->ndev, &set_flag);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;

SETFAIL:
	sprintf(resp, "set failed(param error)");

	return ret;
}

//TLV 150
static int skw_set_assign_address_val(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_set_assign_addr_val *param)
{
	return skw_generic_tlv_operation(wiphy, dev, param,
			sizeof(struct skw_tlv_set_assign_addr_val),
			SKW_MIB_SET_ASSIGN_ADDRESS_VAL);
}

static int skw_iwpriv_set_assign_address_val(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_tlv_set_assign_addr_val params = {0};
	u8 parmas_len = 0, remain_len = 0;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	skw_warn("args %s\n", args);

	p = args;
	q = strchr(args, ',');
	if (!p || !q) {
		skw_err("params error\n");
		return -ENOTSUPP;
	}

	params.addr = skw_iwpriv_convert_string_to_u32(p, q - p);
	skw_dbg("addr: 0x%08x\n", params.addr);

	p = q;
	remain_len = parmas_len - (q - p) - 1;
	if (!p) {
		skw_err("param error\n");
		return -ENOTSUPP;
	}

	params.val = skw_iwpriv_convert_string_to_u32(p + 1, remain_len);
	skw_dbg("val: 0x%08x\n", params.val);

	ret = skw_set_assign_address_val(iface->wdev.wiphy, iface->ndev, &params);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

//TLV 250
static int skw_read_addr(struct wiphy *wiphy, struct net_device *dev,
		struct skw_tlv_get_assign_addr *param)
{
	return skw_generic_tlv_operation(wiphy, dev, param,
			sizeof(struct skw_tlv_get_assign_addr),
			SKW_MIB_GET_ASSIGN_ADDR_VAL_E);
}

static int skw_iwpriv_read_addr(struct skw_iface *iface, void *param,
		char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL;
	struct skw_tlv_get_assign_addr params = {0};
	u8 parmas_len = 0;

	if (!args)
		return -EINVAL;

	parmas_len = strlen(args);
	if (!parmas_len || parmas_len > 10) {
		skw_err("params error\n");
		return -ENOTSUPP;
	}

	skw_dbg("args %s\n", args);

	p = args;
	if (!p) {
		skw_err("params error\n");
		return -ENOTSUPP;
	}

	params.addr = skw_iwpriv_convert_string_to_u32(p, parmas_len);
	skw_dbg("addr: 0x%08x\n", params.addr);

	ret = skw_read_addr(iface->wdev.wiphy, iface->ndev, &params);
	if (!ret)
		sprintf(resp, "set ok ");
	else
		sprintf(resp, "set failed");

	return ret;
}

//TLV 39 ageout thrd
//
static int skw_set_ageout_thrd(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_ageout_thrd *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_ageout_thrd), SKW_MIB_SET_AGEOUT_THOLD);
}

static int skw_iwpriv_set_ageout_thrd(struct skw_iface *iface, void *param,
	char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	struct skw_tlv_set_ageout_thrd thrd = {0};
	u16 temp_val = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args %s\n", args);

	p = args;
	q = strchr(args, ',');
	if (!p || !q) {
		skw_err("param err\n");
		return -ENOTSUPP;
	}

	temp_val = simple_strtol(p, NULL, 10);
	if (temp_val >= 0xFF) {
		skw_warn("parameter out of range (0 ~ 255) %d\n", temp_val);
		thrd.ageout_kick_thrd = 0xFF;
	} else {
		thrd.ageout_kick_thrd  = temp_val;
	}

	skw_dbg("tbtt cnt, kick out %d\n", thrd.ageout_kick_thrd);

	p = q;
	p = q + strlen(",");
	if (!p) {
		skw_warn("param err\n");
		return -ENOTSUPP;
	}

	temp_val = simple_strtol(p, NULL, 10);
	if (temp_val >= 0xFF) {
		skw_warn("parameter out of range (0 ~ 255) %d\n", temp_val);
		thrd.ageout_keep_alive_thrd = 0xFF;
	} else {
		thrd.ageout_keep_alive_thrd  = temp_val;
	}

	skw_dbg("keep alive send null thrd: %d\n", thrd.ageout_keep_alive_thrd);

	ret = skw_set_ageout_thrd(iface->wdev.wiphy, iface->ndev, &thrd);

	if (!ret)
		sprintf(resp, "set ok");
	else
		sprintf(resp, "set failed");

	return ret;

}

//tlv 42
static int skw_set_reported_cqm_interval(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_report_cqm_rssi_low_itvl *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_report_cqm_rssi_low_itvl), SKW_MIB_SET_REPORT_CQM_RSSI_LOW_INT);
}

//tlv 54 ~ 57
static int skw_set_ap_new_channel(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_ap_new_channel *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_ap_new_channel), SKW_MIB_SET_AP_NEW_CHAN);
}

static int skw_set_tx_retry_limit_en(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_tx_retry_limit_en *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_tx_retry_limit_en), SKW_MIB_SET_TX_RETRY_LIMIT_EN);
}

static int skw_partial_twt_sched(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_partial_twt_sched *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_partial_twt_sched), SKW_MIB_SET_PARTIAL_TWT_SCHED);
}

static int skw_set_thm_thre(struct wiphy *wiphy, struct net_device *dev, struct skw_tlv_set_thm_thrd *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_thm_thrd), SKW_MIB_SET_THM_THRD);
}

static int skw_set_normal_scan_with_acs(struct wiphy *wiphy, struct net_device *dev,
										struct skw_tlv_set_normal_scan_with_acs *params)
{
	return skw_generic_tlv_operation(wiphy, dev, params, sizeof(struct skw_tlv_set_normal_scan_with_acs),
									SKW_MIB_SET_NORMAL_SCAN_WITH_ACS);
}

static int skw_set_scan_send_probe_req_cnt(struct wiphy *wiphy, struct net_device *dev,
		u8 cnt)
{
	return skw_generic_tlv_operation(wiphy, dev, &cnt, 1,
			SKW_MIB_SET_SCAN_SEND_PROBE_REQ_CNT);
}

static int skw_iwpriv_set_params(struct skw_iface *iface, void *param,
								char *args, char *resp, int resp_len)
{
	int ret = 0;
	char *p = NULL, *q = NULL;
	u32 params[16] = {0};
	u32 params_num = 0;
	struct skw_tlv_set_ap_new_channel new_chan = {0};
	struct skw_tlv_set_tx_retry_limit_en txr_limit = {0};
	struct skw_tlv_set_report_cqm_rssi_low_itvl itvl = {0};
	struct skw_tlv_set_thm_thrd thrd = {0};
	struct skw_tlv_set_partial_twt_sched twt_params = {0};
	struct skw_tlv_set_normal_scan_with_acs normal_scan_params = {0};
	u8 send_probe_req_cnt = 0;

	if (!args)
		return -EINVAL;

	skw_dbg("args %s\n", args);

	p = args;

	while (p) {
		params[params_num] = simple_strtol(p, NULL, 10);
		params_num++;

		q = strchr(p, ',');
		if (!q) {
			break;
		}

		p = q + 1; /* move past the comma */
	}

	if (params_num < 2) {
		skw_warn("params error, at least 2\n");
		return -ENOTSUPP;
	}

	switch (params[0]) {
		case 1:
			if (params_num != 6) {
				skw_warn("params error, need 6 totally\n");
				return -EINVAL;
			}

			new_chan.chan = params[1];
			new_chan.center_chan = params[2];
			new_chan.center_two_chan = params[3];
			new_chan.bw = params[4];
			new_chan.band = params[5];

			ret = skw_set_ap_new_channel(iface->wdev.wiphy, iface->ndev, &new_chan);
			skw_dbg("set ap new chan done\n");

			break;

		case 2:
			if (params_num != 2) {
				skw_warn("params error, need 2 totally\n");
				return -EINVAL;
			}

			skw_set_mib_bool(iface->wdev.wiphy, iface->id,
					 SKW_MIB_SET_11R_TLV_ID, !!params[1]);

			skw_dbg("set 11r mib done\n");
			break;

		case 3:
			if (params_num != 4) {
				skw_warn("params error, need 4 totally\n");
				return -EINVAL;
			}

			txr_limit.short_retry_check_en = !!params[1];
			txr_limit.long_retry_check_en = !!params[2];
			txr_limit.ampdu_retry_check_en = !!params[3];

			ret = skw_set_tx_retry_limit_en(iface->wdev.wiphy, iface->ndev,&txr_limit);
			skw_dbg("set tx retry limit done\n");

			break;

		case 4:
			if (params_num != 3) {
				skw_warn("params error, need 3 totally\n");
				return -EINVAL;
			}

			itvl.report_cqm_low_intvl_min_dur = params[1];
			itvl.report_cqm_low_intvl_max_dur = params[2];

			ret = skw_set_reported_cqm_interval(iface->wdev.wiphy, iface->ndev,&itvl);
			skw_dbg("set report cqm interval done\n");
			break;

		case 5:
			if (params_num != 3) {
				skw_warn("params error, need 3 totally\n");
				return -EINVAL;
			}

			thrd.thm_high_thrd_tx_suspend = (s16)params[1];
			thrd.thm_low_thrd_tx_resume = (s16)params[2];

			ret = skw_set_thm_thre(iface->wdev.wiphy, iface->ndev, &thrd);
			skw_dbg("set thm thre done\n");

			break;

		case 6:
			if (params_num != 7) {
				skw_warn("params error, need 7 totally\n");
				return -EINVAL;
			}

			twt_params.en = params[1];
			twt_params.start_time_l = params[2];
			twt_params.start_time_h = params[3];
			twt_params.interval = params[4];
			twt_params.duration = params[5];
			twt_params.duration_unit = params[6];
			twt_params.sub_type = params[7];

			ret = skw_partial_twt_sched(iface->wdev.wiphy, iface->ndev, &twt_params);
			skw_dbg("set partial twt sched done\n");

			break;
		case 59:
			if (params_num != 2) {
				skw_warn("params error, need 2 totally\n");
				return -EINVAL;
			}

			normal_scan_params.en = params[1];
			ret = skw_set_normal_scan_with_acs(iface->wdev.wiphy, iface->ndev, &normal_scan_params);
			skw_dbg("set normal scan with acs %d done\n", normal_scan_params.en);

			break;
		case 62:
			if (params_num != 2) {
				skw_warn("params error, need 2 totally    \n");
				return -EINVAL;
			}
			send_probe_req_cnt = params[1];
			ret = skw_set_scan_send_probe_req_cnt(iface->wdev.wiphy, iface->ndev, send_probe_req_cnt);
			skw_dbg("set scan send probe req cnt %d done\n", send_probe_req_cnt);
			break;

		default:
			return -ENOTSUPP;
	}

	if (!ret)
		snprintf(resp, resp_len, "set ok");
	else
		snprintf(resp, resp_len, "set failed");

	return ret;
}

static struct skw_iwpriv_cmd skw_iwpriv_set_cmds[] = {
	/* keep first */
	{"help", skw_iwpriv_help, "usage"},
	{"bandcfg", skw_iwpriv_set_bandcfg, "bandcfg=0/1/2"},
	{"mppdudur", skw_iwpriv_set_max_ppdu_dur, "mppdudur=0~5,1~65535"},
	{"edca", skw_iwpriv_set_edca_params, "edca=0,x(13 total)"},
	{"ccanowifi", skw_iwpriv_set_cca_thre_nowifi, "ccanowifi=-u8"},
	{"cca11b", skw_iwpriv_set_cca_thre_11b, "cca11b=-u8"},
	{"ccaofdm", skw_iwpriv_set_cca_thre_ofdm, "ccaofdm=-u8"},
	{"rtsrate", skw_iwpriv_set_force_rts_rate, "rtsrate=u8,u8,u8"},
	{"rxrsprate", skw_iwpriv_set_force_rx_rsp_rate, "rxrsprate=u8,u8,u8,u8"},
	{"scantime", skw_iwpriv_set_scan_time, "scantime=u8,u8"},
	{"tcpdwhost", skw_iwpriv_set_tcpd_wakeup_host, "tcpdwhost=u8"},
	{"rcminrate", skw_iwpriv_set_rc_min_rate, "rcminrate=u8"},
	{"rcratechg", skw_iwpriv_set_rc_rate_change, "rcratechg=u8,u8,u8,u8,u8"},
	{"rcsperate", skw_iwpriv_set_rc_spe_rate, "rcsperate=u8"},
	{"txlftm", skw_iwpriv_set_tx_lifetime, "txlftm=u8"},
	{"txrtycnt", skw_iwpriv_set_tx_retry_cnt, "txrtycnt=u8"},
	{"txrtsthrd", skw_iwpriv_set_tx_rts_thrd, "txrtsthrd=u8"},
	{"rxsped11frm", skw_iwpriv_set_rx_sepcial_80211_frame, "rxsped11frm=u8,u8,u8"},
	{"rxupdnav", skw_iwpriv_set_rx_update_nav, "rxupdnav=-u8,-u8,u8"},
	{"apgotimap", skw_iwpriv_set_apgo_timap, "apgotimap=u8,u8,u8,u8"},
	{"dbdcdis", skw_iwpriv_set_dbdc_disable, "dbdcdis=u8"},
	{"addrval", skw_iwpriv_set_assign_address_val, "addrval=0x12345678,0x12345678"},
	{"rdaddr", skw_iwpriv_read_addr, "rdaddr=0x12345678"},
	{"ageout", skw_iwpriv_set_ageout_thrd, "ageout=u8,u8"},
	{"params", skw_iwpriv_set_params, "params=index,val1,val2,val3,..."}, //for tlv42 and 54~57,59

	/*keep last*/
	{NULL, NULL, NULL}
};

static struct skw_iwpriv_cmd skw_iwpriv_get_cmds[] = {
	/* keep first */
	{"help", skw_iwpriv_help, "usage"},

	{"bandcfg", skw_iwpriv_get_bandcfg, "bandcfg"},

	/*keep last*/
	{NULL, NULL, NULL}
};

static struct skw_iwpriv_cmd *skw_iwpriv_cmd_match(struct skw_iwpriv_cmd *cmds,
					const char *key, int key_len)
{
	int i;

	for (i = 0; cmds[i].name; i++) {
		if (!memcmp(cmds[i].name, key, key_len))
			return &cmds[i];
	}

	return NULL;
}

static int skw_iwpriv_set(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	int key_len;
	char param[128] = {0};
	char *token = NULL, *args = NULL;
	struct skw_iwpriv_cmd *iwpriv_cmd;
	struct skw_iface *iface = (struct skw_iface *)netdev_priv(dev);

	WARN_ON(sizeof(param) < wrqu->data.length);

	if (copy_from_user(param, wrqu->data.pointer, sizeof(param))) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		return -EFAULT;
	}
	param[sizeof(param) - 1] = '\0';

	token = strchr(param, '=');
	if (!token) {
		key_len = strlen(param);
		args = NULL;
	} else {
		key_len = token - param;
		args = token + 1;
	}

	iwpriv_cmd = skw_iwpriv_cmd_match(skw_iwpriv_set_cmds, param, key_len);
	if (iwpriv_cmd)
		ret = iwpriv_cmd->handler(iface, iwpriv_cmd, args,
				extra, SKW_GET_LEN_512);
	else
		ret = skw_iwpriv_help(iface, skw_iwpriv_set_cmds, NULL,
				extra, SKW_GET_LEN_512);

	if (ret < 0)
		sprintf(extra, " usage: %s\n", iwpriv_cmd->help_info);

	wrqu->data.length = SKW_GET_LEN_1024;

	skw_dbg("resp: %s\n", extra);

	return 0;
}

static int skw_iwpriv_get(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int ret;
	char cmd[128] = {0};
	struct skw_iwpriv_cmd *priv_cmd = NULL;
	struct skw_iface *iface = (struct skw_iface *)netdev_priv(dev);

	if (copy_from_user(cmd, wrqu->data.pointer, sizeof(cmd))) {
		skw_err("copy failed, length: %d\n",
			wrqu->data.length);

		return -EFAULT;
	}

	skw_dbg("cmd: 0x%x, %s(len: %d)\n", info->cmd, cmd, wrqu->data.length);

	cmd[sizeof(cmd) - 1] = '\0';
	priv_cmd = skw_iwpriv_cmd_match(skw_iwpriv_get_cmds, cmd, strlen(cmd));
	if (priv_cmd)
		ret = priv_cmd->handler(iface, priv_cmd, NULL, extra,
				SKW_GET_LEN_512);
	else
		ret = skw_iwpriv_help(iface, skw_iwpriv_get_cmds, NULL,
				extra, SKW_GET_LEN_512);

	wrqu->data.length = SKW_GET_LEN_512;

	skw_dbg("resp: %s\n", extra);

	return ret;
}

static int skw_iwpriv_at(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int ret;
	char cmd[SKW_SET_LEN_256];
	int len = wrqu->data.length;
	struct skw_core *skw = ((struct skw_iface *)netdev_priv(dev))->skw;

	BUG_ON(sizeof(cmd) < len);

	if (copy_from_user(cmd, wrqu->data.pointer, sizeof(cmd))) {
		skw_err("copy failed, length: %d\n", len);

		return -EFAULT;
	}

	skw_dbg("cmd: %s, len: %d\n", cmd, len);

	if (len + 2 > sizeof(cmd))
		return -EINVAL;

	cmd[len - 1] = 0xd;
	cmd[len + 0] = 0xa;
	cmd[len + 1] = 0x0;

	ret = skw_send_at_cmd(skw, cmd, len + 2, extra, SKW_GET_LEN_512);

	wrqu->data.length = SKW_GET_LEN_512;

	skw_dbg("resp: %s", extra);

	return ret;
}

static struct iw_priv_args skw_iw_priv_args[] = {
	{
		SKW_IW_PRIV_SET,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_128,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_1024,
		"set",
	},
	{
		SKW_IW_PRIV_GET,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_128,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"get",
	},
	{
		SKW_IW_PRIV_AT,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_256,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"at",
	},
	{
		SKW_IW_PRIV_80211MODE,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_128,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"mode",
	},
	{
		SKW_IW_PRIV_GET_80211MODE,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_128,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"get_mode",
	},
	{
		SKW_IW_PRIV_KEEP_ALIVE,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_1024,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"keep_alive",
	},
	{
		SKW_IW_PRIV_WOW_FILTER,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_1024,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"wow_filter",
	},
	{
		SKW_IW_PRIV_OFFLOAD_TCP,
		IW_PRIV_TYPE_CHAR | SKW_SET_LEN_1024,
		IW_PRIV_TYPE_CHAR | SKW_GET_LEN_512,
		"offload_tcp",
	},
	{0, 0, 0, {0} }
};

static const iw_handler skw_iw_priv_handlers[] = {
	NULL,
	skw_iwpriv_set,
	NULL,
	skw_iwpriv_get,
	NULL,
	skw_iwpriv_at,
	skw_iwpriv_mode,
	skw_iwpriv_get_mode,
	skw_iwpriv_keep_alive,
	skw_iwpriv_wow_filter,
	skw_iwpriv_offload_keepalive,
	//skw_iwpriv_get_wow_filter,
};
#endif

static const struct iw_handler_def skw_iw_ops = {
	.standard = skw_iw_standard_handlers,
	.num_standard = ARRAY_SIZE(skw_iw_standard_handlers),
#ifdef CONFIG_WEXT_PRIV
	.private = skw_iw_priv_handlers,
	.num_private = ARRAY_SIZE(skw_iw_priv_handlers),
	.private_args = skw_iw_priv_args,
	.num_private_args = ARRAY_SIZE(skw_iw_priv_args),
#endif
	.get_wireless_stats = skw_get_wireless_stats,
};

const void *skw_iw_handlers(void)
{
#ifdef CONFIG_WIRELESS_EXT
	return &skw_iw_ops;
#else
	skw_info("CONFIG_WIRELESS_EXT not enabled\n");
	return NULL;
#endif
}
