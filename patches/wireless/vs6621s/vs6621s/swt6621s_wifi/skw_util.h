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

#ifndef __SKW_UTIL_H__
#define __SKW_UTIL_H__

#include <linux/version.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/sched.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <uapi/linux/sched/types.h>
#endif

#define SKW_NULL
#define SKW_LEAVE                    WLAN_REASON_DEAUTH_LEAVING
#define SKW_2K_SIZE                  2048
#define SKW_SKB_RECYCLE_COUNT        4096
#define SKW_BASIC_RATE_COUNT         8
/* hdr(24) + reason(2) */
#define SKW_DEAUTH_FRAME_LEN         26

#define __SKW_STR(x)                 #x
#define SKW_STR(x)                   __SKW_STR(x)

#define SKW_SET(d, v)                ((d) |= (v))
#define SKW_CLEAR(d, v)              ((d) &= ~(v))
#define SKW_TEST(d, v)               ((d) & (v))

#define SKW_ZALLOC(s, f)             kzalloc(s, f)

#define SKW_KFREE(p)                 \
	do {                         \
		kfree(p);            \
		p = NULL;            \
	} while (0)

#define SKW_KMEMDUP(s, l, f)     (((s) != NULL) ? kmemdup(s, l, f) : NULL)
#define SKW_MGMT_SFC(fc)         (le16_to_cpu(fc) & IEEE80211_FCTL_STYPE)
#define SKW_WDEV_TO_IFACE(w)     container_of(w, struct skw_iface, wdev)

#define SKW_OUI(a, b, c)                                       \
	(((a) & 0xff) << 16 | ((b) & 0xff) << 8 | ((c) & 0xff))

#ifndef list_next_entry
#define list_next_entry(pos, member)                           \
	list_entry((pos)->member.next, typeof(*(pos)), member)
#endif

#ifdef SKWIFI_ASSERT
#define SKW_BUG_ON(c)         BUG_ON(c)
#else
#define SKW_BUG_ON(c)         WARN_ON(c)
#endif

#ifndef READ_ONCE
#define READ_ONCE(x)          ACCESS_ONCE(x)
#endif

#ifndef WRITE_ONCE
#define WRITE_ONCE(x, v)      (ACCESS_ONCE(x) = v)
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(__fallthrough__)
#define skw_fallthrough       __attribute__((__fallthrough__))
#else
#define skw_fallthrough       do {} while (0)  /* fallthrough */
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
static inline void u64_stats_init(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG == 32 && defined(CONFIG_SMP)
	seqcount_init(&syncp->seq);
#endif
}
#endif

#ifndef NET_NAME_ENUM
#define NET_NAME_ENUM 1
#endif

#ifndef netdev_alloc_pcpu_stats
#define netdev_alloc_pcpu_stats(type)				\
({								\
	typeof(type) __percpu *pcpu_stats = alloc_percpu(type); \
	if (pcpu_stats)	{					\
		int i;						\
		for_each_possible_cpu(i) {			\
			typeof(type) *stat;			\
			stat = per_cpu_ptr(pcpu_stats, i);	\
			u64_stats_init(&stat->syncp);		\
		}						\
	}							\
	pcpu_stats;						\
})
#endif

#define skw_foreach_element(_elem, _data, _datalen)			     \
	for (_elem = (struct skw_element *)(_data);                          \
	     (const u8 *)(_data) + (_datalen) - (const u8 *)_elem >=	     \
		(int)sizeof(*_elem) &&					     \
	     (const u8 *)(_data) + (_datalen) - (const u8 *)_elem >=	     \
		(int)sizeof(*_elem) + _elem->datalen;			     \
	     _elem = (struct skw_element *)(_elem->data + _elem->datalen))

#define skw_foreach_element_id(element, _id, data, datalen)		     \
	skw_foreach_element(element, data, datalen)			     \
		if (element->id == (_id))

struct skw_tp_rate {
	union {
		struct {
			u16 value;
			u8 two_dec;
			u8 unit;
		} rate;

		u32 ret;
	};
};

struct skw_template {
	u16 head_offset;
	u16 head_len;
	u16 tail_ofsset;
	u16 tail_len;
	struct ieee80211_mgmt mgmt[0];
};

struct skw_rate {
	u8 flags;
	u8 mcs_idx;
	u16 legacy_rate;
	u8 nss;
	u8 bw;
	u8 gi;
	u8 he_ru;
	u8 he_dcm;
} __packed;

struct skw_arphdr {
	__be16          ar_hrd; /* format of hardware address */
	__be16          ar_pro; /* format of protocol address */
	unsigned char   ar_hln; /* length of hardware address */
	unsigned char   ar_pln; /* length of protocol address */
	__be16          ar_op;  /* ARP opcode (command) */

	unsigned char   ar_sha[ETH_ALEN]; /* sender hardware address */
	__be32          ar_sip;  /* sender IP address */
	unsigned char   ar_tha[ETH_ALEN]; /* target hardware address */
	__be32          ar_tip;  /* target IP address */
} __packed;

