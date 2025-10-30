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

// #include <linux/kernel.h>
#include <net/cfg80211.h>

#include "skw_dfs.h"
#include "skw_util.h"
#include "skw_cfg80211.h"

#define SKW_MIN_PPB_THRESH            50
#define SKW_PPB_THRESH_RATE(PPB, RATE) ((PPB * RATE + 100 - RATE) / 100)
#define SKW_PPB_THRESH(PPB) SKW_PPB_THRESH_RATE(PPB, SKW_MIN_PPB_THRESH)
#define SKW_PRF2PRI(PRF) ((1000000 + PRF / 2) / PRF)

/* percentage of pulse width tolerance */
#define SKW_WIDTH_TOLERANCE           5
#define SKW_WIDTH_LOWER(W) ((W * (100 - SKW_WIDTH_TOLERANCE) + 50) / 100)
#define SKW_WIDTH_UPPER(W) ((W * (100 + SKW_WIDTH_TOLERANCE) + 50) / 100)

#define SKW_ETSI_PATTERN(ID, WMIN, WMAX, PMIN, PMAX, PRF, PPB, CHIRP)        \
{                                                                            \
	.rule = {                                                            \
		.type_id = ID,                                               \
		.width_min = SKW_WIDTH_LOWER(WMIN),                          \
		.width_max = SKW_WIDTH_UPPER(WMAX),                          \
		.pri_min = (SKW_PRF2PRI(PMAX) - SKW_PRI_TOLERANCE),          \
		.pri_max = (SKW_PRF2PRI(PMIN) * PRF + SKW_PRI_TOLERANCE),    \
		.nr_pri = PRF,                                               \
		.ppb = PPB * PRF,                                            \
		.ppb_thresh = SKW_PPB_THRESH(PPB),                           \
		.max_pri_tolerance = SKW_PRI_TOLERANCE,                      \
		.chirp = CHIRP                                               \
	}                                                                    \
}

/* radar types as defined by ETSI EN-301-893 v1.5.1 */
static struct skw_radar_cfg skw_radar_etsi_cfgs[] = {
	SKW_ETSI_PATTERN(0,  0,  1,  700,  700, 1, 18, false),
	SKW_ETSI_PATTERN(1,  0,  5,  200, 1000, 1, 10, false),
	SKW_ETSI_PATTERN(2,  0, 15,  200, 1600, 1, 15, false),
	SKW_ETSI_PATTERN(3,  0, 15, 2300, 4000, 1, 25, false),
	SKW_ETSI_PATTERN(4, 20, 30, 2000, 4000, 1, 20, false),
	SKW_ETSI_PATTERN(5,  0,  2,  300,  400, 3, 10, false),
	SKW_ETSI_PATTERN(6,  0,  2,  400, 1200, 3, 15, false),
};

#define SKW_FCC_PATTERN(ID, WMIN, WMAX, PMIN, PMAX, PRF, PPB, CHIRP)         \
{                                                                            \
	.rule = {                                                            \
		.type_id = ID,                                               \
		.width_min = SKW_WIDTH_LOWER(WMIN),                          \
		.width_max = SKW_WIDTH_UPPER(WMAX),                          \
		.pri_min = PMIN - SKW_PRI_TOLERANCE,                         \
		.pri_max = PMAX * PRF + SKW_PRI_TOLERANCE,                   \
		.nr_pri = PRF,                                               \
		.ppb = PPB * PRF,                                            \
		.ppb_thresh = SKW_PPB_THRESH(PPB),                           \
		.max_pri_tolerance = SKW_PRI_TOLERANCE,                      \
		.chirp = CHIRP                                               \
	}                                                                    \
}

