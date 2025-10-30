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

#include <linux/kernel.h>
#include <linux/percpu-defs.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
#include <linux/dma-direct.h>
#else
#include <linux/dma-mapping.h>
#endif

#include "skw_core.h"
#include "skw_compat.h"
#include "skw_edma.h"
#include "skw_util.h"
#include "skw_log.h"
#include "skw_msg.h"
#include "skw_rx.h"
#include "skw_tx.h"
#include "trace.h"

static struct kmem_cache *skw_edma_node_cache;
static DEFINE_PER_CPU(struct page_frag_cache, skw_edma_alloc_cache);

static void *skw_edma_alloc_frag(size_t fragsz, gfp_t gfp_mask)
{
	struct page_frag_cache *nc;
	unsigned long flags;
	void *data;

	local_irq_save(flags);
	nc = this_cpu_ptr(&skw_edma_alloc_cache);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	data = page_frag_alloc(nc, fragsz, gfp_mask);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	data = __alloc_page_frag(nc, fragsz, gfp_mask);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	data = napi_alloc_frag(fragsz);
#else
	data = NULL;
#endif

	local_irq_restore(flags);

	return data;
}

static int skw_lmac_show(struct seq_file *seq, void *data)
{
	struct skw_lmac *lmac = seq->private;
	struct skw_edma_node *tmp;
	struct list_head *pos, *n;
	struct sk_buff *skb, *skb_tmp;
	struct skw_core *skw = lmac->skw;

	seq_printf(seq, "edma_free_list len:%d avail_skb len:%d rx_dat_q len:%d\n",
		READ_ONCE(lmac->edma_free_list.qlen),
		READ_ONCE(lmac->avail_skb.qlen),
		READ_ONCE(lmac->rx_dat_q.qlen));

	skw_chip_detail(skw->idx, "each skb address of avail_skb queue\n");
	skb_queue_walk_safe(&lmac->avail_skb, skb, skb_tmp)
		skw_chip_detail(skw->idx, "skb->data:%p\n", skb->data);

	skw_chip_detail(skw->idx, "each skb address of edma_free_list queue\n");
	skb_queue_walk_safe(&lmac->edma_free_list, skb, skb_tmp)
		skw_chip_detail(skw->idx, "SKW_SKB_TXCB(skb)->e.pa:%llx\n", (u64)SKW_SKB_TXCB(skb)->e.pa);

	seq_printf(seq, "edma_tx_chn Info: current_node:%d nr_node:%d\n",
		lmac->skw->edma.tx_chn[lmac->id].current_node->node_id,
		atomic_read(&lmac->skw->edma.tx_chn[lmac->id].nr_node));
	list_for_each_safe(pos, n, &lmac->skw->edma.tx_chn[lmac->id].node_list) {
		tmp = list_entry(pos, struct skw_edma_node, list);
		seq_printf(seq, "  node_id:%d used:%d dma_addr:%pad\n",
			tmp->node_id, tmp->used, &tmp->dma_addr);
	}

	seq_puts(seq, "\n");
	seq_printf(seq, "avail_skb queue len : %d\n",
		READ_ONCE(lmac->avail_skb.qlen));
	seq_printf(seq, "edma_rx_req_chn Info: current_node:%d nr_node:%d avail_skb_num:%d\n",
		lmac->skw->edma.rx_req_chn[lmac->id].current_node->node_id,
		atomic_read(&lmac->skw->edma.rx_req_chn[lmac->id].nr_node),
		atomic_read(&lmac->avail_skb_num));
	list_for_each_safe(pos, n, &lmac->skw->edma.rx_req_chn[lmac->id].node_list) {
		tmp = list_entry(pos, struct skw_edma_node, list);
		seq_printf(seq, "  node_id:%d used:%d dma_addr:%pad\n",
			tmp->node_id, tmp->used, &tmp->dma_addr);
	}

	seq_puts(seq, "\n");
	seq_printf(seq, "edma_filter_ch Info: current_node:%d nr_node:%d rx_node_count:%d\n",
		lmac->skw->edma.filter_chn[lmac->id].current_node->node_id,
		atomic_read(&lmac->skw->edma.filter_chn[lmac->id].nr_node),
		lmac->skw->edma.filter_chn[lmac->id].rx_node_count);

	list_for_each_safe(pos, n, &lmac->skw->edma.filter_chn[lmac->id].node_list) {
		tmp = list_entry(pos, struct skw_edma_node, list);
		seq_printf(seq, "  node_id:%d dma_addr:%pad\n",
			tmp->node_id, &tmp->dma_addr);
	}

	seq_puts(seq, "\n");
	seq_printf(seq, "edma_rx_chn Info: current_node:%d nr_node:%d rx_node_count:%d\n",
		lmac->skw->edma.rx_chn[lmac->id].current_node->node_id,
		atomic_read(&lmac->skw->edma.rx_chn[lmac->id].nr_node),
		lmac->skw->edma.rx_chn[lmac->id].rx_node_count);

	list_for_each_safe(pos, n, &lmac->skw->edma.rx_chn[lmac->id].node_list) {
		tmp = list_entry(pos, struct skw_edma_node, list);
		seq_printf(seq, "  node_id:%d dma_addr:%pad\n",
			tmp->node_id, &tmp->dma_addr);
	}

	return 0;
}

static int skw_lmac_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_lmac_show, inode->i_private);
}

