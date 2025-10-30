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

#include <linux/nl80211.h>
#include <net/cfg80211.h>

#include "skw_core.h"
#include "skw_regd.h"
#include "skw_msg.h"
#include "skw_log.h"
#include "skw_db.h"

static int skw_regd_show(struct seq_file *seq, void *data)
{
	struct wiphy *wiphy = seq->private;
	struct skw_core *skw = wiphy_priv(wiphy);

	seq_puts(seq, "\n");

	seq_printf(seq, "country: %c%c\n", skw->country[0], skw->country[1]);

	seq_puts(seq, "\n");

	return 0;
}

static int skw_regd_open(struct inode *inode, struct file *file)
{
	return single_open(file, skw_regd_show, inode->i_private);
}

static ssize_t skw_regd_write(struct file *fp, const char __user *buf,
				size_t size, loff_t *off)
{
	u8 country[3] = {0};
	struct wiphy *wiphy = fp->f_inode->i_private;
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (size != 3) {
		skw_chip_err(skw->idx, "invalid len: %zd\n", size);
		return size;
	}

	if (copy_from_user(&country, buf, size)) {
		skw_chip_err(skw->idx, "copy failed\n");
		return size;
	}

	ret = skw_set_regdom(wiphy, (char *)country);
	if (ret)
		skw_chip_err(skw->idx, "set country failed, ret: %d\n", ret);

	return size;
}

static const struct file_operations skw_regd_fops = {
	.owner = THIS_MODULE,
	.open = skw_regd_open,
	.read = seq_read,
	.write = skw_regd_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static bool skw_alpha2_equal(const char *alpha2_x, const char *alpha2_y)
{
	if (!alpha2_x || !alpha2_y)
		return false;

	return alpha2_x[0] == alpha2_y[0] && alpha2_x[1] == alpha2_y[1];
}

static bool skw_freq_in_rule_band(const struct ieee80211_freq_range *freq_range,
			      u32 freq_khz)
{
#define ONE_GHZ_IN_KHZ	1000000
	u32 limit = freq_khz > 45 * ONE_GHZ_IN_KHZ ?
			20 * ONE_GHZ_IN_KHZ : 2 * ONE_GHZ_IN_KHZ;

	if (abs(freq_khz - freq_range->start_freq_khz) <= limit)
		return true;

	if (abs(freq_khz - freq_range->end_freq_khz) <= limit)
		return true;

	return false;

#undef ONE_GHZ_IN_KHZ
}

static bool skw_does_bw_fit_range(const struct ieee80211_freq_range *freq_range,
				u32 center_freq_khz, u32 bw_khz)
{
	u32 start_freq_khz, end_freq_khz;

	start_freq_khz = center_freq_khz - (bw_khz / 2);
	end_freq_khz = center_freq_khz + (bw_khz / 2);

	if (start_freq_khz >= freq_range->start_freq_khz &&
	    end_freq_khz <= freq_range->end_freq_khz)
		return true;

	return false;
}

static const struct ieee80211_regdomain *skw_get_regd(struct skw_core *skw, const char *alpha2)
{
	int i;
	const struct ieee80211_regdomain *regdom;

	if (!is_skw_valid_reg_code(alpha2)) {
		skw_chip_err(skw->idx, "Invalid alpha\n");
		return NULL;
	}

	for (i = 0; i < skw_regdb_size; i++) {
		regdom = skw_regdb[i];

		if (skw_alpha2_equal(alpha2, regdom->alpha2))
			return regdom;
	}

	skw_chip_warn(skw->idx, "country: %c%c not support\n", alpha2[0], alpha2[1]);

	return NULL;
}

static bool is_skw_valid_reg_rule(struct skw_core *skw, const struct ieee80211_reg_rule *rule)
{
	u32 freq_diff;
	const struct ieee80211_freq_range *freq_range = &rule->freq_range;

	if (freq_range->start_freq_khz <= 0 || freq_range->end_freq_khz <= 0) {
		skw_chip_dbg(skw->idx, "invalid, start: %d, end: %d\n",
			freq_range->start_freq_khz, freq_range->end_freq_khz);

		return false;
	}

	if (freq_range->start_freq_khz > freq_range->end_freq_khz) {
		skw_chip_dbg(skw->idx, "invalid, start: %d > end: %d\n",
			freq_range->start_freq_khz, freq_range->end_freq_khz);
		return false;
	}

	freq_diff = freq_range->end_freq_khz - freq_range->start_freq_khz;

	if (freq_range->end_freq_khz <= freq_range->start_freq_khz ||
	    freq_range->max_bandwidth_khz > freq_diff) {
		skw_chip_dbg(skw->idx, "invalid, start: %d, end: %d, max band: %d, diff: %d\n",
			freq_range->start_freq_khz, freq_range->end_freq_khz,
			freq_range->max_bandwidth_khz, freq_diff);
		return false;
	}

	return true;
}

static bool is_skw_valid_rd(struct skw_core *skw, const struct ieee80211_regdomain *rd)
{
	int i;
	const struct ieee80211_reg_rule *reg_rule = NULL;

	for (i = 0; i < rd->n_reg_rules; i++) {
		reg_rule = &rd->reg_rules[i];

		if (!is_skw_valid_reg_rule(skw, reg_rule))
			return false;
	}

	return true;
}

static const struct ieee80211_reg_rule *
skw_freq_reg_info(const struct ieee80211_regdomain *regd, u32 freq)
{
	int i;
	bool band_rule_found = false;
	bool bw_fits = false;

	if (!regd)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < regd->n_reg_rules; i++) {
		const struct ieee80211_reg_rule *rr;
		const struct ieee80211_freq_range *fr = NULL;

		rr = &regd->reg_rules[i];
		fr = &rr->freq_range;

		if (!band_rule_found)
			band_rule_found = skw_freq_in_rule_band(fr, freq);

		bw_fits = skw_does_bw_fit_range(fr, freq, MHZ_TO_KHZ(20));

		if (band_rule_found && bw_fits)
			return rr;
	}

	if (!band_rule_found)
		return ERR_PTR(-ERANGE);

	return ERR_PTR(-EINVAL);
}

