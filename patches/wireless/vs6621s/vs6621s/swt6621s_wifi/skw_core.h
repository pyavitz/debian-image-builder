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

#ifndef __SKW_CORE_H__
#define __SKW_CORE_H__

#include <net/ipv6.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "skw_util.h"
#include "skw_compat.h"
#include "skw_dentry.h"
#include "skw_log.h"
#include "skw_platform_data.h"
#include "skw_work.h"
#include "skw_edma.h"
#include "skw_iface.h"
#include "skw_calib.h"
#include "skw_recovery.h"
#include "skw_config.h"

#define SKW_EXTEND_NAME                     ".dat"
#define SKW_CONFIG_FILE                     KBUILD_MODNAME SKW_EXTEND_NAME

#define SKW_BUS_TYPE_MASK                   TYPE_MASK
#define SKW_BUS_SDIO                        SDIO_LINK
#define SKW_BUS_USB                         USB_LINK
#define SKW_BUS_PCIE                        PCIE_LINK
#define SKW_BUS_SDIO2                       SDIO2_LINK
#define SKW_BUS_USB2                        USB2_LINK

#define SKW_BSP_NF_ASSERT                   DEVICE_ASSERT_EVENT
#define SKW_BSP_NF_BLOCKED                  DEVICE_BLOCKED_EVENT
#define SKW_BSP_NF_READY                    DEVICE_BSPREADY_EVENT
#define SKW_BSP_NF_DISCONNECT               4
#define SKW_BSP_NF_SUSPEND                  6
#define SKW_BSP_NF_RESUME                   7
#define SKW_BSP_NF_FW_REBOOT                8

#define SKW_FW_IPV6_COUNT_LIMIT             3

/*sap capa flag */
#define SKW_CAPA_HT                         BIT(0)
#define SKW_CAPA_VHT                        BIT(1)
#define SKW_CAPA_HE                         BIT(2)

/* capability */
#define SKW_MAX_LMAC_SUPPORT                2
#define SKW_NR_IFACE                        4
#define SKW_LAST_IFACE_ID                   (SKW_NR_IFACE - 1)
#define SKW_MAX_PEER_SUPPORT                32
#define SKW_MAX_STA_ALLOWED                 10
#define SKW_NR_SGL_DAT                      256
#define SKW_NR_SGL_CMD                      4
#define SKW_MAX_IE_LEN                      1400
#define SKW_MSG_BUFFER_LEN                  2048
#define SKW_TX_PACK_SIZE                    1536

#define SKW_DATA_ALIGN_SIZE                 4
#define SKW_DATA_ALIGN_MASK                 3

#define SKW_EXTER_HDR_SIZE                  4
#define SKW_TX_HDR_SIZE                     6

#define SKW_DBG_NR_CMD                      2
#define SKW_DBG_NR_DAT                      2

/* protocal */
#define SKW_ETH_P_WAPI                      0x88B4

/* ioctl */
#define PRIVATE_COMMAND_MAX_LEN             8192
#define PRIVATE_COMMAND_DEF_LEN             4096

#define SKW_ANDROID_PRIV_START              "START"
#define SKW_ANDROID_PRIV_STOP               "STOP"
#define SKW_ANDROID_PRIV_SETFWPATH          "SETFWPATH"
#define SKW_ANDROID_PRIV_COUNTRY            "COUNTRY"
#define SKW_ANDROID_PRIV_BTCOEXSCAN_STOP    "BTCOEXSCAN-STOP"
#define SKW_ANDROID_PRIV_RXFILTER_START     "RXFILTER-START"
#define SKW_ANDROID_PRIV_RXFILTER_STOP      "RXFILTER-STOP"
#define SKW_ANDROID_PRIV_RXFILTER_ADD       "RXFILTER-ADD"
#define SKW_ANDROID_PRIV_RXFILTER_REMOVE    "RXFILTER-REMOVE"
#define SKW_ANDROID_PRIV_SETSUSPENDMODE     "SETSUSPENDMODE"
#define SKW_ANDROID_PRIV_BTCOEXMODE         "BTCOEXMODE"
#define SKW_ANDROID_PRIV_MAX_NUM_STA        "MAX_NUM_STA"
#define SKW_ANDROID_PRIV_SET_AP_WPS_P2P_IE  "SET_AP_WPS_P2P_IE"