static const struct file_operations skw_lmac_fops = {
	.owner = THIS_MODULE,
	.open = skw_lmac_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int skw_edma_alloc_skb(struct skw_core *skw, struct skw_edma_chn *edma, int num, int lmac_id)
{
	int i;
	struct wiphy *wiphy = priv_to_wiphy(skw);
	struct device *dev = priv_to_wiphy(skw)->dev.parent;
	u64 skb_pcie_addr;
	dma_addr_t skb_dma_addr;
	struct sk_buff *skb;
	struct skw_lmac *lmac;

	if (unlikely(!dev)) {
		skw_chip_err(skw->idx, "dev is null\n");
		return -ENODEV;
	}

	edma->tx_node_count = 0;
	lmac = &skw->hw.lmac[lmac_id];
	for (i = 0; i < num; i++) {
		skb = dev_alloc_skb(SKW_EDMA_SKB_DATA_LEN);
		if (!skb) {
			skw_chip_err(skw->idx, "alloc skb addr fail!\n");
			return -ENOMEM;
		}

		skb_put(skb, SKW_EDMA_SKB_DATA_LEN);

		skb_dma_addr = skw_pci_map_single(skw, skb->data,
			SKW_EDMA_SKB_DATA_LEN, DMA_FROM_DEVICE);
		if (unlikely(skw_pcie_mapping_error(skw, skb_dma_addr))) {
			skw_chip_err(skw->idx, "dma mapping error :%d\n", __LINE__);
			BUG_ON(1);
		}

		skb_pcie_addr = skw_dma_to_pcie(skb_dma_addr);
		skw_edma_set_data(wiphy, edma, &skb_pcie_addr, sizeof(u64));
		skb_queue_tail(&lmac->avail_skb, skb);
	}

	skw_chip_detail(skw->idx, "used:%d edma->tx_node_count:%d num:%d\n",
		edma->current_node->used, edma->tx_node_count, num);
	if (skw->hw_pdata->submit_list_to_edma_channel(edma->channel,
		0, edma->tx_node_count) < 0)
		skw_chip_err(skw->idx, "submit_list_to_edma_channel failed\n");
	else
		atomic_add(num, &lmac->avail_skb_num);
	edma->tx_node_count = 0;

	return 0;
}

static inline void skw_edma_reset_refill(void *priv, u8 lmac_id)
{
	struct skw_core *skw = (struct skw_core *)priv;
	struct skw_edma_chn *edma = NULL;

	edma = &skw->edma.rx_req_chn[lmac_id];
	atomic_set(&edma->chn_refill, 0);
}

bool skw_edma_is_txc_completed(struct skw_core *skw)
{
	u8 lmac_id;
	struct skw_lmac *lmac;

	for (lmac_id = 0; lmac_id < skw->hw.nr_lmac; lmac_id++) {
		lmac = &skw->hw.lmac[lmac_id];

		if (lmac != NULL &&
			skw_lmac_is_actived(skw, lmac_id)) {
			spin_lock(&lmac->edma_free_list.lock);
			if (!skb_queue_empty(&lmac->edma_free_list)) {
				skw_chip_dbg(skw->idx, "txc is not completed");
				spin_unlock(&lmac->edma_free_list.lock);
				return false;
			}
			spin_unlock(&lmac->edma_free_list.lock);
		}
	}

	return true;
}

void skw_edma_inc_refill(void *priv, u8 lmac_id)
{
	struct skw_core *skw = (struct skw_core *)priv;
	struct skw_edma_chn *edma = NULL;

	edma = &skw->edma.rx_req_chn[lmac_id];
	atomic_inc(&edma->chn_refill);
}

void skw_edma_dec_refill(void *priv, u8 lmac_id)
{
	struct skw_core *skw = (struct skw_core *)priv;
	struct skw_edma_chn *edma  = NULL;

	edma = &skw->edma.rx_req_chn[lmac_id];

	if (atomic_read(&edma->chn_refill) > 0)
		atomic_dec(&edma->chn_refill);

	if (atomic_read(&edma->chn_refill) == 0)
		skw_edma_unmask_irq(skw, lmac_id);
}

int skw_edma_get_refill(void *priv, u8 lmac_id)
{
	struct skw_core *skw = (struct skw_core *)priv;
	struct skw_edma_chn *edma  = NULL;

	edma = &skw->edma.rx_req_chn[lmac_id];
	return atomic_read(&edma->chn_refill);
}

static inline struct skw_edma_chn *skw_edma_get_refill_chan(void *priv, u8 lmac_id)
{
	struct skw_core *skw = (struct skw_core *)priv;

	return &skw->edma.rx_req_chn[lmac_id];
}

static void skw_edma_refill_skb(struct skw_core *skw, u8 lmac_id)
{
	u32 total = RX_FREE_BUF_ADDR_CNT * SKW_EDMA_RX_FREE_CHN_NODE_NUM; //TBD:
	u16 avail_skb_num = atomic_read(&skw->hw.lmac[lmac_id].avail_skb_num);
	struct skw_edma_chn *refill_chn = skw_edma_get_refill_chan((void *)skw, lmac_id);

	if (total - avail_skb_num >= RX_FREE_BUF_ADDR_CNT
		&& skw_edma_get_refill((void *)skw, lmac_id) > 0)
		skw_edma_alloc_skb(skw, refill_chn,
			round_down(total - avail_skb_num, RX_FREE_BUF_ADDR_CNT), lmac_id);
}

static inline void skw_dma_free_coherent(struct skw_core *skw,
		dma_addr_t *dma_handle, void *cpu_addr, size_t size)
{
	struct device *dev = priv_to_wiphy(skw)->dev.parent;

	dma_free_coherent(dev, size, cpu_addr, *dma_handle);
}

static inline void *skw_dma_alloc_coherent(struct skw_core *skw,
		dma_addr_t *dma_handle, size_t size, gfp_t flag)
{
	struct device *dev = priv_to_wiphy(skw)->dev.parent;

	return dma_alloc_coherent(dev, size, dma_handle, flag);
}

struct skw_edma_node *skw_edma_next_node(struct skw_edma_chn *chn)
{
	unsigned long flags;

	chn->current_node->dma_addr = skw_pci_map_single(chn->skw,
				chn->current_node->buffer,
				chn->current_node->buffer_len, DMA_TO_DEVICE);
	spin_lock_irqsave(&chn->edma_chan_lock, flags);

	if (list_is_last(&chn->current_node->list, &chn->node_list)) {
		chn->current_node = list_first_entry(&chn->node_list,
				struct skw_edma_node, list);
	} else {
		chn->current_node = list_next_entry(chn->current_node, list);
	}

	chn->current_node->used = 0;
	chn->tx_node_count++;
	atomic_dec(&chn->nr_node);

	spin_unlock_irqrestore(&chn->edma_chan_lock, flags);

	return chn->current_node;
}

int skw_edma_set_data(struct wiphy *wiphy, struct skw_edma_chn *edma,
		void *data, int len)
{
	struct skw_edma_node *node = edma->current_node;
	unsigned long flags;
	u8 *buff = NULL;

	spin_lock_irqsave(&edma->edma_chan_lock, flags);
	buff = (u8 *)node->buffer;
	//skw_chip_dbg(skw->idx, "chan:%d node_id:%d node->used:%d buff:%px +used:%px\n",
		//edma->channel, node->node_id, node->used, buff,
		//(buff + node->used));
	//skw_chip_dbg(skw->idx, "data:%px\n", data);
	memcpy(buff + node->used, data, len);
	//skw_chip_dbg(skw->idx, "%d channel:%d node:%px\n", __LINE__, edma->channel, node);
	node->used += len;
	edma->hdr[node->node_id].data_len = node->used;
	spin_unlock_irqrestore(&edma->edma_chan_lock, flags);
	BUG_ON(len > node->buffer_len);
	if (node->used + len > node->buffer_len)
		node = skw_edma_next_node(edma);

	return 0;
}

int skw_edma_tx(struct wiphy *wiphy, struct skw_edma_chn *edma, int tx_len)
{
	int tx_count;
	struct skw_core *skw = wiphy_priv(wiphy);
	u64 pa = 0;

	if (edma->current_node->used)
		skw_edma_next_node(edma);
	tx_count = edma->tx_node_count;
	pa = edma->hdr->hdr_next;
	//skw_chip_dbg(skw->idx, "channel:%d tx_node_count:%d pa:0x%llx\n",
		//edma->channel, tx_count, pa);
	edma->tx_node_count = 0;

	return  skw->hw_pdata->hw_adma_tx(edma->channel, NULL,
					tx_count, tx_len);
}

int skw_edma_init_data_chan(void *priv, u8 lmac_id)
{
	int ret = 0;
	unsigned long flags;
	struct skw_core *skw = priv;
	struct skw_edma_chn *edma_chn = NULL;

	edma_chn = &skw->edma.filter_chn[lmac_id];

	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);

	ret = skw->hw_pdata->submit_list_to_edma_channel(edma_chn->channel,
		0, edma_chn->rx_node_count);

	edma_chn->rx_node_count = 0;
	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	edma_chn = &skw->edma.rx_chn[lmac_id];
	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);

	ret = skw->hw_pdata->submit_list_to_edma_channel(edma_chn->channel,
		0,
		//(void *)(skw_dma_to_pcie(edma_chn->edma_hdr_pa) + 8),
		edma_chn->rx_node_count);
	edma_chn->rx_node_count = 0;
	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	edma_chn = &skw->edma.tx_resp_chn[lmac_id];
	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);
	ret = skw->hw_pdata->submit_list_to_edma_channel(edma_chn->channel,
		0,
		//(void *)(skw_dma_to_pcie(edma_chn->edma_hdr_pa) + 8),
		edma_chn->rx_node_count);
	edma_chn->rx_node_count = 0;
	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	edma_chn = &skw->edma.rx_req_chn[lmac_id];
	skw_edma_alloc_skb(skw, edma_chn,
		RX_FREE_BUF_ADDR_CNT * SKW_EDMA_RX_FREE_CHN_NODE_NUM, lmac_id);

	skw_edma_inc_refill((void *)skw, lmac_id);

	return ret;
}