static struct skw_radar_cfg skw_radar_fcc_cfgs[] = {
	SKW_FCC_PATTERN(0, 0, 1, 1428, 1428, 1, 18, false),
	SKW_FCC_PATTERN(101, 0, 1, 518, 938, 1, 57, false),
	SKW_FCC_PATTERN(102, 0, 1, 938, 2000, 1, 27, false),
	SKW_FCC_PATTERN(103, 0, 1, 2000, 3066, 1, 18, false),
	SKW_FCC_PATTERN(2, 0, 5, 150, 230, 1, 23, false),
	SKW_FCC_PATTERN(3, 6, 10, 200, 500, 1, 16, false),
	SKW_FCC_PATTERN(4, 11, 20, 200, 500, 1, 12, false),
	SKW_FCC_PATTERN(5, 50, 100, 1000, 2000, 1, 1, true),
	SKW_FCC_PATTERN(6, 0, 1, 333, 333, 1, 9, false),
};

#define SKW_JP_PATTERN(ID, WMIN, WMAX, PMIN, PMAX, PRF, PPB, RATE, CHIRP)    \
{                                                                            \
	.rule = {                                                            \
		.type_id = ID,                                               \
		.width_min = SKW_WIDTH_LOWER(WMIN),                          \
		.width_max = SKW_WIDTH_UPPER(WMAX),                          \
		.pri_min = PMIN - SKW_PRI_TOLERANCE,                         \
		.pri_max = PMAX * PRF + SKW_PRI_TOLERANCE,                   \
		.nr_pri = PRF,                                               \
		.ppb = PPB * PRF,                                            \
		.ppb_thresh = SKW_PPB_THRESH_RATE(PPB, RATE),                \
		.max_pri_tolerance = SKW_PRI_TOLERANCE,                      \
		.chirp = CHIRP,                                              \
	}                                                                    \
}

static struct skw_radar_cfg skw_radar_jp_cfgs[] = {
	SKW_JP_PATTERN(0, 0, 1, 1428, 1428, 1, 18, 29, false),
	SKW_JP_PATTERN(1, 2, 3, 3846, 3846, 1, 18, 29, false),
	SKW_JP_PATTERN(2, 0, 1, 1388, 1388, 1, 18, 50, false),
	SKW_JP_PATTERN(3, 0, 4, 4000, 4000, 1, 18, 50, false),
	SKW_JP_PATTERN(4, 0, 5, 150, 230, 1, 23, 50, false),
	SKW_JP_PATTERN(5, 6, 10, 200, 500, 1, 16, 50, false),
	SKW_JP_PATTERN(6, 11, 20, 200, 500, 1, 12, 50, false),
	SKW_JP_PATTERN(7, 50, 100, 1000, 2000, 1, 3, 50, true),
	SKW_JP_PATTERN(5, 0, 1, 333, 333, 1, 9, 50, false),
};

static const struct skw_radar_info skw_radar_infos[] = {
	[NL80211_DFS_UNSET] = {
		.nr_cfg = 0,
		.cfgs = NULL,
	},
	[NL80211_DFS_FCC] = {
		.nr_cfg = ARRAY_SIZE(skw_radar_fcc_cfgs),
		.cfgs = skw_radar_fcc_cfgs,
	},
	[NL80211_DFS_ETSI] = {
		.nr_cfg = ARRAY_SIZE(skw_radar_etsi_cfgs),
		.cfgs = skw_radar_etsi_cfgs,
	},
	[NL80211_DFS_JP] = {
		.nr_cfg = ARRAY_SIZE(skw_radar_jp_cfgs),
		.cfgs = skw_radar_jp_cfgs,
	},
};

static void pool_put_pseq_elem(struct skw_core *skw, struct skw_pri_sequence *pse)
{
	spin_lock_bh(&skw->dfs.skw_pool_lock);

	list_add(&pse->head, &skw->dfs.skw_pseq_pool);

	spin_unlock_bh(&skw->dfs.skw_pool_lock);
}

static void pool_put_pulse_elem(struct skw_core *skw, struct skw_pulse_elem *pe)
{
	spin_lock_bh(&skw->dfs.skw_pool_lock);

	list_add(&pe->head, &skw->dfs.skw_pulse_pool);

	spin_unlock_bh(&skw->dfs.skw_pool_lock);
}