/* SKW_FLAG_* */
#define SKW_FLAG_FW_ASSERT                  (0)
#define SKW_FLAG_BLOCK_TX                   (1)
#define SKW_FLAG_FW_MAC_RECOVERY            (2)
#define SKW_FLAG_FW_THERMAL                 (3)

#define SKW_FLAG_FW_UART_OPEND              (4)
#define SKW_FLAG_FW_FILTER_ARP              (5)
#define SKW_FLAG_FW_IGNORE_CRED             (6)
/* data not permit */
#define SKW_FLAG_FW_CHIP_RECOVERY           (7)
#define SKW_FLAG_SAP_SME_EXTERNAL           (8)
#define SKW_FLAG_STA_SME_EXTERNAL           (9)
#define SKW_FLAG_MBSSID_PRIV                (10)
#define SKW_FLAG_MP_MODE                    (11)
#define SKW_FLAG_LEGACY_P2P_COMMON_PORT     (12)
#define SKW_FLAG_SWITCHING_USB_MODE         (13)
#define SKW_FLAG_PRIV_REGD                  (15)
#define SKW_FLAG_REPEATER                   (16)
#define SKW_FLAG_FW_PN_REUSE                (17)
#define SKW_FLAG_SUSPEND_PREPARE            (19)

/* SKW_LMAC_FLAG_* */
#define SKW_LMAC_FLAG_INIT                  BIT(0)
#define SKW_LMAC_FLAG_ACTIVED               BIT(1)
#define SKW_LMAC_FLAG_RXCB                  BIT(2)
#define SKW_LMAC_FLAG_TXCB                  BIT(3)

#define SKW_SYNC_ADMA_TX                    0
#define SKW_SYNC_SDMA_TX                    1
#define SKW_ASYNC_ADMA_TX                   2
#define SKW_ASYNC_SDMA_TX                   3
#define SKW_ASYNC_EDMA_TX                   4

#define SKW_TXQ_STOPED(n, q) \
	netif_tx_queue_stopped(netdev_get_tx_queue(n, q))

#define SKW_IOCTL_ANDROID_CMD               (SIOCDEVPRIVATE + 1)
#define SKW_IOCTL_CMD                       (SIOCDEVPRIVATE + 15)
#define SKW_IOCTL_SUBCMD_COUNTRY            1

extern unsigned int tx_wait_time;

struct skw_ioctl_cmd {
	int len;
	int id;
};

struct skw_lmac {
	u8 id;
	u8 flags; /* reference SKW_LMAC_FLAG_ */
	s8 lport; /* logic port */
	s8 dport; /* data port */

	//u8 tx_done_chn;
	//u8 rx_chn;
	//u8 rx_buff_chn;
	int iface_bitmap;
	struct skw_peer_ctx peer_ctx[SKW_MAX_PEER_SUPPORT];
	atomic_t fw_credit;

	// struct skw_wmm_tx cached;

	struct net_device dummy_dev;
	struct napi_struct napi_tx;
	struct napi_struct napi_rx;

	struct work_struct dy_work;
	struct skw_tx_vring *tx_vring;

	atomic_t avail_skb_num;

	struct sk_buff_head rx_dat_q;
	struct sk_buff_head avail_skb;
	struct sk_buff_head edma_free_list;

	struct skw_list rx_todo_list;
	struct skw_core *skw;
};

struct skw_firmware_info {
	u8 build_time[32];
	u8 plat_ver[16];
	u8 wifi_ver[16];
	u8 calib_file[64];
	u16 max_num_sta;
	u16 resv;
	u32 timestamp;
	u64 host_timestamp;
	unsigned long host_seconds;
	u32 fw_bw_capa;
};

#define SKW_BW_CAP_2G_20M       BIT(0)
#define SKW_BW_CAP_2G_40M       BIT(1)
#define SKW_BW_CAP_5G_20M       BIT(2)
#define SKW_BW_CAP_5G_40M       BIT(3)
#define SKW_BW_CAP_5G_80M       BIT(4)
#define SKW_BW_CAP_5G_160M      BIT(5)
#define SKW_BW_CAP_5G_80P80M    BIT(6)

struct skw_chip_info {
	u16 enc_capa;