static inline int skw_submit_edma_chn(struct skw_core *skw, int chn,
		u64 pcie_addr, int count)
{
	if (!skw->hw_pdata || !skw->hw_pdata->submit_list_to_edma_channel)
		return -ENOTSUPP;

	return skw->hw_pdata->submit_list_to_edma_channel(chn, (u64)pcie_addr, count);
}

static void skw_edma_chn_deinit(struct skw_core *skw, struct skw_edma_chn *edma)
{
	struct skw_edma_node *node = NULL, *tmp = NULL;
	unsigned long flags;
	u8 direction = edma->direction ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	// TODO: stop edma channel transmit

	if (!edma) {
		skw_chip_err(skw->idx, "emda is null\n");
		return;
	}

	skw_chip_dbg(skw->idx, "chan: %d\n", edma->channel);

	spin_lock_irqsave(&edma->edma_chan_lock, flags);
	skw->hw_pdata->hw_channel_deinit(edma->channel);
	list_for_each_entry_safe(node, tmp, &edma->node_list, list) {
		list_del(&node->list);
		skw_chip_detail(skw->idx, "channel: %d, node_id: %d, buffer: 0x%p, buffer_pa: 0x%pad\n",
			edma->channel, node->node_id, node->buffer, &node->dma_addr);

		if (node->dma_addr)
			skw_pci_unmap_single(skw, node->dma_addr, node->buffer_len,
					direction);

		if (direction == DMA_FROM_DEVICE)
			skw_compat_page_frag_free(node->buffer);
		else
			SKW_KFREE(node->buffer);

		kmem_cache_free(skw_edma_node_cache, node);
	}

	edma->current_node = NULL;
	atomic_set(&edma->nr_node, 0);
	spin_unlock_irqrestore(&edma->edma_chan_lock, flags);

	skw_dma_free_coherent(skw, &edma->edma_hdr_pa, (void *)edma->hdr,
				edma->edma_hdr_size);
}

static int skw_edma_chn_init(struct skw_core *skw, struct skw_lmac *lmac,
			     struct skw_edma_chn *edma, int channel,
			     int max_node, int node_buff_len,
			     enum SKW_EDMA_DIRECTION direction,
			     skw_edma_isr isr, int irq_threshold,
			     enum SKW_EDMA_CHN_PRIORITY priority,
			     enum SKW_EDMA_CHN_BUFF_ATTR attr,
			     enum SKW_EDMA_CHN_BUFF_TYPE buff_type,
			     enum SKW_EDMA_CHN_TRANS_MODE trans_mode,
			     bool submit_immediately)
{
	int i, ret;
	gfp_t flags;
	u64 hdr_start;
	struct skw_edma_node *node;
	struct skw_channel_cfg cfg;
	enum dma_data_direction dma_direction;
	int hdr_size = sizeof(struct skw_edma_hdr);
	struct device *dev = priv_to_wiphy(skw)->dev.parent;
	void *(*skw_alloc_func)(size_t len, gfp_t gfp);

	skw_chip_dbg(skw->idx, "%d channel:%d edma->edma_hdr_pa:%pad\n", __LINE__, channel,
			&edma->edma_hdr_pa);

	if (direction == SKW_FW_TO_HOST) {
		flags = GFP_ATOMIC;
		dma_direction = DMA_FROM_DEVICE;
		skw_alloc_func = skw_edma_alloc_frag;
	} else {
		flags = GFP_DMA;
		dma_direction = DMA_TO_DEVICE;
		skw_alloc_func = kzalloc;
	}

	INIT_LIST_HEAD(&edma->node_list);
	spin_lock_init(&edma->edma_chan_lock);
	atomic_set(&edma->nr_node, max_node);

	edma->skw = skw;
	edma->lmac = lmac;
	edma->direction = direction;
	edma->n_pld_size = node_buff_len;
	edma->max_node_num = max_node;
	edma->channel = channel;
	edma->tx_node_count = 0;
	edma->rx_node_count = 0;
	edma->swtail = 0;
	edma->edma_hdr_size = PAGE_ALIGN(max_node * hdr_size);

	edma->hdr = dma_alloc_coherent(dev, edma->edma_hdr_size, &edma->edma_hdr_pa, GFP_DMA);
	if (!edma->hdr)
		return -ENOMEM;

	memset((void *)edma->hdr, 0x6a, edma->edma_hdr_size);

	hdr_start = SKW_EDMA_HEADR_RESVD + skw_dma_to_pcie(edma->edma_hdr_pa);

	for (i = 0; i < max_node; i++) {
		node = kmem_cache_alloc(skw_edma_node_cache, GFP_KERNEL);
		if (!node)
			goto node_failed;

		node->buffer = skw_alloc_func(node_buff_len, flags);
		if (!node->buffer)
			goto node_failed;

		memset(node->buffer, 0x5a, node_buff_len);

		node->used = 0;
		node->node_id = i;
		node->buffer_len = node_buff_len;
		node->dma_addr = skw_pci_map_single(skw, node->buffer,
					node_buff_len, dma_direction);

		INIT_LIST_HEAD(&node->list);
		list_add_tail(&node->list, &edma->node_list);

		edma->hdr[i].hdr_next = hdr_start + ((i + 1) % max_node) * hdr_size;
		edma->hdr[i].pcie_addr = skw_dma_to_pcie(node->dma_addr);

		if (channel == SKW_EDMA_WIFI_RX0_FREE_CHN || channel == SKW_EDMA_WIFI_RX1_FREE_CHN)
			edma->hdr[i].data_len = node_buff_len;
	}

	edma->current_node = list_first_entry(&edma->node_list,
				struct skw_edma_node, list);

	memset(&cfg, 0, sizeof(struct skw_channel_cfg));
	cfg.priority = priority;
	cfg.split = attr;
	cfg.ring = buff_type;
	cfg.req_mode = trans_mode;
	cfg.irq_threshold = irq_threshold;
	cfg.node_count = max_node;
	cfg.header = hdr_start;
	cfg.complete_callback = isr;
	cfg.direction = direction;
	cfg.context = edma;

	ret = skw->hw_pdata->hw_channel_init(channel, &cfg, NULL);

	if (submit_immediately)
		ret = skw_submit_edma_chn(skw, channel, hdr_start, max_node);

	return ret;

node_failed:
	skw_edma_chn_deinit(skw, edma);

	return -ENOMEM;
}