static struct skw_pulse_elem *pool_get_pulse_elem(struct skw_core *skw)
{
	struct skw_pulse_elem *pe = NULL;

	spin_lock_bh(&skw->dfs.skw_pool_lock);

	if (!list_empty(&skw->dfs.skw_pulse_pool)) {
		pe = list_first_entry(&skw->dfs.skw_pulse_pool, struct skw_pulse_elem, head);
		list_del(&pe->head);
	}

	spin_unlock_bh(&skw->dfs.skw_pool_lock);

	return pe;
}

static struct skw_pulse_elem *pulse_queue_get_tail(struct skw_pri_detector *pde)
{
	struct list_head *l = &pde->pulses;

	if (list_empty(l))
		return NULL;

	return list_entry(l->prev, struct skw_pulse_elem, head);
}

static bool pulse_queue_dequeue(struct skw_core *skw, struct skw_pri_detector *pde)
{
	struct skw_pulse_elem *p = pulse_queue_get_tail(pde);

	if (p != NULL) {
		list_del_init(&p->head);
		pde->count--;
		/* give it back to pool */
		pool_put_pulse_elem(skw, p);
	}

	return (pde->count > 0);
}

/* remove pulses older than window */
static void pulse_queue_check_window(struct skw_core *skw, struct skw_pri_detector *pde)
{
	u64 min_valid_ts;
	struct skw_pulse_elem *p;

	/* there is no delta time with less than 2 pulses */
	if (pde->count < 2)
		return;

	if (pde->last_ts <= pde->window_size)
		return;

	min_valid_ts = pde->last_ts - pde->window_size;

	while ((p = pulse_queue_get_tail(pde)) != NULL) {
		if (p->ts >= min_valid_ts)
			return;

		pulse_queue_dequeue(skw, pde);
	}
}

static u32 pde_get_multiple(u32 val, u32 fraction, u32 tolerance)
{
	u32 remainder;
	u32 factor;
	u32 delta;

	if (fraction == 0)
		return 0;

	delta = (val < fraction) ? (fraction - val) : (val - fraction);

	if (delta <= tolerance)
		/* val and fraction are within tolerance */
		return 1;

	factor = val / fraction;
	remainder = val % fraction;
	if (remainder > tolerance) {
		/* no exact match */
		if ((fraction - remainder) <= tolerance)
			/* remainder is within tolerance */
			factor++;
		else
			factor = 0;
	}

	return factor;
}

static u32 pseq_handler_add_to_existing_seqs(struct skw_core *skw, struct skw_radar_cfg *cfg, u64 ts)
{
	u32 max_count = 0;
	struct skw_pri_sequence *ps, *ps2;

	list_for_each_entry_safe(ps, ps2, &cfg->pri.sequences, head) {
		u32 delta_ts;
		u32 factor;

		/* first ensure that sequence is within window */
		if (ts > ps->deadline_ts) {
			list_del_init(&ps->head);
			pool_put_pseq_elem(skw, ps);
			continue;
		}

		delta_ts = ts - ps->last_ts;
		factor = pde_get_multiple(delta_ts, ps->pri,
				cfg->rule.max_pri_tolerance);
		if (factor > 0) {
			ps->last_ts = ts;
			ps->count++;

			if (max_count < ps->count)
				max_count = ps->count;
		} else {
			ps->count_falses++;
		}
	}

	return max_count;
}

static struct skw_pri_sequence *pool_get_pseq_elem(struct skw_core *skw)
{
	struct skw_pri_sequence *pse = NULL;

	spin_lock_bh(&skw->dfs.skw_pool_lock);

	if (!list_empty(&skw->dfs.skw_pseq_pool)) {
		pse = list_first_entry(&skw->dfs.skw_pseq_pool, struct skw_pri_sequence, head);
		list_del(&pse->head);
	}