	u32 chip_model;
	u32 chip_version;
	u32 fw_version;
	u32 fw_capa;

	u8 max_sta_allowed;
	u8 max_mc_addr_allowed;

	/* HT */
	u16 ht_capa;
	u16 ht_ext_capa;
	u16 ht_ampdu_param;
	u32 ht_tx_mcs_maps;
	u32 ht_rx_mcs_maps;

	/* VHT */
	u32 vht_capa;
	u16 vht_tx_mcs_maps;
	u16 vht_rx_mcs_maps;

	/* HE */
	u8 max_scan_ssids;
	u8 he_capa[6];
	u8 he_phy_capa[11];
	u16 he_tx_mcs_maps;
	u16 he_rx_mcs_maps;
	u8 mac[ETH_ALEN];

	u8 abg_rate_num;
	u8 abg_rate[15];

	u32 fw_bw_capa; /* reference SKW_BW_CAP_* */

	u32 priv_filter_arp:1;
	u32 priv_ignore_cred:1;
	u32 priv_pn_reuse:1;
	u32 priv_p2p_common_port:1;
	u32 priv_dfs_master_enabled:1;
	u32 priv_2g_only:1;
	u32 priv_resv:18;
	u32 nr_hw_mac:8;

	u8 fw_build_time[32];
	u8 fw_plat_ver[16];
	u8 fw_wifi_ver[16];
	u8 fw_bt_ver[16];

	u32 fw_timestamp;
	u32 fw_chip_type;
	u16 calib_module_id;
	u16 resv;
	u32 fw_ext_capa[12];
} __packed;

enum SKW_MSG_VERSION {V0, V1, V2, V3};

#define SKW_MAX_MSG_ID        256

struct skw_version_info {
	u8 cmd[SKW_MAX_MSG_ID];
	u8 event[SKW_MAX_MSG_ID];
} __packed;

struct skw_hw_extra {
	u8 hdr_len;
	u8 chn_offset;
	u8 len_offset;
	u8 eof_offset;
};

struct skw_fixed_offset {
	s16 hdr_offset;
	s16 msdu_offset;
};

#define SKW_HW_FLAG_EXTRA_HDR            (0)
#define SKW_HW_FLAG_SDIO_V2              (1)
#define SKW_HW_FLAG_USB_V2               (2)
#define SKW_HW_FLAG_CFG80211_PM          (3)

typedef int (*hw_xmit_func)(struct skw_core *skw, struct sk_buff_head *list,
			    int lmac_id, int port, struct scatterlist *sgl,
			    int nents, int tx_bytes);

typedef int (*bus_dat_xmit_func)(struct skw_core *skw, int lmac_id,
				 struct sk_buff_head *txq_list);

typedef int (*bus_cmd_xmit_func)(struct skw_core *skw, void *cmd, int cmd_len);

struct skw_hw_info {
	u8 bus;
	u8 dma;
	u8 nr_lmac;
	u8 cmd_port;

	u16 align;
	int pkt_limit;
	unsigned long flags;
	atomic_t credit; /* total credit of all LMAC */

	hw_xmit_func cmd_xmit;
	hw_xmit_func cmd_disable_irq_xmit;
	hw_xmit_func dat_xmit;

	bus_dat_xmit_func bus_dat_xmit;
	bus_cmd_xmit_func bus_cmd_xmit;

	struct skw_hw_extra extra;
	struct skw_fixed_offset rx_desc;
	struct skw_lmac lmac[SKW_MAX_LMAC_SUPPORT];

	struct {
		bool enabled;
		u16 flags;
	} wow;
};

struct skw_vif {
	u16 bitmap;
	u16 opened_dev;
	spinlock_t lock;
	struct skw_iface *iface[SKW_NR_IFACE];
};

struct skw_work_data {
	spinlock_t rcu_lock;
	struct rcu_head *rcu_hdr;
	struct rcu_head **rcu_tail;

	unsigned long flags;
	struct sk_buff_head work_list;
};

struct skw_timer_data {
	int count;
	spinlock_t lock;
	struct list_head list;
	struct timer_list timer;
};