struct skw_element {
	u8 id;
	u8 datalen;
	u8 data[];
} __packed;

struct skw_tlv {
	u16 type;
	u16 len;
	char value[0];
};

struct skw_tlv_conf {
	void *buff;
	u16 *plen;
	int buff_len, total_len;
};

static inline struct skw_arphdr *skw_arp_hdr(struct sk_buff *skb)
{
	if (!skb)
		return NULL;

	return (struct skw_arphdr *)(skb->data + 14);
}

static inline u64 skw_mac_to_u64(const u8 *addr)
{
	u64 u = 0;
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		u = u << 8 | addr[i];

	return u;
}

static inline void skw_u64_to_mac(u64 u, u8 *addr)
{
	int i;

	for (i = ETH_ALEN - 1; i >= 0; i--) {
		addr[i] = u & 0xff;
		u = u >> 8;
	}
}

static inline void *skw_put_skb_data(struct sk_buff *skb, const void *data,
				 unsigned int len)
{
	void *tmp = skb_put(skb, len);

	memcpy(tmp, data, len);

	return tmp;
}

static inline void *skw_put_skb_zero(struct sk_buff *skb, unsigned int len)
{
	void *tmp = skb_put(skb, len);

	memset(tmp, 0, len);

	return tmp;
}

static inline struct ethhdr *skw_eth_hdr(const struct sk_buff *skb)
{
	return (struct ethhdr *)skb->data;
}

static inline int skw_mac_offset(const struct sk_buff *skb)
{
	return skb_mac_header(skb) - skb->data;
}

static inline void skw_set_thread_priority(struct task_struct *thread,
					   int policy, int priority)
{
#ifdef CONFIG_SWT6621S_HIGH_PRIORITY
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	sched_set_fifo_low(thread);
#else
	struct sched_param param = {
		.sched_priority = priority,
	};

	sched_setscheduler(thread, policy, &param);
#endif
#endif
}

static inline const char *skw_iftype_name(enum nl80211_iftype iftype)
{
	static const char * const ifname[] = {"IFTYPE_NONE",
					"ADHOC",
					"STA",
					"AP",
					"AP_VLAN",
					"WDS",
					"MONITOR",
					"MESH",
					"P2P_GC",
					"P2P_GO",
					"P2P_DEVICE",
					"OCB",
					"NAN",
					"IFTYPE_LAST"};

	return ifname[iftype];
}

/**
 * skw_ether_copy - Copy an Ethernet address
 * @dst: Pointer to a six-byte array Ethernet address destination
 * @src: Pointer to a six-byte array Ethernet address source
 *
 * Please note: dst & src must both be aligned to u16.
 */
static inline void skw_ether_copy(u8 *dst, const u8 *src)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	*(u32 *)dst = *(const u32 *)src;
	*(u16 *)(dst + 4) = *(const u16 *)(src + 4);
#else
	u16 *a = (u16 *)dst;
	const u16 *b = (const u16 *)src;

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
#endif
}

#ifdef SKW_IMPORT_NS
struct file *skw_file_open(const char *path, int flags, int mode);
int skw_file_read(struct file *fp, unsigned char *data,
		size_t size, loff_t offset);
int skw_file_write(struct file *fp, unsigned char *data,
		size_t size, loff_t offset);
int skw_file_sync(struct file *fp);
void skw_file_close(struct file *fp);
#endif

u64 skw_local_clock(void);
int skw_key_idx(unsigned long bitmap);
char *skw_mgmt_name(u16 fc);
int skw_freq_to_chn(int freq);

int skw_tlv_alloc(struct skw_tlv_conf *conf, int len, gfp_t gfp);
void skw_tlv_start(struct skw_tlv_conf *conf);
void skw_tlv_end(struct skw_tlv_conf *conf);
void *skw_tlv_reserve(struct skw_tlv_conf *conf, int len);
int skw_tlv_add(struct skw_tlv_conf *conf, int type, void *dat, int dat_len);
void skw_tlv_free(struct skw_tlv_conf *conf);

static inline int skw_tlv_add_bool(struct skw_tlv_conf *conf, int type, bool enable)
{
	u8 val = enable;

	return skw_tlv_add(conf, type, &val, 1);
}

int skw_set_mib(struct wiphy *wiphy, int inst, int mib, void *dat, int dat_len);
static inline int skw_set_mib_bool(struct wiphy *wiphy, int inst, int mib, bool enable)
{
	u8 data = enable;

	return skw_set_mib(wiphy, inst, mib, &data, 1);
}

static inline int skw_set_mib_u8(struct wiphy *wiphy, int inst, int mib, u8 data)
{
	return skw_set_mib(wiphy, inst, mib, &data, 1);
}

static inline int skw_set_mib_u16(struct wiphy *wiphy, int inst, int mib, u16 data)
{
	return skw_set_mib(wiphy, inst, mib, &data, 2);
}

static inline int skw_set_mib_u32(struct wiphy *wiphy, int inst, int mib, u32 data)
{
	return skw_set_mib(wiphy, inst, mib, &data, 4);
}

#endif