static int
skw_edma_tx_node_isr(void *priv, u64 first_pa, u64 last_pa, int count)
{
	struct skw_edma_chn *edma_chn = priv;
	struct skw_core *skw = edma_chn->skw;
	struct skw_edma_hdr *edma_hdr = NULL;
	int i = 0;
	u64 pa = 0, hdr_next = 0;
	int offset = 0;
	unsigned long flags;

	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);
	hdr_next = edma_chn->hdr->hdr_next;
	offset = skw_pcie_to_dma(first_pa) - 8 - edma_chn->edma_hdr_pa;

	edma_hdr = (struct skw_edma_hdr *) ((u8 *)edma_chn->hdr + offset);

	//skw_chip_dbg(skw->idx, "edma_hdr:%p\n", edma_hdr);
	while (i < count) {
		pa = edma_hdr->pcie_addr; //pcie address
		//skw_chip_dbg(skw->idx, "i:%d edma pcie addr:0x%llx, phy addrs:0x%llx\n",
		//		i, pa, skw_pcie_to_dma(pa));
		skw_pci_unmap_single(skw,
			skw_pcie_to_dma(edma_hdr->pcie_addr),
			edma_chn->current_node->buffer_len, DMA_TO_DEVICE);
		atomic_inc(&edma_chn->nr_node);
		edma_hdr++;
		i++;
	}
	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	return 0;
}

static inline void *skw_edma_coherent_rcvheader_to_cpuaddr(u64 rcv_pcie_addr,
	struct skw_edma_chn *edma_chn)
{
	u32 offset;
	u64 *cpu_addr;

	offset = rcv_pcie_addr - edma_chn->edma_hdr_pa;
	cpu_addr = (u64 *)((u64)edma_chn->hdr + offset);

	return cpu_addr;
}

int skw_edma_txrx_isr(void *priv, u64 first_pa, u64 last_pa, int count)
{
	struct skw_edma_chn *edma_chn = priv;

	if (unlikely(priv == NULL)) {
		skw_err("Ignore spurious isr\n");
		return 0;
	}

	skw_chip_detail(edma_chn->skw->idx, "call channel:%d\n", edma_chn->channel);
	//skw->hw_pdata->edma_mask_irq(edma_chn->channel);

	if (edma_chn->channel == SKW_EDMA_WIFI_TX0_FREE_CHN ||
	    edma_chn->channel == SKW_EDMA_WIFI_TX1_FREE_CHN)
		napi_schedule(&edma_chn->lmac->napi_tx);
	else if (edma_chn->channel == SKW_EDMA_WIFI_RX0_CHN ||
		edma_chn->channel == SKW_EDMA_WIFI_RX1_CHN ||
		edma_chn->channel == SKW_EDMA_WIFI_RX0_FITER_CHN ||
		edma_chn->channel == SKW_EDMA_WIFI_RX1_FITER_CHN) {
		napi_schedule(&edma_chn->lmac->napi_rx);
	}

	return 0;
}

static void skw_pci_edma_tx_free(struct skw_core *skw,
		struct sk_buff_head *free_list, void *data, u16 data_len)
{
	int count;
	unsigned long flags;
	struct sk_buff *skb;
	struct sk_buff_head qlist;
	u64 *p = (u64 *) data;
	u64 p_data = 0;
	unsigned long *skb_addr, *addr;
	struct skw_edma_node *node;

	__skb_queue_head_init(&qlist);

	spin_lock_irqsave(&free_list->lock, flags);
	skb_queue_splice_tail_init(free_list, &qlist);
	spin_unlock_irqrestore(&free_list->lock, flags);

	trace_skw_tx_pcie_edma_free(data_len/8);
	for (count = 0; count < data_len; count = count + 8, p++) {
		p_data = *p & 0xFFFFFFFFFF;
		skw_chip_detail(skw->idx, "p_data:%llx\n", p_data);

		skb_addr = skw_pcie_to_va(p_data) - 2 - sizeof(unsigned long);
		skb = (struct sk_buff *)*skb_addr;
		if (unlikely(skb < (struct sk_buff *)PAGE_OFFSET)) {
			/* Invalid skb pointer */
			skw_chip_dbg(skw->idx, "wrong address p_data:0x%llx from FW\n", p_data);
			continue;
		}
		addr = skw_pcie_to_va(p_data) - 2 - sizeof(unsigned long)*2;
		node = (struct skw_edma_node *)*addr;
		if (node->dma_addr) {
			skw_pci_unmap_single(skw,
				skw_pcie_to_dma(node->dma_addr),
				node->buffer_len, DMA_TO_DEVICE);
			node->dma_addr = 0;
			skw->hw_pdata->edma_clear_src_node_count(SKW_EDMA_WIFI_TX0_CHN + SKW_SKB_TXCB(skb)->lmac_id);
		}

		__skb_unlink(skb, &qlist);
		skw_pci_unmap_single(skw, SKW_SKB_TXCB(skb)->skb_data_pa,
			skb->len, DMA_TO_DEVICE);
		skb->dev->stats.tx_packets++;
		skb->dev->stats.tx_bytes += SKW_SKB_TXCB(skb)->skb_native_len;
		//dev_kfree_skb_any(skb);
		skw_skb_kfree(skw, skb);
	}

