/*****************************************************************
 *Copyright (C) 2021 Seekwave Tech Inc.
 *Filename : skw_sdio_host .h
 *Authors:seekwave platform
 *
 * This software is licensed under the terms of the the GNU
 * General Public License version 2, as published by the Free
 * Software Foundation, and may be copied, distributed, and
 * modified under those terms.
 *
 * This program is distributed in the hope that it will be usefull,
 * but without any warranty;without even the implied warranty of
 * merchantability or fitness for a partcular purpose. See the
 * GUN General Public License for more details.
 * **************************************************************/
#ifndef __SKW_SDIO_HOST_H__
#define __SKW_SDIO_HOST_H__
#include "skw_sdio.h"
//int skw_sdio_card_detect_change(int power_on);
int skw_sdio_mmc_rescan(int sd_id);
int skw_sdio_mmc_scan(int sd_id);
#endif /* __SKW_SDIO_HOST_H__ */