struct skw_recovery_data {
	struct mutex lock;
	struct skw_recovery_ifdata iface[SKW_NR_IFACE];
	struct skw_peer *peer[SKW_MAX_LMAC_SUPPORT][SKW_MAX_PEER_SUPPORT];
};

struct skw_dbg_cmd {
	u16 id, seq;
	u32 loop;
	unsigned long flags;
	u64 trigger, build, xmit, done, ack, assert;
};

struct skw_dbg_dat {
	u32 qlen;
	u32 resv;
	u64 trigger, done;
};

struct skw_core {
	struct sv6160_platform_data *hw_pdata;

	struct sk_buff_head rx_dat_q;
	atomic_t txqlen_pending;

	atomic_t tx_wake, rx_wake, exit;
	wait_queue_head_t tx_wait_q, rx_wait_q;

	struct net_device dummy_dev;
	struct napi_struct napi_rx;

	struct task_struct *rx_thread;

#ifdef CONFIG_SWT6621S_TX_WORKQUEUE
	struct workqueue_struct *tx_wq, *tx_dy;
	struct delayed_work tx_worker;

	//struct workqueue_struct *rx_wq;
	//struct work_struct rx_worker;
#else
	struct task_struct *tx_thread, *rx_thread;
#endif

	/* workqueu for mlme worker and etc. */
	struct workqueue_struct *event_wq;
	struct skw_event_work event_work;

	struct work_struct work;
	struct skw_work_data work_data;
	struct work_struct work_unlock;

	struct sk_buff_head kfree_skb_qlist;
	struct work_struct kfree_skb_task;

	struct sk_buff_head skb_recycle_qlist;

	struct work_struct recovery_work;
	struct skw_recovery_data recovery_data;

	struct mutex lock;
	struct skw_firmware_info fw;
	struct skw_hw_info hw;
	struct skw_vif vif;
	struct mac_address address[SKW_NR_IFACE];

	unsigned long flags; /* reference SKW_FLAG_FW_ */
	unsigned long nf_flags;

	u8 country[2];
	u16 idx;
	u16 skw_event_sn;
	int isr_cpu_id;

	u16 nr_scan_results;
	struct cfg80211_scan_request *scan_req;
	struct cfg80211_sched_scan_request *sched_scan_req;

	struct notifier_block ifa4_nf;
	struct notifier_block ifa6_nf;
	struct notifier_block bsp_nf;
	struct notifier_block pm_nf;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	unsigned int num_iftype_ext_capab;
	struct wiphy_iftype_ext_capab iftype_ext_cap[NUM_NL80211_IFTYPES];
#endif

	struct skw_dpd dpd;
#ifdef CONFIG_SWT6621S_USB3_WORKAROUND
	struct completion usb_switch_done;
#endif

	void *sdma_buff, *eof_blk;
	struct scatterlist *sgl_cmd, *sgl_dat;

	u16 skb_headroom;
	u16 skb_share_len;

	u8 ext_capa[12];
	unsigned long trans_start;
	unsigned long rx_packets;
	unsigned long tx_packets;

#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock rx_wlock;
#endif
	spinlock_t rx_lock;
	struct skw_list rx_todo_list;

	const struct ieee80211_regdomain *regd;
	struct dentry *dentry;
	struct proc_dir_entry *pentry;

	struct skw_timer_data timer_data;
	struct skw_config *config;

	struct {
		struct semaphore lock;
		struct semaphore mgmt_cmd_lock;
		wait_queue_head_t wq;
		struct wakeup_source *ws;

		unsigned long start_time;
		void (*callback)(struct skw_core *skw);

		unsigned long flags; /* reference SKW_CMD_FLAG_ */

		const char *name;
		void *data;
		void *arg;

		u16 data_len;
		u16 arg_size;

		u16 seq;
		u16 status;

		int id;
	} cmd;

	struct {
		struct skw_edma_chn cmd_chn;
		struct skw_edma_chn short_event_chn;
		struct skw_edma_chn long_event_chn;

		struct skw_edma_chn tx_chn[SKW_MAX_LMAC_SUPPORT];
		struct skw_edma_chn tx_resp_chn[SKW_MAX_LMAC_SUPPORT];
		struct skw_edma_chn rx_chn[SKW_MAX_LMAC_SUPPORT];
		struct skw_edma_chn rx_req_chn[SKW_MAX_LMAC_SUPPORT];
		struct skw_edma_chn filter_chn[SKW_MAX_LMAC_SUPPORT];
	} edma;