	if (qlist.qlen) {
		spin_lock_irqsave(&free_list->lock, flags);
		skb_queue_splice_tail_init(&qlist, free_list);
		spin_unlock_irqrestore(&free_list->lock, flags);
	}
}

static struct sk_buff *
skw_check_skb_address_available(struct skw_core *skw, u8 lmac_id, u64 addr)
{
	struct sk_buff *skb, *tmp;

	skb_queue_walk_safe(&skw->hw.lmac[lmac_id].avail_skb, skb, tmp) {
		skw_chip_detail(skw->idx, "lmac_id: %d, skb->data: 0x%p, addr_pcie: %llx, addr_vir: 0x%p",
			lmac_id, skb->data, addr, skw_pcie_to_va(addr));
		if (skb->data == skw_pcie_to_va(addr)) {
			skb_unlink(skb, &skw->hw.lmac[lmac_id].avail_skb);
			return skb;
		}
	}

	return NULL;
}

static void skw_pci_edma_rx_data(struct skw_edma_chn *edma_chn, void *data, int data_len)
{
	struct skw_core *skw = edma_chn->skw;
	struct skw_rx_desc *desc = NULL;
	struct sk_buff *skb;
	struct skw_skb_rxcb *cb = NULL;
	int i, total_len;
	//u64 p_data = 0;
	u64 *p = NULL;

	for (i = 0; i < data_len; i += 8) {
		p = (u64 *)((u8 *)data + i);

		skb = skw_check_skb_address_available(skw, edma_chn->lmac->id, *p & 0xFFFFFFFFFF);
		if (!skb) {
			skw_chip_dbg(skw->idx, "wrong rx data from CP %llx\n", *p & 0xFFFFFFFFFF);
			skw_chip_warn(skw->idx, "rxc node address:%llx\n", virt_to_phys(data));
			skw_hw_assert(skw, false);
			continue;
		}

		cb = SKW_SKB_RXCB(skb);
		cb->lmac_id = edma_chn->lmac->id;

		skw_pci_unmap_single(skw,
				skw_pcie_to_dma(*p & 0xFFFFFFFFFF),
				skb->len, DMA_FROM_DEVICE);
		//p_data = skw_pcie_to_va(*p & 0xFFFFFFFFFF);

		//desc = (struct skw_rx_desc *) ((u8 *) (p_data + 52));
		desc = (struct skw_rx_desc *) ((u8 *) (skb->data + 52));

		//FW use this way to return unused buff
		if (unlikely(!desc->msdu_len)) {
			//skw_compat_page_frag_free((void *)p_data);
			skw_chip_detail(skw->idx, "free skb\n");
			kfree_skb(skb);
			atomic_dec(&edma_chn->lmac->avail_skb_num);
			continue;
		}
		atomic_dec(&edma_chn->lmac->avail_skb_num); //TBD: whether to check the value is minus

		if (desc->snap_match)
			total_len = desc->msdu_len + 80;
		else
			total_len = desc->msdu_len + 88;

		if (unlikely(total_len > SKW_ADMA_BUFF_LEN)) {
			skw_hw_assert(skw, false);
			skw_chip_warn(skw->idx, "total len: %d\n", total_len);
			skw_chip_warn(skw->idx, "rxc node address:%llx skb->data:%llx\n", virt_to_phys(data), virt_to_phys(skb->data));
			skw_hex_dump("invalid rx skb:", skb->data, skb->len, true);

			//skw_compat_page_frag_free((void *)p_data);
			//kfree_skb(skb);
			continue;
		}

		skb_trim(skb, total_len);
		skb_pull(skb, 8);
		__net_timestamp(skb);
		skw_hex_dump("rx skb:", skb->data, skb->len, false);

		skb_queue_tail(&edma_chn->lmac->rx_dat_q, skb);
		skw->rx_packets++;
	}
}

static void skw_pci_edma_rx_filter_data(struct skw_core *skw, void *data, int data_len, u8 lmac_id)
{
	struct sk_buff *skb;
	struct skw_skb_rxcb *cb = NULL;
	int total_len;

	total_len = SKB_DATA_ALIGN(data_len) + skw->skb_share_len;

	if (unlikely(total_len > SKW_ADMA_BUFF_LEN)) {
		skw_chip_warn(skw->idx, "total_len: %d\n", total_len);
		skw_compat_page_frag_free(data);
		return;
	}

	skb = build_skb((void *)data, total_len);
	if (!skb) {
		skw_chip_err(skw->idx, "build skb failed, len: %d\n", total_len);
		skw_compat_page_frag_free(data);
		return;
	}

	cb = SKW_SKB_RXCB(skb);
	cb->lmac_id = lmac_id;
	skb_put(skb, data_len);
	__net_timestamp(skb);

	skb_queue_tail(&skw->hw.lmac[lmac_id].rx_dat_q, skb);
	skw->rx_packets++;
}

