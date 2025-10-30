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

#ifndef __SKW_REGD_H__
#define __SKW_REGD_H__

#include <linux/ctype.h>
#include "skw_core.h"

struct skw_reg_rule {
	u8 start_channel;
	u8 nr_channel;
	s8 max_power;
	s8 max_gain;
	u32 flags;
} __packed;

struct skw_regdom {
	u8 country[3];
	u8 nr_reg_rules;
	struct skw_reg_rule rules[8];
} __packed;

static inline bool skw_regd_self_mamaged(struct wiphy *wiphy)
{
	struct skw_core *skw = wiphy_priv(wiphy);

	return test_bit(SKW_FLAG_PRIV_REGD, &skw->flags);
}

static inline bool is_skw_valid_reg_code(const char *alpha2)
{
	if (!alpha2)
		return false;

	if (alpha2[0] == '0' && alpha2[1] == '0')
		return true;

	return isalpha(alpha2[0]) && isalpha(alpha2[1]);
}

void skw_regd_init(struct wiphy *wiphy);
int skw_set_regdom(struct wiphy *wiphy, char *country);
int skw_set_wiphy_regd(struct wiphy *wiphy, const char *country);
int skw_cmd_set_regdom(struct wiphy *wiphy, const char *alpha2);
#endif