static const struct ieee80211_reg_rule *skw_regd_rule(struct wiphy *wiphy, u32 freq)
{
	u32 freq_khz = MHZ_TO_KHZ(freq);
	struct skw_core *skw = wiphy_priv(wiphy);

	if (skw->regd || skw_regd_self_mamaged(wiphy))
		return skw_freq_reg_info(skw->regd, freq_khz);

	return freq_reg_info(wiphy, freq_khz);
}

int skw_cmd_set_regdom(struct wiphy *wiphy, const char *alpha2)
{
	int ret;
	int i, idx, band;
	struct ieee80211_supported_band *sband;
	struct skw_regdom regd = {};
	struct skw_core *skw = wiphy_priv(wiphy);
	struct skw_reg_rule *rule = &regd.rules[0];
	const struct ieee80211_reg_rule *rr = NULL, *_rr = NULL;

#define SKW_MAX_POWER(rr)  (MBM_TO_DBM(rr->power_rule.max_eirp))
#define SKW_MAX_GAIN(rr)   (MBI_TO_DBI(rr->power_rule.max_antenna_gain))

	regd.country[0] = alpha2[0];
	regd.country[1] = alpha2[1];
	regd.country[2] = 0;

	for (idx = 0, band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		for (i = 0; i < sband->n_channels; i++) {
			struct ieee80211_channel *chn = &sband->channels[i];

			rr = skw_regd_rule(wiphy, chn->center_freq);
			if (IS_ERR(rr) || rr->flags & SKW_RRF_NO_IR)
				continue;

			if (rr != _rr) {
				regd.nr_reg_rules++;

				rule = &regd.rules[idx++];

				rule->nr_channel = 0;
				rule->start_channel = chn->hw_value;
				rule->max_power = SKW_MAX_POWER(rr);
				rule->max_gain = SKW_MAX_GAIN(rr);
				rule->flags = rr->flags;

				_rr = rr;
			}

			rule->nr_channel = chn->hw_value - rule->start_channel + 1;
		}
	}

	if (!regd.nr_reg_rules)
		return 0;

	for (i = 0; i < regd.nr_reg_rules; i++) {
		skw_chip_dbg(skw->idx, "%d @ %d, power: %d, gain: %d, flags: 0x%x\n",
			regd.rules[i].start_channel, regd.rules[i].nr_channel,
			regd.rules[i].max_power, regd.rules[i].max_gain,
			regd.rules[i].flags);
	}

	ret = skw_msg_xmit(wiphy, 0, SKW_CMD_SET_REGD, &regd,
			   sizeof(regd), NULL, 0);
	if (!ret) {
		skw->country[0] = alpha2[0];
		skw->country[1] = alpha2[1];
	} else {
		skw_chip_warn(skw->idx, "failed, country: %c%c, rules: %d, ret: %d\n",
			 alpha2[0], alpha2[1], regd.nr_reg_rules, ret);
	}

	return ret;
}

static int __skw_set_wiphy_regd(struct wiphy *wiphy, struct ieee80211_regdomain *rd)
{
	int ret = 0;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw->regd = rd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	if (rtnl_is_locked())
		ret = skw_set_wiphy_regd_sync(wiphy, rd);
	else
		ret = regulatory_set_wiphy_regd(wiphy, rd);
#else
	wiphy_apply_custom_regulatory(wiphy, rd);
#endif

	return ret;
}

int skw_set_wiphy_regd(struct wiphy *wiphy, const char *country)
{
	const struct ieee80211_regdomain *regd;
	struct skw_core *skw = wiphy_priv(wiphy);

	if (!skw_regd_self_mamaged(wiphy))
		return 0;

	regd = skw_get_regd(skw, country);
	if (!regd)
		return -EINVAL;

	if (!is_skw_valid_rd(skw, regd))
		return -EINVAL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	if (country[0] == '0' && country[1] == '0')
		wiphy->regulatory_flags &= ~REGULATORY_DISABLE_BEACON_HINTS;
	else
		wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
#endif

	return __skw_set_wiphy_regd(wiphy, (void *)regd);
}

int skw_set_regdom(struct wiphy *wiphy, char *country)
{
	int ret;
	struct skw_core *skw = wiphy_priv(wiphy);

	skw_chip_dbg(skw->idx, "country: %c%c\n", country[0], country[1]);

	if (!is_skw_valid_reg_code(country)) {
		skw_chip_err(skw->idx, "Invalid country code: %c%c\n",
			country[0], country[1]);

		return -EINVAL;
	}

	if (skw_regd_self_mamaged(wiphy)) {
		ret = skw_set_wiphy_regd(wiphy, country);
		if (!ret)
			ret = skw_cmd_set_regdom(wiphy, country);

		return ret;
	}

	return regulatory_hint(wiphy, country);
}

void skw_regd_init(struct wiphy *wiphy)
{
	skw_debugfs_file(SKW_WIPHY_DENTRY(wiphy), "regdom", 0666, &skw_regd_fops, wiphy);
}
