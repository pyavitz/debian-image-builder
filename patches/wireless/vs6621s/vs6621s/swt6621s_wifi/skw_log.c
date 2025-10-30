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

#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/if_arp.h>

#include "skw_compat.h"
#include "skw_log.h"
#include "skw_dentry.h"
#include "skw_cfg80211.h"
#include "skw_iface.h"
#include "skw_rx.h"

#define SKW_LL_MASK 0xffff

#if defined(CONFIG_SWT6621S_LOG_ERROR)
#define SKW_LOG_LEVEL SKW_ERROR
#elif defined(CONFIG_SWT6621S_LOG_WARN)
#define SKW_LOG_LEVEL SKW_WARN
#elif defined(CONFIG_SWT6621S_LOG_INFO)
#define SKW_LOG_LEVEL SKW_INFO
#elif defined(CONFIG_SWT6621S_LOG_DEBUG)
#define SKW_LOG_LEVEL SKW_DEBUG
#elif defined(CONFIG_SWT6621S_LOG_DETAIL)
#define SKW_LOG_LEVEL SKW_DETAIL
#else
#define SKW_LOG_LEVEL SKW_INFO
#endif

static unsigned long skw_dbg_level;

unsigned long skw_log_level(void)
{
	return skw_dbg_level;
}

static void skw_set_log_level(int level)
{
	unsigned long dbg_level;

	dbg_level = skw_log_level() & (~SKW_LL_MASK);
	dbg_level |= ((level << 1) - 1);

	xchg(&skw_dbg_level, dbg_level);
}

static void skw_enable_func_log(int func, bool enable)
{
	unsigned long dbg_level = skw_log_level();

	if (enable)
		dbg_level |= func;
	else
		dbg_level &= (~func);

	xchg(&skw_dbg_level, dbg_level);
}


static struct skw_monitor_dbg_iface *g_skw_dbg_iface;
static DEFINE_MUTEX(skw_dbg_mutex);

void skw_dump_frame(u8 *frame, u16 frame_len)
{
	struct skw_radiotap_desc *radio_desc;
	struct sk_buff *skb;

	mutex_lock(&skw_dbg_mutex);
	if (g_skw_dbg_iface == NULL) {
		mutex_unlock(&skw_dbg_mutex);
		return;
	}

	skb = alloc_skb(frame_len + sizeof(struct skw_radiotap_desc), GFP_KERNEL);
	if (skb == NULL) {
		mutex_unlock(&skw_dbg_mutex);
		return;
	}

	radio_desc = (struct skw_radiotap_desc *)skb_put(skb,
								sizeof(struct skw_radiotap_desc));
	radio_desc->radiotap_hdr.it_version = PKTHDR_RADIOTAP_VERSION;
	radio_desc->radiotap_hdr.it_pad = 0;
	radio_desc->radiotap_hdr.it_len = sizeof(struct skw_radiotap_desc);
	radio_desc->radiotap_hdr.it_present = BIT(IEEE80211_RADIOTAP_FLAGS);
	radio_desc->radiotap_flag = 0;

	skw_put_skb_data(skb, frame, frame_len);

	__skb_trim(skb, frame_len + radio_desc->radiotap_hdr.it_len);
	skb_reset_mac_header(skb);

	skb->dev = g_skw_dbg_iface->ndev;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_80211_RAW);

	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
		netif_rx_ni(skb);
	#else
		netif_rx(skb);
	#endif

	mutex_unlock(&skw_dbg_mutex);
}

static netdev_tx_t skw_monitor_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops skw_monitor_netdev_ops = {
	.ndo_start_xmit = skw_monitor_xmit,
};

static void skw_add_dbg_iface(void)
{
	int priv_size, ret;
	struct net_device *ndev = NULL;
	struct skw_monitor_dbg_iface *iface;
	u8 mac[6] = {0xFe, 0xfd, 0xfc, 0x12, 0x11, 0x88};

	if (g_skw_dbg_iface) {
		skw_err("already add dbg iface\n");
		return;
	}

	priv_size = sizeof(struct skw_monitor_dbg_iface);
	ndev = alloc_netdev_mqs(priv_size, "skw_dbg",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
					NET_NAME_ENUM,
#endif
					ether_setup, SKW_WMM_AC_MAX, 1);
	if (!ndev) {
		skw_err("alloc ndev fail\n");
		return;
	}

	iface = netdev_priv(ndev);
	iface->ndev = ndev;

	skw_ether_copy(iface->addr, mac);
	ndev->type = ARPHRD_IEEE80211_RADIOTAP;

	ndev->netdev_ops = &skw_monitor_netdev_ops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	eth_hw_addr_set(ndev, mac);
#else
	skw_ether_copy(ndev->dev_addr, mac);
#endif
	rtnl_lock();
	ret = skw_register_netdevice(ndev);
	rtnl_unlock();
	if (ret) {
		skw_err("register netdev failed\n");
		goto free_iface;
	}

	mutex_lock(&skw_dbg_mutex);
	g_skw_dbg_iface = iface;
	mutex_unlock(&skw_dbg_mutex);

	return;

free_iface:
	if (ndev)
		free_netdev(ndev);
}