	struct {
		bool fw_enabled;
		u64 last_pulse_ts;
		unsigned long flags;

		struct list_head skw_pulse_pool;
		struct list_head skw_pseq_pool;
		spinlock_t skw_pool_lock;

		enum nl80211_dfs_regions region;
		struct cfg80211_chan_def chan;
		const struct skw_radar_info *info;
	} dfs;

	struct {
		atomic_t loop;
		u8 cmd_idx, dat_idx;
		u8 nr_cmd, nr_dat;
		struct skw_dbg_cmd cmd[SKW_DBG_NR_CMD];
		struct skw_dbg_dat dat[SKW_DBG_NR_DAT];
	} dbg;
};

struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
};

#ifdef CONFIG_COMPAT
struct compat_android_wifi_priv_cmd {
	compat_caddr_t buf;
	int used_len;
	int total_len;
};
#endif

struct skw_calib_param {
	u8 seq;
	u8 end;
	u16 len;
	u8 data[512];
} __packed;

#define SKW_WIPHY_DENTRY(w) (((struct skw_core *)wiphy_priv(w))->dentry)
#define SKW_WIPHY_PENTRY(w) (((struct skw_core *)wiphy_priv(w))->pentry)

static inline int skw_wifi_enable(void *pdata)
{
	struct sv6160_platform_data *pd = pdata;

	if (pd && pd->service_start)
		return pd->service_start();

	return -ENOTSUPP;
}

static inline int skw_wifi_disable(void *pdata)
{
	struct sv6160_platform_data *pd = pdata;

	if (pd && pd->service_stop)
		return pd->service_stop();

	return -ENOTSUPP;
}

static inline int skw_power_on_chip(void)
{
	return 0;
}

static inline int skw_power_off_chip(void)
{
	return 0;
}

static inline int skw_hw_reset_chip(struct skw_core *skw)
{
	return 0;
}

static inline int skw_hw_get_chip_id(struct skw_core *skw)
{
	return 0;
}

static inline struct skw_tx_cb *SKW_SKB_TXCB(struct sk_buff *skb)
{
	return (struct skw_tx_cb *)skb->cb;
}

void skw_dbg_dump(struct skw_core *skw);
static inline int skw_hw_assert(struct skw_core *skw, bool dump)
{
	if (test_and_set_bit(SKW_FLAG_FW_ASSERT, &skw->flags))
		return 0;

	if (dump)
		skw_dbg_dump(skw);

	if (skw->hw_pdata->modem_assert)
		skw->hw_pdata->modem_assert();

	return 0;
}

static inline int skw_hw_request_ack(struct skw_core *skw)
{
	if (!skw->hw_pdata || !skw->hw_pdata->rx_thread_wakeup)
		return -ENOTSUPP;

	skw->hw_pdata->rx_thread_wakeup();

	return 0;
}

static inline int skw_register_rx_cb(struct skw_core *skw, int port,
			      rx_submit_fn rx_cb, void *data)
{
	if (!skw->hw_pdata || !skw->hw_pdata->callback_register)
		return -ENOTSUPP;

	return skw->hw_pdata->callback_register(port, (void *)rx_cb, data);
}

static inline int skw_register_tx_cb(struct skw_core *skw, int port,
			      rx_submit_fn tx_cb, void *data)
{
	if (!skw->hw_pdata || !skw->hw_pdata->tx_callback_register)
		return -ENOTSUPP;

	return skw->hw_pdata->tx_callback_register(port, (void *)tx_cb, data);
}

static inline bool skw_need_extra_hdr(struct skw_core *skw)
{
	return test_bit(SKW_HW_FLAG_EXTRA_HDR, &skw->hw.flags);
}

static inline void skw_set_extra_hdr(struct skw_core *skw, void *extra_hdr,
				u8 chn, u16 len, u16 pad, u8 eof)
{
	u32 *hdr = extra_hdr;
	struct skw_hw_extra *ext = &skw->hw.extra;

	*hdr = chn << ext->chn_offset |
	       (len - ext->hdr_len) << ext->len_offset |
	       (!!eof) << ext->eof_offset;
}