	spin_unlock_bh(&skw->dfs.skw_pool_lock);

	return pse;
}

#define GET_PRI_TO_USE(MIN, MAX, RUNTIME) \
	(MIN + SKW_PRI_TOLERANCE == MAX - SKW_PRI_TOLERANCE ? \
	 MIN + SKW_PRI_TOLERANCE : RUNTIME)
static bool pseq_handler_create_sequences(struct skw_core *skw,
		struct skw_radar_cfg *cfg, u64 ts, u32 min_count)
{
	struct skw_pulse_elem *p;

	list_for_each_entry(p, &cfg->pri.pulses, head) {
		struct skw_pri_sequence ps, *new_ps;
		struct skw_pulse_elem *p2;
		u32 tmp_false_count;
		u64 min_valid_ts;
		u32 delta_ts = ts - p->ts;

		if (delta_ts < cfg->rule.pri_min)
			/* ignore too small pri */
			continue;

		if (delta_ts > cfg->rule.pri_max)
			/* stop on too large pri (sorted list) */
			break;

		/* build a new sequence with new potential pri */
		ps.count = 2;
		ps.count_falses = 0;
		ps.first_ts = p->ts;
		ps.last_ts = ts;
		ps.pri = GET_PRI_TO_USE(cfg->rule.pri_min, cfg->rule.pri_max, ts - p->ts);
		ps.dur = ps.pri * (cfg->rule.ppb - 1) + 2 * cfg->rule.max_pri_tolerance;

		p2 = p;
		tmp_false_count = 0;
		min_valid_ts = ts - ps.dur;
		/* check which past pulses are candidates for new sequence */
		list_for_each_entry_continue(p2, &cfg->pri.pulses, head) {
			u32 factor;

			if (p2->ts < min_valid_ts)
				/* stop on crossing window border */
				break;
			/* check if pulse match (multi)PRI */
			factor = pde_get_multiple(ps.last_ts - p2->ts, ps.pri,
					cfg->rule.max_pri_tolerance);
			if (factor > 0) {
				ps.count++;
				ps.first_ts = p2->ts;
				/*
				 * on match, add the intermediate falses
				 * and reset counter
				 */
				ps.count_falses += tmp_false_count;
				tmp_false_count = 0;
			} else {
				/* this is a potential false one */
				tmp_false_count++;
			}
		}

		if (ps.count <= min_count)
			/* did not reach minimum count, drop sequence */
			continue;

		/* this is a valid one, add it */
		ps.deadline_ts = ps.first_ts + ps.dur;
		new_ps = pool_get_pseq_elem(skw);
		if (new_ps == NULL) {
			new_ps = kmalloc(sizeof(*new_ps), GFP_ATOMIC);
			if (new_ps == NULL)
				return false;
		}

		memcpy(new_ps, &ps, sizeof(ps));
		INIT_LIST_HEAD(&new_ps->head);
		list_add(&new_ps->head, &cfg->pri.sequences);
	}

	return true;
}

static void pri_detector_reset(struct skw_core *skw, struct skw_pri_detector *pde, u64 ts)
{
	struct skw_pri_sequence *ps, *ps0;
	struct skw_pulse_elem *p, *p0;

	list_for_each_entry_safe(ps, ps0, &pde->sequences, head) {
		list_del_init(&ps->head);
		pool_put_pseq_elem(skw, ps);
	}

	list_for_each_entry_safe(p, p0, &pde->pulses, head) {
		list_del_init(&p->head);
		pool_put_pulse_elem(skw, p);
	}

	pde->count = 0;
	pde->last_ts = ts;
}

static struct skw_pri_sequence *pseq_handler_check_detection(struct skw_radar_cfg *cfg)
{
	struct skw_pri_sequence *ps;

	if (list_empty(&cfg->pri.sequences))
		return NULL;