static void skw_pcie_edma_rx_cb(struct skw_edma_chn *edma, void *data, u16 data_len)
{
	u16 channel = 0;
	int ret = 0, total_len = 0;
	struct skw_core *skw = edma->skw;
	struct skw_iface *iface = NULL;
	struct skw_event_work *work = NULL;
	struct sk_buff *skb = NULL;
	struct skw_msg *msg = NULL;

	channel = edma->channel;

	//skw_chip_dbg(skw->idx, "phy data:0x%llx len:%u\n", virt_to_phys(data), data_len);
	//short & long event channel
	//skw_chip_dbg(skw->idx, "channel:%d\n", channel);
	if (channel == SKW_EDMA_WIFI_SHORT_EVENT_CHN || channel == SKW_EDMA_WIFI_LONG_EVENT_CHN) {
		//skw_hex_dump("rx_cb data", data, 16, 1);

		total_len = SKB_DATA_ALIGN(data_len) + skw->skb_share_len;
		if (unlikely(total_len > SKW_ADMA_BUFF_LEN)) {
			skw_chip_warn(skw->idx, "data: %d\n", data_len);
			skw_compat_page_frag_free(data);
			return;
		}

		skb = build_skb(data, total_len);
		if (!skb) {
			skw_compat_page_frag_free(data);
			skw_chip_err(skw->idx, "build skb failed, len: %d\n", data_len);
			return;
		}
		skb_put(skb, data_len);

		msg = (struct skw_msg *) skb->data;
		switch (msg->type) {
		case SKW_MSG_CMD_ACK:
			skw_cmd_ack_handler(skw, skb->data, skb->len);
			kfree_skb(skb);
			break;

		case SKW_MSG_EVENT:
			if (++skw->skw_event_sn != msg->seq) {
				skw_chip_warn(skw->idx, "invalid event seq:%d, expect:%d\n",
					 msg->seq, skw->skw_event_sn);
				// skw_hw_assert(skw, false);
				// kfree_skb(skb);
				// break;
			}

			if (msg->id == SKW_EVENT_CREDIT_UPDATE) {
				skw_chip_warn(skw->idx, "PCIE doesn't support CREDIT");
				kfree_skb(skb);
				break;
			}

			iface = to_skw_iface(skw, msg->inst_id);
			if (iface)
				work = &iface->event_work;
			else
				work = &skw->event_work;

			ret = skw_queue_event_work(priv_to_wiphy(skw),
						work, skb);
			if (ret < 0) {
				skw_chip_err(skw->idx, "inst: %d, drop event %d\n",
					msg->inst_id, msg->id);
				kfree_skb(skb);
			}
			break;

		default:
			skw_chip_warn(skw->idx, "invalid: type: %d, id: %d, seq: %d\n",
				 msg->type, msg->id, msg->seq);
			kfree_skb(skb);
			break;
		}
	} else if (channel == SKW_EDMA_WIFI_TX0_FREE_CHN ||
			channel == SKW_EDMA_WIFI_TX1_FREE_CHN) {
		struct sk_buff_head *edma_free_list = NULL;

		//skw_chip_dbg(skw->idx, "channel:%d received tx free data\n", channel);
		edma_free_list = &edma->lmac->edma_free_list;

		skw_pci_edma_tx_free(skw, edma_free_list, data, data_len);
	} else if (channel == SKW_EDMA_WIFI_RX0_CHN ||
			channel == SKW_EDMA_WIFI_RX1_CHN) {
		//skw_chip_dbg(skw->idx, "channel:%d received data\n", channel);


		skw_pci_edma_rx_data(edma, data, data_len);
		skw_edma_refill_skb(skw, edma->lmac->id);
	} else if (channel == SKW_EDMA_WIFI_RX0_FITER_CHN ||
			channel == SKW_EDMA_WIFI_RX1_FITER_CHN) {
		//skw_chip_dbg(skw->idx, "channel:%d received filter data\n", channel);
		//skw_hex_dump("filter data", data, data_len, 1);
		skw_pci_edma_rx_filter_data(skw, data, data_len, edma->lmac->id);
	}
}

int skw_edma_napi_txrx_compl_task(void *priv, int *quota)
{
	struct skw_edma_chn *edma_chn = priv;
	u16 channel = edma_chn->channel;
	struct skw_core *skw = edma_chn->skw;
	struct skw_edma_hdr *edma_hdr = NULL;
	unsigned long flags;
	//int times = 0;
	int count = 0;
	void *p_addr = NULL;

	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);

	//while (edma_chn->hdr[edma_chn->swtail].done == 0 && times++ < edma_chn->max_node_num)
	//	edma_chn->swtail = skw_edma_hdr_tail(edma_chn);

	edma_hdr = (struct skw_edma_hdr *)((u8 *)edma_chn->hdr +
		edma_chn->swtail * sizeof(struct skw_edma_hdr));

	while (edma_hdr->done && (*quota > 0)) {
		skw_chip_detail(skw->idx, "channel: %d, node_id: %d, node_pa: %pad, edma_pa: %llx, buffer: %p\n",
			channel, edma_chn->current_node->node_id,
			&edma_chn->current_node->dma_addr, (u64)edma_hdr->pcie_addr,
			edma_chn->current_node->buffer);

		count += edma_hdr->data_len / 8;
		if (*quota >= count)
			*quota -= count;
		else
			*quota = 0;
		edma_chn->swtail = skw_edma_hdr_tail(edma_chn);

		if (edma_chn->current_node->dma_addr) {
			skw_pci_unmap_single(skw,
				skw_pcie_to_dma(edma_hdr->pcie_addr),
				edma_chn->current_node->buffer_len, DMA_FROM_DEVICE);
			edma_chn->current_node->dma_addr = 0;
		}

		skw_pcie_edma_rx_cb(edma_chn, skw_pcie_to_va(edma_hdr->pcie_addr),
				edma_hdr->data_len);
		edma_chn->rx_node_count++;

		if (channel == SKW_EDMA_WIFI_RX0_FITER_CHN ||
			channel ==  SKW_EDMA_WIFI_RX1_FITER_CHN) {

			p_addr = skw_edma_alloc_frag(edma_chn->n_pld_size, GFP_ATOMIC);
			if (!p_addr) {
				skw_chip_err(skw->idx, "Alloc memory for channel:%d failed\n", channel);
				return -ENOMEM;
			}

			edma_chn->current_node->dma_addr = skw_pci_map_single(skw, p_addr,
				edma_chn->n_pld_size, DMA_FROM_DEVICE);
			edma_hdr->pcie_addr = skw_dma_to_pcie(edma_chn->current_node->dma_addr);
			edma_chn->current_node->buffer = p_addr;
		}

		if (edma_chn->rx_node_count == edma_chn->max_node_num) {
			edma_chn->rx_node_count = 0;
			skw->hw_pdata->submit_list_to_edma_channel(
				edma_chn->channel, 0, edma_chn->max_node_num);
		}

		edma_hdr->done = 0;
		edma_hdr++;
		if (edma_chn->current_node->dma_addr == 0)
			edma_chn->current_node->dma_addr = skw_pci_map_single(skw,
				edma_chn->current_node->buffer,
				edma_chn->current_node->buffer_len, DMA_FROM_DEVICE);
		if (list_is_last(&edma_chn->current_node->list,
				&edma_chn->node_list)) {
			edma_chn->current_node = list_first_entry(&edma_chn->node_list,
					struct skw_edma_node, list);
		} else {
			edma_chn->current_node = list_next_entry(edma_chn->current_node,
				list);
		}
	}
	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	return count;
}