static inline int skw_uart_open(struct skw_core *skw)
{
	u8 port;

	if (!skw->hw_pdata || !skw->hw_pdata->at_ops.open)
		return -ENOTSUPP;

	if (test_bit(SKW_FLAG_FW_UART_OPEND, &skw->flags))
		return 0;

	port = skw->hw_pdata->at_ops.port;

	set_bit(SKW_FLAG_FW_UART_OPEND, &skw->flags);

	return skw->hw_pdata->at_ops.open(port, NULL, NULL);
}

static inline int skw_uart_write(struct skw_core *skw, char *cmd, int len)
{
	u8 port;

	if (!skw->hw_pdata || !skw->hw_pdata->at_ops.write)
		return -ENOTSUPP;

	port = skw->hw_pdata->at_ops.port;

	return skw->hw_pdata->at_ops.write(port, cmd, len);
}

static inline int skw_uart_read(struct skw_core *skw, char *buf, int buf_len)
{
	u8 port;

	if (!skw->hw_pdata || !skw->hw_pdata->at_ops.read)
		return -ENOTSUPP;

	port = skw->hw_pdata->at_ops.port;

	return skw->hw_pdata->at_ops.read(port, buf, buf_len);
}

static inline int skw_uart_close(struct skw_core *skw)
{
	u8 port;

	if (!skw->hw_pdata || !skw->hw_pdata->at_ops.close)
		return -ENOTSUPP;

	port = skw->hw_pdata->at_ops.port;

	return skw->hw_pdata->at_ops.close(port);
}

static inline int skw_register_bsp_notifier(struct skw_core *skw,
					struct notifier_block *nb)
{
	if (!skw->hw_pdata || !skw->hw_pdata->modem_register_notify)
		return -ENOTSUPP;

	skw->hw_pdata->modem_register_notify(nb);

	return 0;
}

static inline int skw_unregister_bsp_notifier(struct skw_core *skw,
					struct notifier_block *nb)
{
	if (!skw->hw_pdata || !skw->hw_pdata->modem_unregister_notify)
		return -ENOTSUPP;

	skw->hw_pdata->modem_unregister_notify(nb);

	return 0;
}

static inline void skw_wakeup_tx(struct skw_core *skw, unsigned long delay)
{
#ifdef CONFIG_SWT6621S_TX_WORKQUEUE
	mod_delayed_work(skw->tx_wq, &skw->tx_worker, delay);
#else
	if (atomic_add_return(1, &skw->tx_wake) == 1)
		wake_up(&skw->tx_wait_q);
#endif
}

static inline void skw_wakeup_rx(struct skw_core *skw)
{
	int i;

	//wake_up_process(skw->rx_thread);
	if (skw->hw.bus == SKW_BUS_PCIE) {
		for (i = 0; i < skw->hw.nr_lmac; i++)
			if (skw->hw.lmac->iface_bitmap != 0)
				napi_schedule(&skw->hw.lmac[i].napi_rx);
	} else
		wake_up_process(skw->rx_thread);
	//napi_schedule(&skw->napi_rx);
}

static inline struct skw_iface *to_skw_iface(struct skw_core *skw, int id)
{
	if (!skw || id & 0xfffffffc)
		return NULL;

	return skw->vif.iface[id];
}

static inline void skw_sync_credit(struct skw_core *skw, int lmac_id)
{
	int new, done;
	struct skw_edma_chn *chn;
	struct skw_lmac *lmac = &skw->hw.lmac[lmac_id];

	if (!skw->hw_pdata || !skw->hw_pdata->edma_get_node_tot_cnt)
		return;

	chn = &skw->edma.tx_chn[lmac_id];
	done = skw->hw_pdata->edma_get_node_tot_cnt(SKW_EDMA_WIFI_TX0_CHN+lmac_id);
	new = chn->max_node_num - atomic_read(&chn->nr_node) - done;

	atomic_add(new, &chn->nr_node);
	atomic_set(&lmac->fw_credit, atomic_read(&chn->nr_node) * SKW_EDMA_TX_CHN_CREDIT);
}