	list_for_each_entry(ps, &cfg->pri.sequences, head) {
		/*
		 * we assume to have enough matching confidence if we
		 * 1) have enough pulses
		 * 2) have more matching than false pulses
		 */
		if ((ps->count >= cfg->rule.ppb_thresh) &&
		    (ps->count * cfg->rule.nr_pri >= ps->count_falses))
			return ps;
	}

	return NULL;
}

static bool pulse_queue_enqueue(struct skw_core *skw, struct skw_pri_detector *pde, u64 ts)
{
	struct skw_pulse_elem *p = pool_get_pulse_elem(skw);

	if (p == NULL) {
		p = kmalloc(sizeof(*p), GFP_ATOMIC);
		if (p == NULL)
			return false;
	}

	INIT_LIST_HEAD(&p->head);

	p->ts = ts;
	list_add(&p->head, &pde->pulses);

	pde->count++;
	pde->last_ts = ts;
	pulse_queue_check_window(skw, pde);

	if (pde->count >= pde->max_count)
		pulse_queue_dequeue(skw, pde);

	return true;
}

int skw_dfs_add_pulse(struct wiphy *wiphy, struct net_device *dev, struct skw_pulse_info *pulse)
{
	int i;
	bool reset = false;
	u32 max_updated_seq;
	struct skw_pri_sequence *ps;
	struct skw_iface *iface = netdev_priv(dev);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!iface->sap.dfs.flags)
		return 0;

	if (skw->dfs.last_pulse_ts > pulse->ts)
		reset = true;

	skw->dfs.last_pulse_ts = pulse->ts;

	for (i = 0; i < skw->dfs.info->nr_cfg; i++) {
		struct skw_radar_cfg *cfg = &skw->dfs.info->cfgs[i];
		const struct skw_radar_rule *rule = &cfg->rule;

		if (reset) {
			pri_detector_reset(skw, &cfg->pri, skw->dfs.last_pulse_ts);
			continue;
		}

		if ((rule->width_min > pulse->width) || (rule->width_max < pulse->width)) {
			skw_chip_detail(skw->idx, "invalid pulse width, (%d - %d), pulse width: %d\n",
				 rule->width_min, rule->width_max, pulse->width);
			continue;
		}

		if (rule->chirp && rule->chirp != pulse->chirp) {
			skw_chip_detail(skw->idx, "invalid chirp\n");
			continue;
		}

		if ((pulse->ts - cfg->pri.last_ts) < rule->max_pri_tolerance) {
			skw_chip_detail(skw->idx, "invalid timestap, %lld - %lld = %lld, max: %d\n",
				 pulse->ts, cfg->pri.last_ts, pulse->ts - cfg->pri.last_ts,
				 rule->max_pri_tolerance);
			continue;
		}

		max_updated_seq = pseq_handler_add_to_existing_seqs(skw, cfg, pulse->ts);
		if (!pseq_handler_create_sequences(skw, cfg, pulse->ts, max_updated_seq)) {
			pri_detector_reset(skw, &cfg->pri, pulse->ts);
			continue;
		}

		ps = pseq_handler_check_detection(cfg);
		if (ps) {
			skw_chip_info(skw->idx, "radar deteced, iface dfs flags: 0x%lx\n", iface->sap.dfs.flags);

			skw_dfs_deinit(wiphy, dev);

			cfg80211_radar_event(wiphy, &skw->dfs.chan, GFP_KERNEL);

			break;
		}

		pulse_queue_enqueue(skw, &cfg->pri, pulse->ts);
	}

	return 0;
}

