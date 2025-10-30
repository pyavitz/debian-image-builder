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

#ifndef __SKW_EDMA_H__
#define __SKW_EDMA_H__

#define SKW_NR_EDMA_NODE                                32
#define SKW_NR_EDMA_ELEMENT                             64
#define SKW_EDMA_DATA_LEN                               768
#define SKW_EDMA_SKB_DATA_LEN                           2048

#define SKW_EDMA_WIFI_CMD_CHN                           14
#define SKW_EDMA_WIFI_SHORT_EVENT_CHN                   15
#define SKW_EDMA_WIFI_LONG_EVENT_CHN                    16
#define SKW_EDMA_WIFI_RX0_FITER_CHN                     17
#define SKW_EDMA_WIFI_RX1_FITER_CHN                     18
#define SKW_EDMA_WIFI_TX0_CHN                           19
#define SKW_EDMA_WIFI_TX1_CHN                           20
#define SKW_EDMA_WIFI_TX0_FREE_CHN                      21
#define SKW_EDMA_WIFI_TX1_FREE_CHN                      22
#define SKW_EDMA_WIFI_RX0_FREE_CHN                      23
#define SKW_EDMA_WIFI_RX1_FREE_CHN                      24
#define SKW_EDMA_WIFI_RX0_CHN                           25
#define SKW_EDMA_WIFI_RX1_CHN                           26


#define SKW_EDMA_EVENT_CHN_NODE_NUM                     2
#define SKW_EDMA_FILTER_CHN_NODE_NUM                    8
#define SKW_EDMA_TX_CHN_NODE_NUM                        64
#define SKW_EDMA_TX_FREE_CHN_NODE_NUM                   16
#define SKW_EDMA_RX_CHN_NODE_NUM                        32
#define SKW_EDMA_RX_FREE_CHN_NODE_NUM                   24

#define SKW_EDMA_TX_CHN_CREDIT                          16

#define TX_BUF_ADDR_CNT                                 64
#define TX_FREE_BUF_ADDR_CNT                            64
#define RX_PKT_ADDR_BUF_CNT                             64
#define RX_FREE_BUF_ADDR_CNT                            32

#define SKW_EDMA_HEADR_RESVD                            8
#define skw_dma_to_pcie(addr)                           ((addr) + 0x8000000000)
#define skw_pcie_to_dma(addr)                           ((addr) - 0x8000000000)
#define skw_pcie_to_va(addr)                            phys_to_virt(skw_pcie_to_dma(addr))

typedef int (*skw_edma_isr)(void *priv, u64 first_pa, u64 last_pa, int cnt);
typedef int (*skw_edma_empty_isr)(void *priv);

enum SKW_EDMA_DIRECTION {
	SKW_HOST_TO_FW = 0,
	SKW_FW_TO_HOST,
};

enum SKW_EDMA_CHN_PRIORITY {
	SKW_EDMA_CHN_PRIORITY_0,
	SKW_EDMA_CHN_PRIORITY_1,
	SKW_EDMA_CHN_PRIORITY_2,
	SKW_EDMA_CHN_PRIORITY_3
};

enum SKW_EDMA_CHN_BUFF_ATTR {
	SKW_EDMA_CHN_BUFF_LINNER,
	SKW_EDMA_CHN_BUFF_NON_LINNER,
};

enum SKW_EDMA_CHN_BUFF_TYPE {
	SKW_EDMA_CHN_LIST_BUFF,
	SKW_EDMA_CHN_RING_BUFF
};

enum SKW_EDMA_CHN_TRANS_MODE {
	SKW_EDMA_CHN_STD_MODE,
	SKW_EDMA_CHN_LINKLIST_MODE,
};

struct skw_edma_elem {
	u64 pa:40;
	u64 rsv:8;

	u64 eth_type:16;

	u8 id_rsv:2;
	u8 mac_id:2;
	u8 tid:4;

	u8 peer_idx:5;
	u8 prot:1;
	u8 encry_dis:1;
	u8 rate:1;

	u16 msdu_len:12;
	u16 resv:4;
} __packed;

struct skw_edma_hdr {
	u64 pcie_addr:40;
	u64 rsv0:16;
	u64 tx_int:1;
	u64 rsv1:6;
	u64 done:1;

	u64 hdr_next:40;
	u64 rsv2:8;
	u64 data_len:16;
} __packed;

struct skw_edma_node {
	struct list_head list;
	void *buffer;
	dma_addr_t dma_addr;
	int buffer_len;
	u16 used;
	u16 node_id;
};

struct skw_edma_chn {
	struct skw_core *skw;
	struct skw_lmac *lmac;
	struct list_head node_list;
	struct skw_edma_hdr *hdr;
	struct skw_edma_node *current_node;
	atomic_t nr_node;
	dma_addr_t edma_hdr_pa;
	u16 edma_hdr_size;
	u16 n_pld_size;
	u16 swtail;
	u16 channel;
	u16 max_node_num;
	u16 tx_node_count;
	u16 rx_node_count;
	u16 direction;
	atomic_t chn_refill;
	spinlock_t edma_chan_lock;
};

#ifdef CONFIG_SWT6621S_EDMA
int skw_edma_init(struct wiphy *wiphy);
void skw_edma_deinit(struct wiphy *wiphy);
int skw_edma_set_data(struct wiphy *wiphy, struct skw_edma_chn *edma,
			void *data, int len);
int skw_edma_tx(struct wiphy *wiphy, struct skw_edma_chn *edma, int tx_len);
int skw_edma_init_data_chan(void *priv, u8 lmac_id);
int skw_edma_get_refill(void *priv, u8 lmac_id);
void skw_edma_inc_refill(void *priv, u8 lmac_id);
void skw_edma_dec_refill(void *priv, u8 lmac_id);
bool skw_edma_is_txc_completed(struct skw_core *skw);
void skw_edma_mask_irq(struct skw_core *skw, u8 lmac_id);
void skw_edma_unmask_irq(struct skw_core *skw, u8 lmac_id);

static inline u16 skw_edma_hdr_tail(struct skw_edma_chn *edma_chn)
{
	return (edma_chn->swtail + 1) % edma_chn->max_node_num;
}
#else
static inline int skw_edma_init(struct wiphy *wiphy)
{
	return 0;
}

static inline void skw_edma_deinit(struct wiphy *wiphy)
{
}

static inline int skw_edma_set_data(struct wiphy *wiphy,
		struct skw_edma_chn *edma, void *data, int len)
{
	return 0;
}

static inline int skw_edma_tx(struct wiphy *wiphy,
		struct skw_edma_chn *edma, int tx_len)
{
	return 0;
}

static inline int skw_edma_init_data_chan(void *priv, u8 lmac_id)
{
	return 0;
}

static inline int skw_edma_get_refill(void *priv, u8 lmac_id)
{
	return 0;
}

static inline void skw_edma_inc_refill(void *priv, u8 lmac_id)
{
}

static inline void skw_edma_dec_refill(void *priv, u8 lmac_id)
{
}

static inline u16 skw_edma_hdr_tail(struct skw_edma_chn *edma_chn)
{
	return (edma_chn->swtail + 1) % edma_chn->max_node_num;
}

static inline bool skw_edma_is_txc_completed(struct skw_core *skw)
{
	return true;
}

static inline void skw_edma_mask_irq(struct skw_core *skw, u8 lmac_id)
{
}

static inline void skw_edma_unmask_irq(struct skw_core *skw, u8 lmac_id)
{
}

#endif
#endif