void skw_del_dbg_iface(void)
{
	struct skw_monitor_dbg_iface *iface;

	if (!g_skw_dbg_iface)
		return;

	mutex_lock(&skw_dbg_mutex);

	iface = g_skw_dbg_iface;
	g_skw_dbg_iface = NULL;

	mutex_unlock(&skw_dbg_mutex);

	rtnl_lock();
	skw_unregister_netdevice(iface->ndev);
	rtnl_unlock();
}

static void skw_set_dump_frame(bool enable)
{
	if (enable)
		skw_add_dbg_iface();
	else
		skw_del_dbg_iface();
}

static char *skw_dump_frame_status(void)
{
	if (g_skw_dbg_iface)
		return "enable";
	else
		return "disable";
}

static int skw_log_show(struct seq_file *seq, void *data)
{
	int i;
	u32 level = skw_log_level();
	const char *log_level;
	static const char *log_name[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "DETAIL"};

	i = ffs((level & SKW_LL_MASK) + 1) - 1;
	if (i >= 0 && i < ARRAY_SIZE(log_name))
		log_level = log_name[i];
	else
		log_level = "INVALID";

	seq_puts(seq, "\n");
	seq_printf(seq, "Log Level: %s    [ERROR|WARN|INFO|DEBUG|DETAIL]\n", log_name[i]);

#define SKW_LOG_STATUS(s) (level & (s) ? "enable" : "disable")
	seq_puts(seq, "\n");
	seq_printf(seq, "command log: %s\n", SKW_LOG_STATUS(SKW_CMD));
	seq_printf(seq, "event   log: %s\n", SKW_LOG_STATUS(SKW_EVENT));
	seq_printf(seq, "dump    log: %s\n", SKW_LOG_STATUS(SKW_DUMP));
	seq_printf(seq, "scan    log: %s\n", SKW_LOG_STATUS(SKW_SCAN));
	seq_printf(seq, "timer   log: %s\n", SKW_LOG_STATUS(SKW_TIMER));
	seq_printf(seq, "state   log: %s\n", SKW_LOG_STATUS(SKW_STATE));
	seq_printf(seq, "work    log: %s\n", SKW_LOG_STATUS(SKW_WORK));
	seq_printf(seq, "dump  frame: %s\n", skw_dump_frame_status());
	seq_printf(seq, "DFS     log: %s\n", SKW_LOG_STATUS(SKW_DFS));
#undef SKW_LOG_STATUS

	return 0;
}

static int skw_log_open(struct inode *inode, struct file *file)
{
	// return single_open(file, &skw_log_show, inode->i_private);
	return single_open(file, &skw_log_show, skw_pde_data(inode));
}

static int skw_log_control(const char *cmd, bool enable)
{
	if (!strcmp("command", cmd))
		skw_enable_func_log(SKW_CMD, enable);
	else if (!strcmp("event", cmd))
		skw_enable_func_log(SKW_EVENT, enable);
	else if (!strcmp("dump", cmd))
		skw_enable_func_log(SKW_DUMP, enable);
	else if (!strcmp("scan", cmd))
		skw_enable_func_log(SKW_SCAN, enable);
	else if (!strcmp("timer", cmd))
		skw_enable_func_log(SKW_TIMER, enable);
	else if (!strcmp("state", cmd))
		skw_enable_func_log(SKW_STATE, enable);
	else if (!strcmp("work", cmd))
		skw_enable_func_log(SKW_WORK, enable);
	else if (!strcmp("dfs", cmd))
		skw_enable_func_log(SKW_DFS, enable);
	else if (!strcmp("detail", cmd))
		skw_set_log_level(SKW_DETAIL);
	else if (!strcmp("debug", cmd))
		skw_set_log_level(SKW_DEBUG);
	else if (!strcmp("info", cmd))
		skw_set_log_level(SKW_INFO);
	else if (!strcmp("warn", cmd))
		skw_set_log_level(SKW_WARN);
	else if (!strcmp("error", cmd))
		skw_set_log_level(SKW_ERROR);
	else if (!strcmp("dumpframe", cmd))
		skw_set_dump_frame(enable);
	else
		return -EINVAL;

	return 0;
}

static ssize_t skw_log_write(struct file *fp, const char __user *buffer,
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
			skw_log_control(cmd, enable);
			idx = 0;
			break;

		default:
			cmd[idx++] = tolower(c);
			idx %= 32;

			break;
		}

		buffer++;
	}

	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops skw_log_fops = {
	.proc_open = skw_log_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = skw_log_write,
};
#else
static const struct file_operations skw_log_fops = {
	.owner = THIS_MODULE,
	.open = skw_log_open,
	.read = seq_read,
	.release = single_release,
	.write = skw_log_write,
};
#endif

void skw_log_level_init(void)
{
	skw_set_log_level(SKW_LOG_LEVEL);

	skw_enable_func_log(SKW_CMD, false);
	skw_enable_func_log(SKW_EVENT, false);
	skw_enable_func_log(SKW_DUMP, false);
	skw_enable_func_log(SKW_SCAN, false);
	skw_enable_func_log(SKW_TIMER, false);
	skw_enable_func_log(SKW_STATE, true);
	skw_enable_func_log(SKW_WORK, false);
	skw_enable_func_log(SKW_DFS, false);

	skw_procfs_file(NULL, "log_level", 0666, &skw_log_fops, NULL);
}

void skw_log_level_deinit(void)
{
}
