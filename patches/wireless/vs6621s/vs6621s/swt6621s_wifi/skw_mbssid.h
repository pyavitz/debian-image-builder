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

#ifndef __SKW_MBSSID_H__
#define __SKW_MBSSID_H__

#define SKW_EID_EXT_NON_INHERITANCE   56

void skw_mbssid_data_parser(struct wiphy *wiphy, bool beacon,
		struct ieee80211_channel *chan, s32 signal,
		struct ieee80211_mgmt *mgmt, int mgmt_len);

#endif