static void skw_dfs_cac_work(struct work_struct *work)
{
	struct delayed_work *dwk = to_delayed_work(work);
	struct skw_iface *iface = container_of(dwk, struct skw_iface,
						sap.dfs.cac_work);

	skw_chip_dbg(iface->skw->idx, "dev: %s finished\n", netdev_name(iface->ndev));

	skw_wdev_lock(&iface->wdev);

	if (iface->wdev.cac_started) {
		skw_dfs_stop_cac(priv_to_wiphy(iface->skw), iface->ndev);

		cfg80211_cac_event(iface->ndev, &iface->skw->dfs.chan,
				NL80211_RADAR_CAC_FINISHED, GFP_KERNEL);
	}

	skw_wdev_unlock(&iface->wdev);
}

int skw_dfs_chan_init(struct wiphy *wiphy, struct net_device *dev,
		      struct cfg80211_chan_def *chandef, u32 cac_time_ms)
{
	int i;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "chan: %d, dfs region: %d, flags: 0x%lx\n",
		chandef->chan->hw_value, skw->dfs.region,
		skw->dfs.flags);

	if (!skw->dfs.fw_enabled)
		return -ENOTSUPP;

	if (skw->dfs.flags) {
		if (skw->dfs.chan.width != chandef->width ||
		    skw->dfs.chan.chan != chandef->chan) {
			skw_chip_warn(skw->idx, "current chan: %d, require chan: %d\n",
				 skw->dfs.chan.chan->hw_value,
				 chandef->chan->hw_value);

			return -EBUSY;
		}
	}

	if (skw->dfs.region >= ARRAY_SIZE(skw_radar_infos)) {
		skw_chip_err(skw->idx, "invalid dfs region: %d\n", skw->dfs.region);

		return -EINVAL;
	}

	skw->dfs.info = &skw_radar_infos[skw->dfs.region];
	if (!skw->dfs.info->nr_cfg || !skw->dfs.info->cfgs) {
		skw_chip_err(skw->idx, "invalid, region: %d, nr_cfg: %d\n",
			skw->dfs.region, skw->dfs.info->nr_cfg);

		return -EINVAL;
	}

	iface->sap.dfs.cac_time_ms = cac_time_ms;
	skw->dfs.last_pulse_ts = 0;
	skw->dfs.chan = *chandef;

	for (i = 0; i < skw->dfs.info->nr_cfg; i++) {
		struct skw_radar_cfg *cfg = &skw->dfs.info->cfgs[i];
		const struct skw_radar_rule *rule = &cfg->rule;

		INIT_LIST_HEAD(&cfg->pri.sequences);
		INIT_LIST_HEAD(&cfg->pri.pulses);

		cfg->pri.window_size = rule->pri_max * rule->ppb * rule->nr_pri;
		cfg->pri.max_count = rule->ppb * 2;
	}

	return 0;
}

int skw_dfs_start_cac(struct wiphy *wiphy, struct net_device *dev)
{
	int ret;
	struct skw_dfs_cac cac;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "%s\n", netdev_name(dev));

	if (!skw->dfs.fw_enabled)
		return -ENOTSUPP;

	cac.type = SKW_DFS_START_CAC;
	cac.len = sizeof(struct skw_cac_params);

	cac.params.chn = skw->dfs.chan.chan->hw_value;
	cac.params.center_chn1 = skw_freq_to_chn(skw->dfs.chan.center_freq1);
	cac.params.center_chn2 = skw_freq_to_chn(skw->dfs.chan.center_freq2);
	cac.params.band_width = to_skw_bw(skw->dfs.chan.width);
	cac.params.region = skw->dfs.region;
	cac.params.time_ms = iface->sap.dfs.cac_time_ms;

	ret = skw_send_msg(wiphy, dev, SKW_CMD_DFS, &cac, sizeof(cac), NULL, 0);
	if (!ret) {
		set_bit(SKW_DFS_FLAG_CAC_MODE, &iface->sap.dfs.flags);
		set_bit(SKW_DFS_FLAG_CAC_MODE, &skw->dfs.flags);
	}

	return ret;
}