static int
skw_edma_rx_node_isr(void *priv, u64 first_pa, u64 last_pa, int count)
{
	struct skw_edma_chn *edma_chn = priv;
	u16 channel = edma_chn->channel;
	struct skw_core *skw = edma_chn->skw;
	struct skw_edma_hdr *edma_hdr = NULL;
	struct skw_edma_hdr *last_edma_hdr = NULL;
	int i = 0;
	u64 hdr_next = 0;
	int offset = 0;
	unsigned long flags;
	void *p_addr = NULL;

	// Wait till tail done bit is set
	last_edma_hdr = (struct skw_edma_hdr *)skw_edma_coherent_rcvheader_to_cpuaddr(((u64)last_pa) - 8, edma_chn);
	while (!last_edma_hdr->done) {
		mdelay(1);
		barrier();
	}

	spin_lock_irqsave(&edma_chn->edma_chan_lock, flags);
	edma_chn->rx_node_count += count;
	hdr_next = edma_chn->hdr->hdr_next;

	offset = skw_pcie_to_dma(first_pa) - 8 - edma_chn->edma_hdr_pa;
	edma_hdr = (struct skw_edma_hdr *)((u8 *)edma_chn->hdr + offset);
	while (i < count) {
		skw_chip_detail(skw->idx, "channel:%d node_id:%d current_node->buffer_pa:%pad edma_hdr->buffer_pa:%llx\n",
			channel, edma_chn->current_node->node_id, &edma_chn->current_node->dma_addr, (u64)edma_hdr->pcie_addr);
		if (edma_chn->current_node->dma_addr) {
			skw_pci_unmap_single(skw,
				skw_pcie_to_dma(edma_hdr->pcie_addr),
				edma_chn->current_node->buffer_len, DMA_FROM_DEVICE);
			edma_chn->current_node->dma_addr = 0;
		}

		skw_pcie_edma_rx_cb(edma_chn,
			(void *)skw_pcie_to_va(edma_hdr->pcie_addr),
			edma_hdr->data_len);

		if (channel == SKW_EDMA_WIFI_SHORT_EVENT_CHN ||
			channel == SKW_EDMA_WIFI_LONG_EVENT_CHN) {

			// This payload of edma channel will be freed in asyn way
			p_addr = skw_edma_alloc_frag(edma_chn->n_pld_size, GFP_ATOMIC);
			if (!p_addr) {
				skw_chip_err(skw->idx, "Alloc memory for channel:%d failed\n", channel);
				return -ENOMEM;
			}

			edma_chn->current_node->dma_addr = skw_pci_map_single(skw, p_addr,
				edma_chn->n_pld_size, DMA_FROM_DEVICE);
			edma_hdr->pcie_addr = skw_dma_to_pcie(edma_chn->current_node->dma_addr);
			edma_chn->current_node->buffer = p_addr;
		} else {
			// This payload will be reused
			// TBD: reset the payload
		}

		if (edma_chn->rx_node_count == edma_chn->max_node_num) { //TBD: How to improve rx data channel
			//skw_chip_dbg(skw->idx, "resubmit for channel:%d\n", edma_chn->channel);
			edma_chn->rx_node_count = 0;
			skw->hw_pdata->submit_list_to_edma_channel(
				edma_chn->channel, 0, edma_chn->max_node_num);
		}

		edma_hdr++;
		i++;
		if (list_is_last(&edma_chn->current_node->list, &edma_chn->node_list)) {
			edma_chn->current_node = list_first_entry(&edma_chn->node_list,
					struct skw_edma_node, list);
		} else {
			edma_chn->current_node = list_next_entry(edma_chn->current_node, list);
		}
	}

	spin_unlock_irqrestore(&edma_chn->edma_chan_lock, flags);

	last_edma_hdr->done = 0;

	return 0;
}

static int skw_netdev_poll_tx(struct napi_struct *napi, int budget)
{
	int quota = budget;
	struct skw_lmac *lmac = container_of(napi, struct skw_lmac, napi_tx);
	struct skw_core *skw = lmac->skw;

	skw_edma_napi_txrx_compl_task(&skw->edma.tx_resp_chn[lmac->id], &quota);

	if (quota) {
		napi_complete(napi);
		skw->hw_pdata->edma_unmask_irq(skw->edma.tx_resp_chn[lmac->id].channel);
		return 0;
	}

	return budget;
}

static int skw_netdev_poll_rx(struct napi_struct *napi, int budget)
{
	int quota = budget;
	struct skw_lmac *lmac = container_of(napi, struct skw_lmac, napi_rx);
	struct skw_core *skw = lmac->skw;

	skw_edma_napi_txrx_compl_task(&skw->edma.rx_chn[lmac->id], &quota);
	skw_edma_napi_txrx_compl_task(&skw->edma.filter_chn[lmac->id], &quota);

	skw_rx_process(skw, &lmac->rx_dat_q, &lmac->rx_todo_list);
	if (lmac->rx_todo_list.count || !quota)
		return budget;

	napi_complete(napi);
	skw->hw_pdata->edma_unmask_irq(skw->edma.rx_chn[lmac->id].channel);
	skw->hw_pdata->edma_unmask_irq(skw->edma.filter_chn[lmac->id].channel);

	return 0;
}

static int skw_edma_cache_init(struct skw_core *skw)
{
	skw_edma_node_cache = kmem_cache_create("skw_edma_node_cache",
						sizeof(struct skw_edma_node),
						0, 0, NULL);
	if (skw_edma_node_cache == NULL)
		return -ENOMEM;

	return 0;
}

static void skw_edma_cache_deinit(struct skw_core *skw)
{
	kmem_cache_destroy(skw_edma_node_cache);
}