static inline int skw_get_hw_credit(struct skw_core *skw, int lmac_id)
{
	struct skw_lmac *lmac = &skw->hw.lmac[lmac_id];

#if 0
	if (!(lmac->flags & SKW_LMAC_FLAG_ACTIVED))
		return 0;
#endif
	if (test_bit(SKW_FLAG_FW_IGNORE_CRED, &skw->flags))
		return INT_MAX;

	//for test
	if (tx_wait_time == 99)
		return INT_MAX;

	if (skw->hw.bus == SKW_BUS_PCIE)
		skw_sync_credit(skw, lmac_id);

	return atomic_read(&lmac->fw_credit);
}

static inline void skw_set_trans_start(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++)
		netdev_get_tx_queue(dev, i)->trans_start = jiffies;
}

static inline void skw_start_dev_queue(struct skw_core *skw)
{
	int i;
	struct skw_iface *iface;

	for (i = 0; i < SKW_NR_IFACE; i++) {
		iface = skw->vif.iface[i];
		if (!iface || !iface->ndev)
			continue;
		if (iface->ndev->flags & IFF_UP) {
			netif_tx_start_all_queues(iface->ndev);
			skw_set_trans_start(iface->ndev);
			netif_tx_schedule_all(iface->ndev);
		}
	}
}

static inline void skw_stop_dev_queue(struct skw_core *skw)
{
	int i;
	struct skw_iface *iface;

	for (i = 0; i < SKW_NR_IFACE; i++) {
		iface = skw->vif.iface[i];
		if (!iface || !iface->ndev)
			continue;
		if (iface->ndev->flags & IFF_UP) {
			netif_tx_stop_all_queues(iface->ndev);
			smp_mb();
		}
	}
}

static inline void skw_sub_credit(struct skw_core *skw, int lmac_id, int used)
{
	smp_rmb();
	atomic_sub(used, &skw->hw.lmac[lmac_id].fw_credit);
}

static inline bool is_skw_local_addr6(struct in6_addr *addr)
{
	return ipv6_addr_type(addr) &
		(IPV6_ADDR_LINKLOCAL | IPV6_ADDR_LOOPBACK);
}

static inline dma_addr_t skw_pci_map_single(struct skw_core *skw, void *ptr,
			size_t size, int direction)
{
	struct device *dev = priv_to_wiphy(skw)->dev.parent;

	return dma_map_single(dev, ptr, size, direction);
}

static inline void skw_pci_unmap_single(struct skw_core *skw,
		dma_addr_t dma_addr, size_t size, int direction)
{
	struct device *dev = priv_to_wiphy(skw)->dev.parent;

	return dma_unmap_single(dev, dma_addr, size, direction);
}

static inline int
skw_pcie_mapping_error(struct skw_core *skw, dma_addr_t dma_addr)
{
	struct device *dev = priv_to_wiphy(skw)->dev.parent;

	return dma_mapping_error(dev, dma_addr);
}

static inline bool skw_lmac_is_actived(struct skw_core *skw, int lmac_id)
{
	return (skw->hw.lmac[lmac_id].flags & SKW_LMAC_FLAG_ACTIVED);
}

static inline const char *skw_bus_name(int bus)
{
	static const char name[][8] = {"sdio", "usb", "pcie", "null"};

	return name[bus & 0x3];
}

struct skw_peer_ctx *skw_get_ctx(struct skw_core *skw, u8 lmac_id, u8 idx);
int skw_lmac_bind_iface(struct skw_core *skw, struct skw_iface *iface, int lmac_id);
int skw_lmac_unbind_iface(struct skw_core *skw, int lmac_id, int iface_id);
int skw_netdev_init(struct wiphy *wiphy, struct net_device *ndev, u8 *addr);
void skw_netdev_deinit(struct net_device *ndev);
void skw_add_credit(struct skw_core *skw, int lmac_id, int cred);
int skw_sync_chip_info(struct wiphy *wiphy, struct skw_chip_info *chip);
int skw_sync_cmd_event_version(struct wiphy *wiphy);
void skw_get_dev_ip(struct net_device *ndev);
void skw_set_ip_to_fw(struct wiphy *wiphy, struct net_device *ndev);
int skw_calib_download(struct wiphy *wiphy, const char *fname);
struct skw_ctx_entry *skw_get_ctx_entry(struct skw_core *skw, const u8 *addr);
void skw_init_mib(struct wiphy *wiphy);
#endif