int skw_dfs_stop_cac(struct wiphy *wiphy, struct net_device *dev)
{
	struct skw_dfs_cac cac;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "%s\n", netdev_name(dev));

	if (!skw->dfs.fw_enabled)
		return -ENOTSUPP;

	cac.type = SKW_DFS_STOP_CAC;
	cac.len = 0;

	clear_bit(SKW_DFS_FLAG_CAC_MODE, &iface->sap.dfs.flags);
	clear_bit(SKW_DFS_FLAG_CAC_MODE, &skw->dfs.flags);

	return skw_send_msg(wiphy, dev, SKW_CMD_DFS, &cac, sizeof(cac), NULL, 0);
}

int skw_dfs_start_monitor(struct wiphy *wiphy, struct net_device *dev)
{
	int ret;
	struct skw_dfs_cac cac;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "%s\n", netdev_name(dev));

	if (!skw->dfs.fw_enabled)
		return -ENOTSUPP;

	cac.type = SKW_DFS_START_MONITOR;
	cac.len = 0;

	ret = skw_send_msg(wiphy, dev, SKW_CMD_DFS, &cac, sizeof(cac), NULL, 0);
	if (!ret) {
		set_bit(SKW_DFS_FLAG_MONITOR_MODE, &skw->dfs.flags);
		set_bit(SKW_DFS_FLAG_MONITOR_MODE, &iface->sap.dfs.flags);
	}

	return ret;
}

int skw_dfs_stop_monitor(struct wiphy *wiphy, struct net_device *dev)
{
	struct skw_dfs_cac cac;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	skw_chip_dbg(skw->idx, "%s\n", netdev_name(dev));

	if (!skw->dfs.fw_enabled)
		return -ENOTSUPP;

	cac.type = SKW_DFS_STOP_MONITOR;
	cac.len = 0;

	clear_bit(SKW_DFS_FLAG_MONITOR_MODE, &skw->dfs.flags);
	clear_bit(SKW_DFS_FLAG_MONITOR_MODE, &iface->sap.dfs.flags);

	return skw_send_msg(wiphy, dev, SKW_CMD_DFS, &cac, sizeof(cac), NULL, 0);
}

int skw_dfs_init(struct wiphy *wiphy, struct net_device *dev)
{
	int i, j;
	struct skw_iface *iface = netdev_priv(dev);

	BUILD_BUG_ON(IS_ENABLED(CONFIG_SWT6621S_REGD_SELF_MANAGED));

	iface->sap.dfs.flags = 0;
	INIT_DELAYED_WORK(&iface->sap.dfs.cac_work, skw_dfs_cac_work);

	for (i = 0; i < ARRAY_SIZE(skw_radar_infos); i++) {
		const struct skw_radar_info *info = &skw_radar_infos[i];

		for (j = 0; j < info->nr_cfg; j++) {
			INIT_LIST_HEAD(&info->cfgs[j].pri.sequences);
			INIT_LIST_HEAD(&info->cfgs[j].pri.pulses);
		}
	}

	return 0;
}

int skw_dfs_deinit(struct wiphy *wiphy, struct net_device *dev)
{
	int i, j;
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_iface *iface = netdev_priv(dev);

	if (test_bit(SKW_DFS_FLAG_CAC_MODE, &iface->sap.dfs.flags)) {

		cancel_delayed_work_sync(&iface->sap.dfs.cac_work);

		skw_dfs_stop_cac(wiphy, dev);

		cfg80211_cac_event(dev, &skw->dfs.chan,
				NL80211_RADAR_CAC_ABORTED, GFP_KERNEL);
	}

	if (test_bit(SKW_DFS_FLAG_MONITOR_MODE, &iface->sap.dfs.flags))
		skw_dfs_stop_monitor(wiphy, dev);

	for (i = 0; i < ARRAY_SIZE(skw_radar_infos); i++) {
		const struct skw_radar_info *info = &skw_radar_infos[i];

		for (j = 0; j < info->nr_cfg; j++)
			pri_detector_reset(skw, &info->cfgs[j].pri, 0);
	}

	return 0;
}