int skw_edma_init(struct wiphy *wiphy)
{
	int ret, i;
	struct skw_lmac *lmac = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);
	char name[32] = {0};

	if (skw->hw.bus != SKW_BUS_PCIE)
		return 0;

	ret = skw_edma_cache_init(skw);
	if (ret < 0) {
		skw_chip_err(skw->idx, "edma cached init failed, ret: %d\n", ret);
		return ret;
	}

	// cmd channel
	ret = skw_edma_chn_init(skw, NULL, &skw->edma.cmd_chn,
				SKW_EDMA_WIFI_CMD_CHN,
				1, SKW_MSG_BUFFER_LEN,
				SKW_HOST_TO_FW, skw_edma_tx_node_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, false);

	// short event channel
	ret = skw_edma_chn_init(skw, NULL, &skw->edma.short_event_chn,
				SKW_EDMA_WIFI_SHORT_EVENT_CHN,
				SKW_EDMA_EVENT_CHN_NODE_NUM,
				SKW_MSG_BUFFER_LEN, SKW_FW_TO_HOST,
				skw_edma_rx_node_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_LIST_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// long event channel
	ret = skw_edma_chn_init(skw, NULL, &skw->edma.long_event_chn,
				SKW_EDMA_WIFI_LONG_EVENT_CHN,
				SKW_EDMA_EVENT_CHN_NODE_NUM,
				SKW_MSG_BUFFER_LEN, SKW_FW_TO_HOST,
				skw_edma_rx_node_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_LIST_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// RX0 filter channel
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[0],
				&skw->edma.filter_chn[0],
				SKW_EDMA_WIFI_RX0_FITER_CHN,
				SKW_EDMA_FILTER_CHN_NODE_NUM,
				SKW_MSG_BUFFER_LEN, SKW_FW_TO_HOST,
				skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_LIST_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// RX1 filter channel
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[1],
				&skw->edma.filter_chn[1],
				SKW_EDMA_WIFI_RX1_FITER_CHN,
				SKW_EDMA_FILTER_CHN_NODE_NUM,
				SKW_MSG_BUFFER_LEN, SKW_FW_TO_HOST,
				skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_LIST_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// TX0 chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[0],
				&skw->edma.tx_chn[0],
				SKW_EDMA_WIFI_TX0_CHN,
				SKW_EDMA_TX_CHN_NODE_NUM,
				SKW_EDMA_DATA_LEN, SKW_HOST_TO_FW,
				NULL,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, false);
	skw->hw_pdata->edma_mask_irq(SKW_EDMA_WIFI_TX0_CHN);

	// TX1 chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[1],
				&skw->edma.tx_chn[1],
				SKW_EDMA_WIFI_TX1_CHN,
				SKW_EDMA_TX_CHN_NODE_NUM,
				SKW_EDMA_DATA_LEN, SKW_HOST_TO_FW,
				NULL,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, false);
	skw->hw_pdata->edma_mask_irq(SKW_EDMA_WIFI_TX1_CHN);

	// TX0 free chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[0],
				&skw->edma.tx_resp_chn[0],
				SKW_EDMA_WIFI_TX0_FREE_CHN,
				SKW_EDMA_TX_FREE_CHN_NODE_NUM,
				TX_FREE_BUF_ADDR_CNT * sizeof(long long),
				SKW_FW_TO_HOST, skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// TX1 free chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[1],
				&skw->edma.tx_resp_chn[1],
				SKW_EDMA_WIFI_TX1_FREE_CHN,
				SKW_EDMA_TX_FREE_CHN_NODE_NUM,
				TX_FREE_BUF_ADDR_CNT * sizeof(long long),
				SKW_FW_TO_HOST, skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);
	// RX0 free chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[0],
				&skw->edma.rx_req_chn[0],
				SKW_EDMA_WIFI_RX0_FREE_CHN,
				SKW_EDMA_RX_FREE_CHN_NODE_NUM,
				RX_FREE_BUF_ADDR_CNT * sizeof(long long),
				SKW_HOST_TO_FW, skw_edma_tx_node_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, false);

	// RX1 free chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[1],
				&skw->edma.rx_req_chn[1],
				SKW_EDMA_WIFI_RX1_FREE_CHN,
				SKW_EDMA_RX_FREE_CHN_NODE_NUM,
				RX_FREE_BUF_ADDR_CNT * sizeof(long long),
				SKW_HOST_TO_FW, skw_edma_tx_node_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, false);

	// RX0 chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[0],
				&skw->edma.rx_chn[0],
				SKW_EDMA_WIFI_RX0_CHN,
				SKW_EDMA_RX_CHN_NODE_NUM,
				RX_PKT_ADDR_BUF_CNT * sizeof(long long),
				SKW_FW_TO_HOST,
				skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);

	// RX1 chan
	ret = skw_edma_chn_init(skw, &skw->hw.lmac[1],
				&skw->edma.rx_chn[1],
				SKW_EDMA_WIFI_RX1_CHN,
				SKW_EDMA_RX_CHN_NODE_NUM,
				RX_PKT_ADDR_BUF_CNT * sizeof(long long),
				SKW_FW_TO_HOST,
				skw_edma_txrx_isr,
				1, SKW_EDMA_CHN_PRIORITY_0,
				SKW_EDMA_CHN_BUFF_NON_LINNER,
				SKW_EDMA_CHN_RING_BUFF,
				SKW_EDMA_CHN_LINKLIST_MODE, true);


	// data tx/rx channel
	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		lmac = &skw->hw.lmac[i];

		sprintf(name, "mac%d", i);
		skw_debugfs_file(SKW_WIPHY_DENTRY(wiphy), name, 0444, &skw_lmac_fops, lmac);

		init_dummy_netdev(&lmac->dummy_dev);
		skw_compat_netif_napi_add_weight(&lmac->dummy_dev, &lmac->napi_tx, skw_netdev_poll_tx, 64);
		skw_compat_netif_napi_add_weight(&lmac->dummy_dev, &lmac->napi_rx, skw_netdev_poll_rx, 64);

		skb_queue_head_init(&lmac->edma_free_list);
		skb_queue_head_init(&lmac->avail_skb);

		skb_queue_head_init(&lmac->rx_dat_q);
		skw_list_init(&lmac->rx_todo_list);

		skw_edma_reset_refill((void *) skw, i);
		lmac->flags = SKW_LMAC_FLAG_INIT;

		napi_enable(&lmac->napi_tx);
		napi_enable(&lmac->napi_rx);
	}

	return 0;
}

void skw_edma_deinit(struct wiphy *wiphy)
{
	int i = 0;
	struct skw_lmac *lmac = NULL;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (skw->hw.bus != SKW_BUS_PCIE)
		return;

	skw_edma_chn_deinit(skw, &skw->edma.cmd_chn);
	skw_edma_chn_deinit(skw, &skw->edma.short_event_chn);
	skw_edma_chn_deinit(skw, &skw->edma.long_event_chn);

	for (i = 0; i < SKW_MAX_LMAC_SUPPORT; i++) {
		lmac = &skw->hw.lmac[i];
		napi_disable(&lmac->napi_tx);
		napi_disable(&lmac->napi_rx);
		netif_napi_del(&lmac->napi_tx);
		netif_napi_del(&lmac->napi_rx);
		skw_edma_chn_deinit(skw, &skw->edma.tx_chn[i]);
		skw_edma_chn_deinit(skw, &skw->edma.tx_resp_chn[i]);
		skw_edma_chn_deinit(skw, &skw->edma.rx_chn[i]);
		skw_edma_chn_deinit(skw, &skw->edma.rx_req_chn[i]);
		skw_edma_chn_deinit(skw, &skw->edma.filter_chn[i]);
		skb_queue_purge(&lmac->edma_free_list);
		skb_queue_purge(&lmac->avail_skb);
		skb_queue_purge(&lmac->rx_dat_q);
		skw_rx_todo(&lmac->rx_todo_list);
	}

	skw_edma_cache_deinit(skw);
}

void skw_edma_mask_irq(struct skw_core *skw, u8 lmac_id)
{
	if (skw->hw.bus != SKW_BUS_PCIE)
		return;

	skw->hw_pdata->edma_mask_irq(SKW_EDMA_WIFI_TX0_CHN + lmac_id);
}

void skw_edma_unmask_irq(struct skw_core *skw, u8 lmac_id)
{
	if (skw->hw.bus != SKW_BUS_PCIE)
		return;

	skw->hw_pdata->edma_unmask_irq(SKW_EDMA_WIFI_TX0_CHN + lmac_id);
}
