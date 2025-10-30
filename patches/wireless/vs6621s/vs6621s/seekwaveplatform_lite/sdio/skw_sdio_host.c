/*
 * Copyright (C) 2021 Seekwave Tech Inc.
 *
 * Filename : skw_sdio_host.c
 * Abstract : This file is a implementation for Seekwave sdio  function
 *
 * Authors	:skw BSP team
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/version.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/printk.h>
#include "skw_sdio.h"
#include "skw_sdio_log.h"
#include "skw_sdio_host.h"
#include "../skwutil/boot_config.h"
//#define CONFIG_SKW_HOST_PLATFORM_FULLHAN

extern int sdio_reset_comm(struct mmc_card *card);
int skw_sdio_mmc_scan(int sd_id)
{
	int ret = 0;
	pr_info("%s:[%d]\n", __func__, sd_id);
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	//如果定义了CONFIG_SKW_HOST_PLATFORM_AMLOGIC，则调用extern_wifi_set_enable(1)函数
	extern_wifi_set_enable(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_FULLHAN)
	pr_info("%s:[mmc%d:card init]\n", __func__, sd_id);
	fh_sdio_card_scan(sd_id); //fullhan sdio card scan
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	sunxi_wlan_set_power(1);
	msleep(100);
	sunxi_mmc_rescan_card(1); //allwiner sdio card rescan
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	rockchip_wifi_power(1);
	msleep(150);
	rockchip_wifi_set_carddetect(1);
#else
	pr_info("%s: no need skw self scan!!\n", __func__);
#endif
	pr_info("%s:[-]\n", __func__);
	return ret;
}

int skw_sdio_mmc_rescan(int sd_id)
{
	int ret = 0;
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	//如果定义了CONFIG_SKW_HOST_PLATFORM_AMLOGIC，则调用extern_wifi_set_enable(1)函数
	//extern_wifi_set_enable(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_FULLHAN)
	skw_chip_power_reset();
	fh_sdio_card_scan(sd_id); //fullhan sdio card scan
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	skw_chip_power_reset();
	msleep(100);
	sunxi_mmc_rescan_card(1); //allwiner sdio card rescan
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	rockchip_wifi_set_carddetect(0);
	msleep(150);
	skw_chip_power_reset();
	msleep(150);
	rockchip_wifi_set_carddetect(1);
#else
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("[+]");
	if (skw_sdio) {
#if (KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE &&                         \
     LINUX_VERSION_CODE <= KERNEL_VERSION(5, 18, 19))
		if (skw_sdio->sdio_dev_host) {
			sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
			skw_chip_power_reset();
			msleep(100);
			ret = mmc_sw_reset(skw_sdio->sdio_dev_host);
			sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		} else {
			return -EINVAL;
		}
#elif (KERNEL_VERSION(5, 19, 0) <= LINUX_VERSION_CODE)
		if (skw_sdio->sdio_dev_host && skw_sdio->sdio_dev_host->card) {
			sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
			skw_chip_power_reset();
			msleep(100);
			ret = mmc_sw_reset(skw_sdio->sdio_dev_host->card);
			sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		} else {
			return -EINVAL;
		}
#else
		if (skw_sdio->sdio_dev_host && skw_sdio->sdio_dev_host->card) {
			sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
			skw_chip_power_reset();
			msleep(100);
			ret = mmc_hw_reset((skw_sdio->sdio_dev_host));
			//ret = sdio_reset_comm((skw_sdio->sdio_dev_host->card));
			sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		} else {
			return -EINVAL;
		}
#endif
	} else {
		skw_sdio_warn("sdio_dev_host is null\n");
	}
#endif
	skw_sdio_info("[-]");
	return ret;
}