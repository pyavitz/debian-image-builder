/*
 * Copyright (C) 2021 Seekwave Tech Inc.
 *
 * Filename : skw_sdio.c
 * Abstract : This file is a implementation for Seekwave sdio  function
 *
 * Authors	:
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include "skw_sdio_log.h"
#include "skw_sdio_host.h"
#include "skw_sdio_debugfs.h"
#include "skw_sdio.h"
int bind_device=0;

extern unsigned int test_debug;
extern int skw_use_sdma;
extern int g_chipen_pin;
extern struct sv6160_platform_data ucom_pdata;
extern struct sv6160_platform_data wifi_pdata;
extern struct sdio_port sdio_ports[];

static int card_id = SKW_MMC_HOST_SD_INDEX;
module_param(card_id, int, S_IRUGO);
module_param(bind_device, int, S_IRUGO);
module_param(test_debug, uint, S_IRUGO);
module_param(skw_use_sdma, int, S_IRUGO);

#ifndef MMC_CAP2_SDIO_IRQ_NOTHREAD
#define MMC_CAP2_SDIO_IRQ_NOTHREAD (1 << 17)
#endif

#define skw_sdio_transfer_enter() mutex_lock(&skw_sdio->transfer_mutex)
#define skw_sdio_transfer_exit() mutex_unlock(&skw_sdio->transfer_mutex)

irqreturn_t skw_gpio_irq_handler(int irq, void *dev_id); //interrupt
//int (*skw_dloader)(unsigned int subsys);
//static int skw_get_chipid(char *chip_id);
static int check_chipid(void);
static int skw_sdio_cp_reset(void);
static int skw_sdio_cp_service_ops(int service_ops);
static int skw_sdio_cpdebug_boot(void);
struct skw_sdio_data_t *g_skw_sdio_data;
static struct sdio_driver skw_sdio_driver;
static struct mutex dloader_mutex;
static int skw_sdio_set_dma_type(unsigned int address, unsigned int dma_type);
static int skw_sdio_slp_feature_en(unsigned int address, unsigned int slp_en);
static int skw_sdio_host_irq_init(unsigned int irq_gpio_num);
static int skw_WIFI_service_start(void);
static int skw_WIFI_service_stop(void);
static int skw_BT_service_start(void);
static int skw_BT_service_stop(void);
static int skw_sdio_host_check(struct skw_sdio_data_t *skw_sdio);
extern int sdio_reset_comm(struct mmc_card *card);
extern void kernel_restart(char *cmd);
extern void skw_sdio_exception_work(struct work_struct *work);
static int skw_sdio_reg_reset_cp(void);
extern int  cp_detect_sleep_mode;
extern char skw_cp_ver;
extern int max_ch_num;
extern int max_pac_size;
extern int skw_sdio_blk_size;
extern char assert_context[];
extern int  assert_context_size;
extern struct debug_vars debug_infos;
//=======================================================
//debug sdio macro and Variable
//=======================================================

struct skw_sdio_data_t *skw_sdio_get_data(void)
{
	return g_skw_sdio_data;
}

void skw_sdio_unlock_rx_ws(struct skw_sdio_data_t *skw_sdio)
{

	if (!atomic_read(&skw_sdio->rx_wakelocked))
		return;
	atomic_set(&skw_sdio->rx_wakelocked, 0);
#ifdef CONFIG_WAKELOCK
	__pm_relax(&skw_sdio->rx_wl.ws);
#else
	__pm_relax(skw_sdio->rx_ws);
#endif
}
static void skw_sdio_lock_rx_ws(struct skw_sdio_data_t *skw_sdio)
{
//	if (atomic_read(&skw_sdio->rx_wakelocked))
		return;
	atomic_set(&skw_sdio->rx_wakelocked, 1);
#ifdef CONFIG_WAKELOCK
	__pm_stay_awake(&skw_sdio->rx_wl.ws);
#else
	__pm_stay_awake(skw_sdio->rx_ws);
#endif
}
static void skw_sdio_wakeup_source_init(struct skw_sdio_data_t *skw_sdio)
{
	if(skw_sdio) {
#ifdef CONFIG_WAKELOCK
	wake_lock_init(&skw_sdio->rx_wl, WAKE_LOCK_SUSPEND,"skw_sdio_r_wakelock");
#else
	skw_sdio->rx_ws = skw_wakeup_source_register(NULL, "skw_sdio_r_wakelock");
#endif
	}
}
static void skw_sdio_wakeup_source_destroy(struct skw_sdio_data_t *skw_sdio)
{
	if(skw_sdio) {
#ifdef CONFIG_WAKELOCK
	wake_lock_destroy(&skw_sdio->rx_wl);
#else
	wakeup_source_unregister(skw_sdio->rx_ws);
#endif
	}
}

void skw_resume_check(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	unsigned int timeout;

	timeout = 0;
	while((!atomic_read(&skw_sdio->resume_flag)) && (timeout++ < 20000))
		usleep_range(1500, 2000);
}

static void skw_sdio_abort(int err_code)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func0 = skw_sdio->sdio_func[FUNC_0];
	unsigned char value;
	int ret;

	if (err_code == -ETIMEDOUT)
		return;
	if(err_code != 0) {
		if(skw_sdio->cp_state){
			skw_sdio_warn("cp_state:%d on recoving!!\n", skw_sdio->cp_state);
			return;
		}
		//send_modem_assert_command();
		skw_sdio->cp_state = 1;
		schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(2000));
	}
	sdio_claim_host(func0);

	value = sdio_readb(func0, SDIO_VER_CCCR, &ret);

	sdio_writeb(func0, SDIO_ABORT_TRANS, SKW_SDIO_CCCR_ABORT, &ret);

	value = sdio_readb(func0, SDIO_VER_CCCR, &ret);
	skw_sdio_err("SDIO Abort, SDIO_VER_CCCR:0x%x\n", value);

	sdio_release_host(func0);
}

int skw_sdio_sdma_write(unsigned char *src, unsigned int len)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	int blksize = func->cur_blksize;
	int ret = 0;

	if (!src || len%4) {
		skw_sdio_err("%s invalid para %p, %d\n", __func__, src, len);
		return -1;
	}

	len = (len + blksize -1)/blksize*blksize;

	skw_resume_check();
	skw_sdio_transfer_enter();
	sdio_claim_host(func);
	ret = sdio_writesb(func, SKW_SDIO_PK_MODE_ADDR, src, len);
	if (ret < 0)
		skw_sdio_err("%s  ret = %d\n", __func__, ret);
	sdio_release_host(func);
	if (ret) 
		skw_sdio_abort(ret);
	skw_sdio_transfer_exit();

	return ret;
}

int skw_sdio_sdma_read(unsigned char *src, unsigned int len)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	int ret = 0;
	skw_resume_check();
	skw_sdio_transfer_enter();
	sdio_claim_host(func);
	ret = sdio_readsb(func, src, SKW_SDIO_PK_MODE_ADDR, len);
	sdio_release_host(func);
	if (ret != 0)
		skw_sdio_abort(ret);
	skw_sdio_transfer_exit();
	return ret;
}

void *skw_get_bus_dev(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int time_count = 0;
	if ((!skw_sdio) || (!skw_sdio->sdio_dev_host)) {
		skw_sdio_err("%d try again get sdio bus dev  \n", __LINE__);
		do {
			skw_sdio = skw_sdio_get_data();
			if (skw_sdio && skw_sdio->sdio_dev_host) {
				break;
			}
			msleep(10);
			time_count++;
		} while (time_count < 50);
	}
	if ((!skw_sdio) || (!skw_sdio->sdio_dev_host)) {
		skw_sdio_err("skw_sdio or dev_host is NULL!\n");
		return NULL;
	}
	return &skw_sdio->sdio_func[FUNC_1]->dev;
}
irqreturn_t skw_host_wake_irq_handler(int irq, void *dev_id) //interrupt
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int	value = gpio_get_value(skw_sdio->gpio_in);

	skw_sdio_dbg("gpio request_irq=%d  GPIO value %d!\n", irq, value);
	return IRQ_HANDLED;
}

static int skw_sdio_host_wake_irq_init(unsigned int gpio_num)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;

	skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);
	skw_sdio->irq_num = gpio_to_irq(skw_sdio->gpio_in);
	skw_sdio->irq_trigger_type = IRQF_TRIGGER_RISING;
	skw_sdio_info("gpio_irq %d\n", skw_sdio->irq_num);
	if (skw_sdio->irq_num) {
		ret = request_irq(skw_sdio->irq_num, skw_host_wake_irq_handler,
				skw_sdio->irq_trigger_type | IRQF_ONESHOT, "skw-gpio-irq", NULL);
		if (ret != 0) {
			free_irq(skw_sdio->irq_num, NULL);
			skw_sdio_err("%s request gpio irq fail ret=%d\n", __func__, ret);
			return -1;
		} else {
			skw_sdio_dbg("gpio request_irq=%d  GPIO value %d!\n",
					skw_sdio->irq_num, skw_sdio->device_active);
		}
	}
	enable_irq_wake(skw_sdio->irq_num);
	return ret;
}

int skw_sdio_gpio_irq_pre_ops(void)
{
	int ret = 0;
	struct sdio_func *func;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if (!skw_sdio->boot_data)
		return -1;

	skw_sdio->gpio_in = skw_sdio->boot_data->gpio_in;
	skw_sdio->gpio_out = skw_sdio->boot_data->gpio_out;
	if (skw_sdio->boot_data->gpio_in < 0 ||
	    skw_sdio->boot_data->gpio_out < 0) {
		if (skw_sdio->boot_data->gpio_in >= 0)
			ret = skw_sdio_host_wake_irq_init(
				skw_sdio->boot_data->gpio_in);
		skw_sdio_warn(" no support gpio irq!!!\n");
		return ret;
	}
	switch (cp_detect_sleep_mode) {
	case 0:
		break;
	case 1:
	case 2:
		func = skw_sdio->sdio_func[FUNC_1];
		sdio_claim_host(func);
		sdio_release_irq(func);
		sdio_release_host(func);
		ret = skw_sdio_host_irq_init(skw_sdio->gpio_in);
		break;
	case 3:
		gpio_set_value(skw_sdio->gpio_out, 0);
		msleep(100);
		loopcheck_send_data("APGPIORDY", 9);
		msleep(5);
		gpio_set_value(skw_sdio->gpio_out, 1);
		func = skw_sdio->sdio_func[FUNC_1];
		sdio_claim_host(func);
		sdio_release_irq(func);
		sdio_release_host(func);
		ret = skw_sdio_host_irq_init(skw_sdio->gpio_in);
		break;
	default:
		break;
	}
	if (ret)
		skw_sdio_err("gpio irq init fail\n");
	return ret;
}
static int skw_sdio_start_transfer(struct scatterlist *sgs, int sg_count,
	int total, struct sdio_func *sdio_func, uint fix_inc, bool dir, uint addr)
{
	struct mmc_request mmc_req;
	struct mmc_command mmc_cmd;
	struct mmc_data mmc_dat;
	struct mmc_host *host = sdio_func->card->host;
	bool fifo = (fix_inc == SKW_SDIO_DATA_FIX);
	uint fn_num = sdio_func->num;
	uint blk_num, blk_size, max_blk_count, max_req_size;
	int err_ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();


	blk_size = SKW_SDIO_BLK_SIZE;
	max_blk_count = min_t(unsigned int, host->max_blk_count, (uint)MAX_IO_RW_BLK);
	max_req_size = min_t(unsigned int,	max_blk_count*blk_size, host->max_req_size);

	memset(&mmc_req, 0, sizeof(struct mmc_request));
	memset(&mmc_cmd, 0, sizeof(struct mmc_command));
	memset(&mmc_dat, 0, sizeof(struct mmc_data));

	if (total % blk_size != 0) {
		skw_sdio_err("total %d not aligned to blk size\n", total);
		return -1;
	}

	blk_num = total / blk_size;
	mmc_dat.sg = sgs;
	mmc_dat.sg_len = sg_count;
	mmc_dat.blksz = blk_size;
	mmc_dat.blocks = blk_num;
	mmc_dat.flags = dir ? MMC_DATA_WRITE : MMC_DATA_READ;
	mmc_cmd.opcode = 53; /* SD_IO_RW_EXTENDED */
	mmc_cmd.arg = dir ? 1<<31 : 0;
	mmc_cmd.arg |= (fn_num & 0x7) << 28;
	mmc_cmd.arg |= 1<<27;
	mmc_cmd.arg |= fifo ? 0 : 1<<26;
	mmc_cmd.arg |= (addr & 0x1FFFF) << 9;
	mmc_cmd.arg |= blk_num & 0x1FF;
	mmc_cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;
	mmc_req.cmd = &mmc_cmd;
	mmc_req.data = &mmc_dat;
	if (!fifo)
		addr += total;
	skw_sdio_dbg("total:%d sg_count:%d cmd_arg 0x%x\n", total, sg_count, mmc_cmd.arg);
	sdio_claim_host(sdio_func);
	if(skw_sdio->power_off == 1){
		sdio_release_host(sdio_func);
		return 0;
	}
	mmc_set_data_timeout(&mmc_dat, sdio_func->card);
	mmc_wait_for_req(host, &mmc_req);
	sdio_release_host(sdio_func);

	err_ret = mmc_cmd.error ? mmc_cmd.error : mmc_dat.error;
	if (err_ret != 0) {
		skw_sdio_err(":CMD53 %s failed error=%d\n",
				  dir ? "write" : "read", err_ret);
	}
	return err_ret;
}

int skw_sdio_adma_write(int portno, struct scatterlist *sgs, int sg_count, int total)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;

	skw_resume_check();
	skw_sdio_transfer_enter();
	if(skw_sdio->resume_com==0)
		skw_sdio->resume_com = 1; 
	ret = skw_sdio_start_transfer(sgs, sg_count, SKW_SDIO_ALIGN_BLK(total),
				  skw_sdio->sdio_func[FUNC_1], SKW_SDIO_DATA_FIX,
				  SKW_SDIO_WRITE, SKW_SDIO_PK_MODE_ADDR);
	if (ret) {
		skw_sdio_abort(ret);
	} else {
		if (skw_sdio->device_active==0 && skw_sdio->irq_type && skw_sdio->gpio_in >= 0)
			skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);
	}
	skw_sdio_transfer_exit();

	return ret;
}

int skw_sdio_adma_read(struct skw_sdio_data_t *skw_sdio, struct scatterlist *sgs, int sg_count, int total)
{
	int ret = 0;

	skw_resume_check();
	skw_sdio_transfer_enter();
	ret = skw_sdio_start_transfer(sgs, sg_count, total,
				  skw_sdio->sdio_func[FUNC_1], SKW_SDIO_DATA_FIX,
				  SKW_SDIO_READ, SKW_SDIO_PK_MODE_ADDR);
	if (ret)
		skw_sdio_abort(ret);
	skw_sdio_transfer_exit();
	return ret;
}

static int skw_sdio_dt_set_address(unsigned int address)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_0];
	unsigned char value ;
	int err = 0;
	int i;

	sdio_claim_host(func);
	for (i = 0; i < 4; i++) {
		value = (address >> (8 * i)) & 0xFF;
		sdio_writeb(func, value, SKW_SDIO_FBR_REG+i, &err);
		if (err != 0)
			break;
	}
	sdio_release_host(func);

	return err;
}

static int skw_sdio_dt_get_address(void)
{
	int ret = 0;
	int i = 0;
	unsigned int reg_val = 0;
	u8 reg;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if (skw_sdio->boot_data)
		skw_sdio_info("sys addr :0x%x\n",
			      skw_sdio->boot_data->iram_dl_addr);
	for (i = 0; i < 4; i++) {
		ret = skw_sdio_readb(SKW_SDIO_FBR_REG + i, &reg);
		if (ret)
			return ret;
		//skw_sdio_info("reg = 0x%x\n", reg);
		reg_val |= (reg & 0xFF) << 8 * i;
	}
	skw_sdio_info("feedback sys_addr = 0x%x\n", reg_val);
	return ret;
}

int skw_sdio_writel(unsigned int address, void *data)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	int ret = 0;

	skw_resume_check();
	skw_sdio_transfer_enter();

	ret = skw_sdio_dt_set_address(address);
	if (ret != 0) {
		skw_sdio_transfer_exit();
		return ret;
	}

	sdio_claim_host(func);
	sdio_writel(func, *(unsigned int *)data, SKW_SDIO_DT_MODE_ADDR, &ret);
	sdio_release_host(func);
	skw_sdio_transfer_exit();

	if (ret) {
		skw_sdio_err("%s fail ret:%d, addr=0x%x\n", __func__,
				ret, address);
		skw_sdio_abort(ret);
	}

	return ret;
}

int skw_sdio_readl(unsigned int address, void *data)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	int ret = 0;


	skw_resume_check();
	skw_sdio_transfer_enter();
	ret = skw_sdio_dt_set_address(address);
	if (ret != 0) {
		skw_sdio_transfer_exit();
		return ret;
	}

	sdio_claim_host(func);

	*(unsigned int *)data = sdio_readl(func, SKW_SDIO_DT_MODE_ADDR, &ret);

	sdio_release_host(func);
	skw_sdio_transfer_exit();
	if (ret) {
		skw_sdio_err("%s fail ret:%d, addr=0x%x\n", __func__, ret, address);
		skw_sdio_abort(ret);
	}

	return ret;
}
/*
 *command = 0: service_start else service stop
 *service = 0: WIFI_service else BT service.
 */
int send_modem_service_command(u16 service, u16 command)
{
	u16 cmd;
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if(command)
		skw_sdio->service_state_map&= ~(1<<service);
		//command = 1;
	cmd = (service<<1)|command;
	cmd = 1 << cmd;
	if (cmd>>8) {
		skw_sdio_err("service command error 0x%x!", cmd);
			return -EINVAL;
	}
	skw_sdio_info("service = %d cmd %x\n", service, cmd);
	if(skw_sdio->cp_state)
		return -EINVAL;

	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, cmd & 0xff);
	skw_sdio_info("ret = %d command %x\n", ret, command);
	return ret;
}

static unsigned int max_bytes(struct sdio_func *func)
{
	unsigned int mval = func->card->host->max_blk_size;

	if (func->card->quirks & MMC_QUIRK_BLKSZ_FOR_BYTE_MODE)
		mval = min(mval, func->cur_blksize);
	else
		mval = min(mval, func->max_blksize);

	if (func->card->quirks & MMC_QUIRK_BROKEN_BYTE_MODE_512)
		return min(mval, 511u);

	/* maximum size for byte mode */
	return min(mval, 512u);
}

int skw_sdio_dt_write(unsigned int address,	void *buf, unsigned int len)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	unsigned int remainder = len;
	unsigned int trans_len;
	int ret = 0;
	char *data= skw_sdio->next_size_buf;

	skw_resume_check();
	skw_sdio_transfer_enter();

	ret = skw_sdio_dt_set_address(address);
	if (ret != 0) {
		skw_sdio_err("%s set address error!!!", __func__);
		skw_sdio_transfer_exit();
		return ret;
	}

	if(skw_sdio->resume_com==0)
		skw_sdio->resume_com = 1;
	sdio_claim_host(func);
	while (remainder > 0) {
		if (remainder >= func->cur_blksize)
			trans_len = func->cur_blksize;
		else
			trans_len = min(remainder, max_bytes(func));

		memcpy(data, buf,trans_len);
		ret = sdio_memcpy_toio(func, SKW_SDIO_DT_MODE_ADDR, data, trans_len);
		if (ret) {
			skw_sdio_err("%s sdio_memcpy_toio failed!!!", __func__);
			break;
		}
		remainder -= trans_len;
		buf += trans_len;
	}
	sdio_release_host(func);
	skw_sdio_transfer_exit();
	if (ret) {
		skw_sdio_err("dt write fail ret:%d, address=0x%x\n", ret, address);
		skw_sdio_abort(ret);
	}
	return ret;
}

int skw_sdio_dt_read(unsigned int address, void *buf, unsigned int len)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	unsigned int remainder = len;
	unsigned int trans_len;
	int ret = 0;

	ret = skw_sdio_dt_set_address(address);
	if (ret != 0) {
		skw_sdio_err("set address error ret=%d !!!", ret);
		return ret;
	}
	if(skw_sdio->resume_com==0)
		skw_sdio->resume_com = 1; 
	skw_sdio_transfer_enter();
	sdio_claim_host(func);
	while (remainder > 0) {
		if (remainder >= func->cur_blksize)
			trans_len = func->cur_blksize;
		else
			trans_len = min(remainder, max_bytes(func));
		ret = sdio_memcpy_fromio(func, buf, SKW_SDIO_DT_MODE_ADDR, trans_len);
		if (ret) {
			skw_sdio_err("sdio_memcpy_fromio: %p 0x%x ret=%d\n", buf, *(uint32_t *)buf, ret);
			break;
		}
		remainder -= trans_len;
		buf += trans_len;
	}
	sdio_release_host(func);
	skw_sdio_transfer_exit();
	if (ret) {
		skw_sdio_err("dt read fail ret:%d, address=0x%x\n", ret, address);
		skw_sdio_abort(ret);
	}

	return ret;
}

int skw_sdio_readb(unsigned int address, unsigned char *value)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_0];
	unsigned char reg = 0;
	int err = 0, i;

	for (i=0; i<2; i++) {
		try_to_wakeup_modem(max_ch_num);
		sdio_claim_host(func);
		reg = sdio_readb(func, address, &err);
		if (value)
			*value = reg;
		sdio_release_host(func);
		if (err ==0)
			break;
	}
	return err;
}

int skw_sdio_writeb(unsigned int address, unsigned char value)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_0];
	int err = 0, i;

	for (i=0; i<2; i++) {
		try_to_wakeup_modem(max_ch_num);
		sdio_claim_host(func);
		sdio_writeb(func, value, address, &err);
		sdio_release_host(func);
		if (err == 0)
			break;
	}
	return err;
}

static int skw_sdio_host_irq_init(unsigned int irq_gpio_num)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;

	skw_sdio->irq_type = SKW_SDIO_EXTERNAL_IRQ;
	skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);
	skw_sdio->irq_num = gpio_to_irq(skw_sdio->gpio_in);
	skw_sdio->irq_trigger_type = IRQF_TRIGGER_RISING;
	skw_sdio_info("gpio_In:%d,gpio_out:%d irq %d\n",skw_sdio->gpio_in,
		skw_sdio->gpio_out, skw_sdio->irq_num);
	if (skw_sdio->irq_num) {
		ret = request_irq(skw_sdio->irq_num, skw_gpio_irq_handler,
				skw_sdio->irq_trigger_type | IRQF_ONESHOT, "skw-gpio-irq", NULL);
		if (ret != 0) {
			free_irq(skw_sdio->irq_num, NULL);
			skw_sdio_err(" request gpio irq fail ret=%d\n", ret);
			return -1;
		} else {
			skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);
			skw_sdio_info("gpio request_irq=%d  GPIO value %d!\n",
					skw_sdio->irq_num, skw_sdio->device_active);
		}
	}
	enable_irq_wake(skw_sdio->irq_num);
	skw_sdio_rx_up(skw_sdio);
	return ret;
}

static int skw_sdio_get_dev_func(struct sdio_func *func)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if (func->num >= MAX_FUNC_NUM) {
		skw_sdio_err("func num err!!! func num is %d!!!",
			func->num);
		return -1;
	}
	skw_sdio_dbg("func num is %d.", func->num);

	if (func->num == 1) {
		skw_sdio->sdio_func[FUNC_0] = kmemdup(func, sizeof(*func),
							 GFP_KERNEL);
		if (!skw_sdio->sdio_func[FUNC_0]) {
		    return -ENOMEM;
		}
		skw_sdio->sdio_func[FUNC_0]->num = 0;
		skw_sdio->sdio_func[FUNC_0]->max_blksize = SKW_SDIO_BLK_SIZE;
	}
	skw_sdio->sdio_func[FUNC_1] = func;

	return 0;
}

extern u64 skw_local_clock(void);
void skw_sdio_inband_irq_handler(struct sdio_func *func)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func0 = skw_sdio->sdio_func[FUNC_0];
	int ret;

	if(!skw_sdio->cp_downloaded_flag){//fw not download done
		return;
	}

	if (!debug_infos.cp_assert_time) {
		debug_infos.last_irq_time = skw_local_clock();
		debug_infos.last_irq_times[debug_infos.rx_inband_irq_cnt % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		skw_sdio_dbg("irq coming %d\n", debug_infos.rx_inband_irq_cnt);
	}
	if (!SKW_CARD_ONLINE(skw_sdio)) {
		skw_sdio_err("%s  card offline\n", __func__);
		return;
	}

	skw_resume_check();

	/* send cmd to clear cp int status */
	sdio_claim_host(func0);
	try_to_wakeup_modem(max_ch_num);
	sdio_f0_readb(func0, SDIO_CCCR_INTx, &ret);
	if (!debug_infos.cp_assert_time) {
		debug_infos.last_clear_irq_times[debug_infos.rx_inband_irq_cnt % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.rx_inband_irq_cnt++;
	}
	sdio_release_host(func0);
	if (ret < 0)
		skw_sdio_err("%s error %d\n", __func__, ret);
	skw_sdio_lock_rx_ws(skw_sdio);
	skw_sdio_rx_up(skw_sdio);
}

int skw_sdio_enable_async_irq(void)
{
	int ret = 0;
	u8 reg;
	skw_sdio_dbg("[+]");
	ret = skw_sdio_readb(SDIO_INT_EXT, &reg);
	if (ret)
		return ret;

	reg |= 1 << 1; /* Enable Asynchronous Interrupt */

	ret = skw_sdio_writeb(SDIO_INT_EXT, reg & 0xff);
	if (ret)
		return ret;
	ret = skw_sdio_readb(SDIO_INT_EXT, &reg);
	if (ret)
		return ret;

	if (!(reg & (1 << 1)))
		skw_sdio_err("enable sdio async irq fail reg = 0x%x\n", reg);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
int skw_sdio_set_suspend_indication(void)
{
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if(skw_sdio->irq_type != SKW_SDIO_INBAND_IRQ)
		return 0;
	skw_sdio_info("Enter slp_disable=%d\n", skw_sdio->boot_data->slp_disable);
	ret = skw_sdio_writeb(SDIOHAL_CPLOG_TO_AP_SWITCH, 0x02);
	if (ret < 0) {
		skw_sdio_err("cls the log signal fail ret=%d\n", ret);
		return ret;
	}
	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(5));
	if (ret < 0) {
		skw_sdio_err("cls log irq fail ret=%d\n", ret);
		return ret;
	}
	return 0;
}
int skw_sdio_set_resume_indication(void)
{
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if(skw_sdio->irq_type != SKW_SDIO_INBAND_IRQ)
		return 0;
	skw_sdio_info("Enter slp_disable=%d\n", skw_sdio->boot_data->slp_disable);
	ret = skw_sdio_writeb(SDIOHAL_CPLOG_TO_AP_SWITCH, 0x04);
	if (ret < 0) {
		skw_sdio_err("cls the log signal fail ret=%d\n", ret);
		return ret;
	}
	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(5));
	if (ret < 0) {
		skw_sdio_err("cls log irq fail ret=%d\n", ret);
		return ret;
	}
	return 0;
}

static int skw_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = container_of(dev, struct sdio_func, dev);
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int  ret = 0;

	skw_sdio_dbg("[%s]enter\n", __func__);

	if (skw_sdio->cp_state != 0)
		return -EBUSY;

	skw_sdio_set_suspend_indication();

	atomic_set(&skw_sdio->resume_flag, 0);

	if (SKW_CARD_ONLINE(skw_sdio))
		func->card->host->pm_flags |= MMC_PM_KEEP_POWER;

	func = skw_sdio->sdio_func[FUNC_1];
	send_host_suspend_indication(skw_sdio);
	if ((skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ) && skw_sdio->resume_com) {
		sdio_claim_host(func);
		try_to_wakeup_modem(max_ch_num);
		msleep(1);
		ret = sdio_release_irq(func);
		sdio_release_host(func);
		skw_sdio_dbg("%s sdio_release_irq ret = %d\n", __func__, ret);
	} 
	atomic_set(&skw_sdio->suspending, 1);
#ifdef CONFIG_NO_SERVICE_PD
	if (!skw_sdio->cp_state && skw_sdio->service_state_map==0)
		skw_sdio->power_off = 1;
#endif
	skw_sdio->resume_com = 0;
	return ret;
}

static int skw_sdio_resume(struct device *dev)
{
	struct sdio_func *func = container_of(dev, struct sdio_func, dev);
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;

	skw_sdio_dbg("[%s]enter\n", __func__);
#if defined(SKW_BOOT_DEBUG)
	skw_dloader(2);
#endif
	skw_sdio_set_resume_indication();
	skw_sdio->suspend_wake_unlock_enable =0;
	if (SKW_CARD_ONLINE(skw_sdio))
		func->card->host->pm_flags &= ~MMC_PM_KEEP_POWER;

	func = skw_sdio->sdio_func[FUNC_1];
	send_host_resume_indication(skw_sdio);
	atomic_set(&skw_sdio->resume_flag, 1);
	if (!func->irq_handler && (skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ)) {
		sdio_claim_host(func);
		try_to_wakeup_modem(max_ch_num);
		ret = sdio_claim_irq(func, skw_sdio_inband_irq_handler);
		sdio_release_host(func);
		if(ret < 0) {
			skw_sdio_err("%s sdio_claim_irq ret = %d\n", __func__, ret);
		} else {
			ret = skw_sdio_enable_async_irq();
			if (ret < 0)
				skw_sdio_err("enable sdio async irq fail ret = %d\n", ret);
		}
	}
	return ret;
}
#endif
irqreturn_t skw_gpio_irq_handler(int irq, void *dev_id) //interrupt
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	int value = gpio_get_value(skw_sdio->gpio_in);
	//skw_sdio_info("debug ----line[%d]- cp active[%d]--enter\n", __LINE__,value);
	if (!debug_infos.cp_assert_time) {
		debug_infos.last_irq_time = skw_local_clock();
		debug_infos.last_irq_times[debug_infos.rx_gpio_irq_cnt % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.rx_gpio_irq_cnt++;
		skw_sdio_dbg("irq coming %d\n", debug_infos.rx_gpio_irq_cnt);
	}
	if (!SKW_CARD_ONLINE(skw_sdio)) {
		skw_sdio_err("%s card offline\n", __func__);
		return IRQ_HANDLED;
	}
	if (!skw_sdio->suspend_wake_unlock_enable) {
		skw_sdio_dbg("suspend wake lock enable!!!!\n");
		skw_sdio_lock_rx_ws(skw_sdio);
	}

	if (value && (skw_sdio->irq_type == SKW_SDIO_EXTERNAL_IRQ)){
		skw_sdio_rx_up(skw_sdio);
	 }
	host_gpio_in_routine(value);
	return IRQ_HANDLED;
}

static int skw_check_cp_ready(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if (wait_for_completion_timeout(&skw_sdio->download_done,
		msecs_to_jiffies(2000)) == 0) {
		skw_sdio_err("CP-ready timeout, kick RXTHREAD\n");
		skw_sdio_rx_up(skw_sdio);
		if(skw_sdio->chip_en >= 0)
			skw_sdio_info("chip_en:%d\n", gpio_get_value(skw_sdio->chip_en));
		return -ETIME;
	}
	return 0;
}
static void skw_sdio_launch_thread(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	init_completion(&skw_sdio->rx_completed);
	skw_sdio_wakeup_source_init(skw_sdio);
	skw_sdio->rx_thread =
		kthread_create(skw_sdio_rx_thread, NULL, "skw_sdio_rx_thread");
	if (IS_ERR(skw_sdio->rx_thread)) {
		skw_sdio_err("creat skw_sdio_rx_thread fail\n");
		return;
	}
	if (skw_sdio->rx_thread) {
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
		sched_set_fifo_low(skw_sdio->rx_thread);
#else
		struct sched_param param;
		param.sched_priority = 1;
		sched_setscheduler(skw_sdio->rx_thread, SCHED_FIFO, &param);
#endif
		kthread_bind(skw_sdio->rx_thread, cpumask_first(cpu_online_mask));
		set_user_nice(skw_sdio->rx_thread, SKW_MIN_NICE);
		wake_up_process(skw_sdio->rx_thread);
	} else
		skw_sdio_err("creat skw_sdio_rx_thread fail\n");
}

static int skw_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct mmc_host *host = func->card->host;
	int ret;

	skw_sdio_info(": func->class=%x, vendor=0x%04x, device=0x%04x, "
		      "func_num=0x%04x, clock=%d blksize=0x%x max_blkcnt %d\n",
		      func->class, func->vendor, func->device, func->num,
		      host->ios.clock, func->cur_blksize,
		      func->card->host->max_blk_count);

	ret = skw_sdio_get_dev_func(func);
	if (ret < 0) {
		skw_sdio_err("get func err\n");
		return ret;
	}

	skw_sdio->sdio_dev_host = skw_sdio->sdio_func[FUNC_1]->card->host;
	if (skw_sdio->sdio_dev_host == NULL) {
		kfree(skw_sdio->sdio_func[FUNC_1]);
		skw_sdio->sdio_func[FUNC_1] = NULL;
		skw_sdio_err("get host failed!!!");
		return -1;
	}

	skw_sdio_debugfs_init();
	skw_sdio_launch_thread();
	if (!skw_sdio->pwrseq) {
		struct sdio_func *func1 = skw_sdio->sdio_func[FUNC_1];
		/* Enable Function 1 */
		sdio_claim_host(func1);
		ret = sdio_enable_func(func1);

		skw_sdio_info("sdio_enable_func ret=%d type %d\n", ret, skw_sdio->irq_type);
		if(!ret) {
			sdio_set_block_size(func1, SKW_SDIO_BLK_SIZE);
			func1->max_blksize = SKW_SDIO_BLK_SIZE;
			if (skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ) {
				if(sdio_claim_irq(func1,skw_sdio_inband_irq_handler)) {
					skw_sdio_err("sdio_claim_irq failed\n");
				} else {
					ret = skw_sdio_enable_async_irq();
					if (ret < 0)
						skw_sdio_err("enable sdio async irq fail ret = %d\n", ret);
				}
			}
			sdio_release_host(func1);
		} else {
			sdio_disable_func(func1); //disable func for remove
			sdio_release_host(func1);
			kfree(skw_sdio->sdio_func[FUNC_1]);
			skw_sdio->sdio_func[FUNC_1] = NULL;
			skw_sdio_err("enable func1 err!!! ret is %d\n", ret);
			return ret;
		}
		skw_sdio->resume_com = 1;
		skw_sdio_info("enable func1 done\n");
	} else
		pm_runtime_put_noidle(&func->dev);
	if (!SKW_CARD_ONLINE(skw_sdio))
		atomic_sub(SKW_SDIO_CARD_OFFLINE, &skw_sdio->online);

	check_chipid();
	if(!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",10))
	{
		struct sdio_func *func1 = skw_sdio->sdio_func[FUNC_1];
		skw_sdio_info("SV6160LITE chip\n");
		sdio_claim_host(func1);
		skw_sdio->sdio_func[FUNC_0]->max_blksize = SKW_SDIO_BLK_SIZE;
		sdio_set_block_size(func1, SKW_SDIO_BLK_SIZE);
		func1->max_blksize = SKW_SDIO_BLK_SIZE;
		sdio_release_host(func1);
	}
	/* the card is nonremovable */
	if (skw_sdio->sdio_dev_host->caps & MMC_CAP_NONREMOVABLE) {
		//skw_sdio->sdio_dev_host->caps |= MMC_CAP_NONREMOVABLE; //| MMC_CAP_SDIO_IRQ;
		skw_sdio_info("nonremovable card detected\n");
	} else {
		skw_sdio_info("removable card detected default\n");
#ifdef SKW_SUPPORT_MMC_NONREMOVABLE
		skw_sdio->sdio_dev_host->caps |= MMC_CAP_NONREMOVABLE;
#endif
	}
	skw_sdio_bind_platform_driver(skw_sdio->sdio_func[FUNC_1]);
#ifdef CONFIG_BT_SEEKWAVE
	skw_sdio_bind_btseekwave_driver(skw_sdio->sdio_func[FUNC_1]);
#endif
	skw_sdio->service_state_map = 0;
	skw_sdio->service_index_map = 0;
	skw_sdio->host_active = 1;
	skw_sdio->power_off = 0;
	complete(&skw_sdio->scan_done);
	return 0;
}

static void skw_sdio_remove(struct sdio_func *func)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	skw_sdio_info("Enter\n");

	complete(&skw_sdio->remove_done);

	if (skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ) {
		sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
		sdio_release_irq(skw_sdio->sdio_func[FUNC_1]);
		sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
	}
	if (skw_sdio->irq_num >= 0)
		free_irq(skw_sdio->irq_num, NULL);

	skw_sdio->host_active = 0;
	skw_sdio_unbind_platform_driver(skw_sdio->sdio_func[FUNC_1]);
	skw_sdio_unbind_WIFI_driver(skw_sdio->sdio_func[FUNC_1]);
	skw_sdio_unbind_BT_driver(skw_sdio->sdio_func[FUNC_1]);
	kfree(skw_sdio->sdio_func[FUNC_0]);
}

void skw_sdio_stop_thread(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if (skw_sdio->rx_thread) {
		atomic_set(&skw_sdio->threads_exit, 1);
		skw_sdio_rx_up(skw_sdio);
		kthread_stop(skw_sdio->rx_thread);
		skw_sdio->rx_thread = NULL;
		skw_sdio_wakeup_source_destroy(skw_sdio);
	}
	skw_sdio_info("done\n");
}

static const struct dev_pm_ops skw_sdio_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(skw_sdio_suspend, skw_sdio_resume)
};

static const struct sdio_device_id skw_sdio_ids[] = {
	//{ .compatible = "seekwave-sdio", },
	//{SDIO_DEVICE(0, 0)},
	{SDIO_DEVICE(0x3607, 0x6160)},
	{SDIO_DEVICE(0x1FFE, 0x6621)},
	{},
};

static struct sdio_driver skw_sdio_driver = {
	.probe = skw_sdio_probe,
	.remove = skw_sdio_remove,
	.name = "skw_sdio",
	.id_table = skw_sdio_ids,
	.drv = {
		.pm = &skw_sdio_pm_ops,
	},
};

void skw_sdio_remove_card(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	sdio_unregister_driver(&skw_sdio_driver);
	skw_sdio_info(" sdio_unregister_driver: %s\n", skw_sdio_driver.name);
	wait_for_completion_timeout(&skw_sdio->remove_done, msecs_to_jiffies(100));
	skw_sdio_info("remove card end\n");

}

int skw_sdio_scan_card(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;

	skw_sdio_info("sdio_scan_card\n");
	init_completion(&skw_sdio->scan_done);
	init_completion(&skw_sdio->remove_done);
	init_completion(&skw_sdio->download_done);
	init_completion(&skw_sdio->device_wakeup);
	init_waitqueue_head(&skw_sdio->wq);
	//skw_sdio->irq_type = SKW_SDIO_EXTERNAL_IRQ;
	skw_sdio->irq_type = SKW_SDIO_INBAND_IRQ;
	ret = sdio_register_driver(&skw_sdio_driver);
	if (ret != 0) {
		skw_sdio_driver.name = "skw_sdio_lite";
		skw_sdio->multi_sdio_drivers = 1;
		ret = sdio_register_driver(&skw_sdio_driver);
		skw_sdio_info("sdio_register_driver rename :%s\n", skw_sdio_driver.name);
		if (ret)
			skw_sdio_err("sdio_register_driver error :%d\n", ret);
	}
	if (wait_for_completion_timeout(&skw_sdio->scan_done, msecs_to_jiffies(1000)) == 0) {
		skw_sdio_err("wait scan card time out\n");
		return -ENODEV;
	}
	return ret;
}

/****************************************************************
 *Description:sleep feature support en api
 *Author:junwei.jiang
 *Date:2023-06-14
 * ************************************************************/
static int skw_sdio_slp_feature_en(unsigned int address, unsigned int slp_en)
{
	int ret = 0;
	ret = skw_sdio_writeb(address,slp_en);
	if(ret !=0){
		skw_sdio_err("no-sleep support en write fail, ret=%d\n",ret);
		return -1;
	}
	skw_sdio_info("no-sleep_support_enable:%d\n ",slp_en);
	return 0;
}

/****************************************************************
 *Description:set the dma type SDMA, AMDA
 *Author:junwei.jiang
 *Date:2021-11-23
 * ************************************************************/
static int skw_sdio_set_dma_type(unsigned int address, unsigned int dma_type)
{
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if(dma_type == SDMA){
		/*support the sdma so adma_rx_enable set 0*/
		skw_sdio->adma_rx_enable = 0;
	}
	if(!bind_device){
		ret = skw_sdio_writeb(address, ADMA);
		if(ret !=0){
			skw_sdio_err("dma type write fail, ret=%d\n",ret);
			return -1;
		}
	}
	skw_sdio_info("dma_type=%d,adma_rx_enable:%d\n ",dma_type,skw_sdio->adma_rx_enable);
	return 0;
}

/****************************************************************
*Description:
*Func:used the ap boot cp interface;
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Author：JUNWEI.JIANG
*Date:2021-09-07
****************************************************************/
static int skw_sdio_boot_cp(int boot_mode)
{
	int ret =0;
	//struct sdio_func *func;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	skw_sdio_set_dma_type(skw_sdio->boot_data->dma_type_addr,
			skw_sdio->boot_data->dma_type);
	skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,
			skw_sdio->boot_data->slp_disable);

	//2:download the boot bin 1CPALL 2, wifi 3,bt
	skw_sdio_info("line:%d dram_dl_size = %d iram_dl_size = %d iram_dl_addr = 0x%x, dram_dl_addr = 0x%x\n", __LINE__,
		skw_sdio->boot_data->dram_dl_size, skw_sdio->boot_data->iram_dl_size,
		skw_sdio->boot_data->iram_dl_addr, skw_sdio->boot_data->dram_dl_addr);
	if (skw_sdio->boot_data->dram_dl_size) {
		ret = skw_sdio_dt_write(skw_sdio->boot_data->dram_dl_addr,
					skw_sdio->boot_data->dram_img_data,
					skw_sdio->boot_data->dram_dl_size);
		if (ret != 0)
			goto FAIL;
	}

	if (skw_sdio->boot_data->iram_dl_size) {
		ret = skw_sdio_dt_write(skw_sdio->boot_data->iram_dl_addr,
					skw_sdio->boot_data->iram_img_data,
					skw_sdio->boot_data->iram_dl_size);
		if (ret != 0)
			goto FAIL;
	}

	if (skw_sdio->cp_state) {
		if (skw_sdio->sdio_exti_gpio_state) {
			ret = skw_sdio_writeb(SKW_SDIO_AP2CP_EXTI_SETVAL,
					      (skw_sdio->sdio_exti_gpio_state) &
						      0xff);
			skw_sdio_info("the -exti gpio state is %d\n",
				      (skw_sdio->sdio_exti_gpio_state) & 0xff);
		}
		if (skw_sdio->irq_type == SKW_SDIO_EXTERNAL_IRQ &&
		    skw_sdio->irq_num >= 0) {
			enable_irq(skw_sdio->irq_num);
		}
	}
	//first boot need the setup cp first_dl_flag=0 is first
	skw_sdio_info("line:%d write the download done flag\n", __LINE__);
	skw_sdio->cp_downloaded_flag = 1;
	ret |= skw_sdio_writeb(skw_sdio->boot_data->save_setup_addr, BIT(0));
	if (!skw_sdio->cp_state)
		ret |= skw_check_cp_ready();

	if (ret != 0)
		goto FAIL;

#if !defined(SKW_BOOT_MEMPOWERON)
	if(!ret&&boot_mode==SKW_FIRST_BOOT) {
		if(skw_sdio->chip_en < 0)
			skw_sdio_warn("chip_en:%d Invalid Pls check HW !!\n", skw_sdio->chip_en);
		ret = skw_sdio_host_check(skw_sdio);
		ret |= skw_sdio_chk_cp_gpio_cfg();//check the cp gpio cfg
		if (ret < 0)
			return ret;

		skw_sdio_bind_WIFI_driver(skw_sdio->sdio_func[FUNC_1]);
#ifndef CONFIG_BT_SEEKWAVE
		skw_sdio_bind_BT_driver(skw_sdio->sdio_func[FUNC_1]);
#endif
	}
#endif
	skw_sdio_info("boot cp pass!!\n");
	return ret;
FAIL:
	skw_sdio_dt_get_address();
	skw_sdio_err("fail ret=%d\n", ret);
	//modem_notify_event(DEVICE_BLOCKED_EVENT);
	return ret;
}
int skw_sdio_service_enable(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	uint32_t value = 0;
	int ret = 0;
	skw_sdio->log_data->service_en = SKW_SERVICE_EN; //service enable [8:9]
	//get the service enable reg value
	send_cp_wakeup_signal(skw_sdio);
	ret = skw_sdio_dt_read(skw_sdio->log_data->service_en,
			       (void *)&skw_sdio->log_data->service_eb_val,
			       SKW_REG_RW_LENGTH);
	if (ret < 0) {
		skw_sdio_err("read poweron cp reg  fail ret=%d\n", ret);
		return ret;
	}
	//set the service enable bit [8:9] = 2'b11
	value = (unsigned int)skw_sdio->log_data->service_eb_val;
	value |= 0x300;
	send_cp_wakeup_signal(skw_sdio);
	ret = skw_sdio_dt_write(skw_sdio->log_data->service_en, (void *)&value,
				SKW_REG_RW_LENGTH);
	if (ret < 0) {
		skw_sdio_err("write poweron cp reg  fail ret=%d\n", ret);
		return ret;
	}
	skw_sdio_info(" reg write success\n");
	return ret;
}

/****************************************************************/
// add the sdio memdump poweron cp mem
//6160:
//wifi power on
//40104000[3:2]=2'b0
//6160lite:
//40108000[3:2]=2'b0
//6316:
//40108000[3:2]=2'b0
/**********************************************************/
int skw_sdio_smem_poweron(void)
{
	int ret = 0;
	uint32_t value = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if 1
	if(skw_sdio->log_data->smem_poweron) {
		return ret;
	} else {
		skw_sdio_info("memdump poweron OPS !!\n");
		skw_sdio_service_enable();
		msleep(10);
		skw_sdio->log_data->smem_poweron = 1;
	}
#endif
	//get chip id
	skw_sdio_info(" the chip id:%s\n", (char *)skw_sdio->chip_id);
	if (!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",10)) {
		skw_sdio->log_data->smem_poweron = SKW_SMEM_POWERON2;
	} else if (!strncmp((char *)skw_sdio->chip_id,"SV6316",6)) {
		skw_sdio->log_data->smem_poweron = SKW_SMEM_POWERON2;
	} else if(!strncmp((char *)skw_sdio->chip_id,"SV6160",6)) {
		skw_sdio->log_data->smem_poweron = SKW_SMEM_POWERON1;//SKW_SMEM_POWERON1
	} else {
		skw_sdio_err("chip_id ls unkown not support memdump!\n");
		return -1;
	}
	//get the memdump addr value
	send_cp_wakeup_signal(skw_sdio);
	ret = skw_sdio_dt_read(skw_sdio->log_data->smem_poweron,
		(void *)&skw_sdio->log_data->reg_val, SKW_REG_RW_LENGTH);
	if(ret <0) {
		skw_sdio_err("read poweron cp reg  fail ret=%d\n", ret);
		return ret;
	}
	//if vaule [3:2] =2'b0 means power on done,else set the value [3:2]=2'b0
	if (!((unsigned int)skw_sdio->log_data->reg_val & (0x3<<2))) {
		skw_sdio_info("cp has poweron done reg  addr:%x value:%x\n",
		skw_sdio->log_data->smem_poweron,(unsigned int)skw_sdio->log_data->reg_val);
		return 0;
	}
	//set the memdump addr value 40108000[3:2]=2'b0;
	value = (unsigned int)skw_sdio->log_data->reg_val;
	value &= ~(0x3<<2);
	skw_sdio_info("poweron cp reg  addr:%x value:%x\n",
		skw_sdio->log_data->smem_poweron,value);
	send_cp_wakeup_signal(skw_sdio);
	ret = skw_sdio_dt_write(skw_sdio->log_data->smem_poweron,
		(void *)&value, SKW_REG_RW_LENGTH);
	if (ret <0) {
		skw_sdio_err("write	poweron cp reg  fail ret=%d\n", ret);
		return ret;
	}
	return 0;
}

/************************************************************************
 *Decription:release CP close the CP log
 *Author:junwei.jiang
 *Date:2023-02-16
 *Modfiy:
 *
 ********************************************************************* */
int skw_sdio_cp_log_disable(int disable)
{
	int ret = 0;
	skw_sdio_info("Enter\n");
	ret = skw_sdio_writeb(SDIOHAL_CPLOG_TO_AP_SWITCH, disable);
	if (ret < 0) {
		skw_sdio_err("cls the log signal fail ret=%d\n", ret);
		return ret;
	}
	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(5));
	if (ret < 0) {
		skw_sdio_err("cls log irq fail ret=%d\n", ret);
		return ret;
	}
	if (!disable)
		skw_sdio_info(" enable the CP log \n");
	else
		skw_sdio_info(" disable the CP log !!\n");

	return 0;
}

void skw_sdio_cp_dumpmem(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio->cp_state = 3;
	skw_sdio_info("Enter cp_state:%d \n", skw_sdio->cp_state);
	modem_notify_event(DEVICE_BLOCKED_EVENT);
}

/************************************************************************
 *Decription:send WIFI start command to modem.
 *Author:junwei.jiang
 *Date:2022-10-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_WIFI_service_start(void)
{
	int ret;

	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("Enter STARTWIFI cp_state:%d\n",skw_sdio->cp_state);
	if (skw_sdio->service_state_map & (1<<WIFI_SERVICE))
		return 0;

	mutex_lock(&skw_sdio->except_mutex);
	if (skw_sdio->service_state_map==0 && skw_sdio->power_off){
		skw_reinit_completion(skw_sdio->download_done);
		skw_recovery_mode();
    }
	skw_reinit_completion(skw_sdio->download_done);
	ret = send_modem_service_command(WIFI_SERVICE, SERVICE_START);
	if (ret==0)
		ret = skw_check_cp_ready();
	mutex_unlock(&skw_sdio->except_mutex);
	return ret;
}


/************************************************************************
 *Decription: send WIFI stop command to modem.
  *Author:junwei.jiang
 *Date:2022-10-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_WIFI_service_stop(void)
{
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if CONFIG_NO_SERVICE_PD
	struct sdio_func *sdio_func = skw_sdio->sdio_func[FUNC_1];
#endif
	skw_sdio_info("Enter,STOPWIFI  cp_state:%d",skw_sdio->cp_state);
	mutex_lock(&skw_sdio->except_mutex);
	if (skw_sdio->service_state_map & (1<<WIFI_SERVICE))
		ret = send_modem_service_command(WIFI_SERVICE, SERVICE_STOP);
	if (!skw_sdio->cp_state && ret==0 && skw_sdio->service_state_map==0) {
#if CONFIG_NO_SERVICE_PD
		skw_sdio->service_index_map = 0;
		skw_sdio->power_off = 1;
		skw_sdio->cp_downloaded_flag = 0;

		sdio_claim_host(sdio_func);
		skw_chip_set_power(0);
		sdio_release_host(sdio_func);
		skw_sdio_info("chip power off %d\n", ret);

#endif
	}
	mutex_unlock(&skw_sdio->except_mutex);
	return ret;
}
/************************************************************************
 *Decription:send BT start command to modem.
 *Author:junwei.jiang
 *Date:2022-10-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_BT_service_start(void)
{
	int ret;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("Enter cpstate=%d\n",skw_sdio->cp_state);
	if(assert_context_size)
		skw_sdio_info("%s\n", assert_context);
	if (skw_sdio->service_state_map & (1<<BT_SERVICE))
		return 0;

	mutex_lock(&skw_sdio->except_mutex);
	if (skw_sdio->service_state_map==0 && skw_sdio->power_off){
		skw_reinit_completion(skw_sdio->download_done);
		skw_recovery_mode();
    }
	skw_reinit_completion(skw_sdio->download_done);
	ret =send_modem_service_command(BT_SERVICE, SERVICE_START);
	if (!ret)
		ret = skw_check_cp_ready();
	mutex_unlock(&skw_sdio->except_mutex);
	return ret;
}

/************************************************************************
 *Decription:send BT stop command to modem.
 *Author:junwei.jiang
 *Date:2022-10-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_BT_service_stop(void)
{
	int ret = 0;

	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if CONFIG_NO_SERVICE_PD
	struct sdio_func *sdio_func = skw_sdio->sdio_func[FUNC_1];
#endif
	skw_sdio_info("Enter cpstate=%d\n",skw_sdio->cp_state);

	mutex_lock(&skw_sdio->except_mutex);
	if (skw_sdio->service_state_map & (1<<BT_SERVICE) && !skw_sdio->cp_state) {
		skw_reinit_completion(skw_sdio->download_done);
		ret = send_modem_service_command(BT_SERVICE, SERVICE_STOP);
	}
	if (!skw_sdio->cp_state && ret==0 && skw_sdio->service_state_map==0) {
#if CONFIG_NO_SERVICE_PD
		skw_sdio->service_index_map = 0;
		skw_sdio->power_off = 1;
		skw_sdio->cp_downloaded_flag = 0;
		sdio_claim_host(sdio_func);
		skw_chip_set_power(0);
		sdio_release_host(sdio_func);

		skw_sdio_info("chip power off %d\n", ret);

#endif
	}
	mutex_unlock(&skw_sdio->except_mutex);
	kill_BT_rx_transfer();
	return ret;
}

/****************************************************************
*Description:
*Func:used the ap boot cp interface;
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Author：JUNWEI.JIANG
*Date:2021-09-07
****************************************************************/
static int skw_sdio_cp_service_ops(int service_ops)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	switch(service_ops)
	{
		case SKW_WIFI_START:
			ret = wait_event_interruptible_timeout(skw_sdio->wq,
				!skw_sdio->cp_state, msecs_to_jiffies(2000));
			if (ret <= 0) {
				skw_sdio_err("cp_state:%d cp not ready!!\n",
					     skw_sdio->cp_state);
			}
			ret = skw_WIFI_service_start();
			skw_sdio_info("-----WIFI SERIVCE START\n");
		break;
		case SKW_WIFI_STOP:
			ret =skw_WIFI_service_stop();
			skw_sdio_dbg("----WIFI SERVICE---STOP\n");
		break;
		case SKW_BT_START:
		{
			ret = wait_event_interruptible_timeout(skw_sdio->wq,
				!skw_sdio->cp_state, msecs_to_jiffies(2000));
			if (ret <= 0) {
				skw_sdio_err("cp_state:%d cp not ready!!\n",
							skw_sdio->cp_state);
			}
			ret=skw_BT_service_start();
			skw_sdio_dbg("-----BT SERIVCE --START\n");
		}
		break;
		case SKW_BT_STOP:
			ret =skw_BT_service_stop();
			skw_sdio_dbg("-----BT SERVICE --STOP\n");
		break;
		default:
		break;
	}
	return ret;
}
int skw_sdio_chk_cp_gpio_cfg(void)
{
	int ret = 0;
	unsigned char exti_val = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if (skw_sdio->gpio_in < 0 || skw_sdio->gpio_out < 0) {
		skw_sdio_info("gpio_in and gpio_out no config\n");
		return ret;
	}
	if (!cp_detect_sleep_mode) {
		skw_sdio_err(
			"GPIOOUT:%d cannot be operated, pls check gpio num or hw connect!!.\n",
			skw_sdio->gpio_out);
		return -1;
	}
	if (skw_sdio->gpio_in == skw_sdio->gpio_out) {
		skw_sdio_err(
			"gpio_in :%d and gpio_out:%d can't be the same!!\n",
			skw_sdio->gpio_in, skw_sdio->gpio_out);
		return -1;
	}

	if (cp_detect_sleep_mode == 1 || cp_detect_sleep_mode == 2) {
		skw_sdio->sdio_exti_gpio_state = cp_detect_sleep_mode;
	} else if (cp_detect_sleep_mode == 3) {
		ret = skw_sdio_readb(SKW_SDIO_CP2AP_EXTI_GETVAL, &exti_val);
		if (ret) {
			skw_sdio_err("read exti val fail exti:%d, ret=%d \n", exti_val,
					 ret);
			return -1;
		}
		if (exti_val == 3) {
			skw_sdio_err(
				"exti val:%d error, pls check your gpio num config or hw connect !!\n",
				exti_val);
			return -1;
		}
		skw_sdio->sdio_exti_gpio_state = exti_val;
	}
	skw_sdio_info("chk cp gpio cfg is %d\n", skw_sdio->sdio_exti_gpio_state);
	return ret;
}

/****************************************************************
*Description:skw_boot_loader
*Func:used the ap boot cp interface;
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Author：JUNWEI.JIANG
*Date:2021-09-07
****************************************************************/
int skw_boot_loader(struct seekwave_device *boot_data)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if defined(SKW_BOOT_MEMPOWERON)
	struct sdio_func *func;
	skw_sdio->boot_data= boot_data;
	skw_sdio->gpio_in = skw_sdio->boot_data->gpio_in;
	skw_sdio->gpio_out = skw_sdio->boot_data->gpio_out;
	if(!skw_sdio->boot_data->first_boot_flag ){
		if(skw_sdio->boot_data->iram_img_data) {
			if(skw_sdio->boot_data->gpio_in >= 0) {
				skw_sdio_set_dma_type(skw_sdio->boot_data->dma_type_addr,
						skw_sdio->boot_data->dma_type);
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,
						skw_sdio->boot_data->slp_disable);
				func = skw_sdio->sdio_func[FUNC_1];
				sdio_claim_host(func);
				try_to_wakeup_modem(max_ch_num);
				ret = sdio_release_irq(func);
				sdio_release_host(func);
				skw_sdio->irq_type = SKW_SDIO_EXTERNAL_IRQ;
				ret = skw_sdio_host_irq_init(skw_sdio->gpio_in);
				if (ret < 0) {
					skw_sdio_err("gpio irq init  fail\n");
					goto FAIL;
				}
			} else {
				ret = skw_sdio_cpdebug_boot();
				if (ret < 0) {
					skw_sdio_err("cpdebug_boot fail\n");
					goto FAIL;
				}
		}
		if(skw_sdio->boot_data->chip_en < 0){
			skw_sdio_err("chip_en:%d Invalid Pls check HW !!\n", skw_sdio->boot_data->chip_en);
			ret = -1;
			return ret;
		}

		ret = skw_sdio_host_check(skw_sdio);
		if (ret < 0)
			return ret;
		skw_sdio_bind_WIFI_driver(skw_sdio->sdio_func[FUNC_1]);
#ifndef CONFIG_BT_SEEKWAVE
		skw_sdio_bind_BT_driver(skw_sdio->sdio_func[FUNC_1]);
#endif
		if(!skw_sdio->service_index_map&&skw_sdio->boot_data->iram_img_data){
			skw_chip_set_power(0);
			skw_sdio->power_off = 1;
			skw_sdio_info(" No service Power Down CP--!!!\n");
		}else{
			skw_sdio_err("sevice_index_map:%d or iram_img_data is NULL Pls CONFIG!!\n", skw_sdio->service_index_map);
			ret = -1;
			return ret;
		}
	}else{
		if(!skw_sdio->service_index_map&&skw_sdio->boot_data->iram_img_data){
#ifdef CONFIG_SV6160_LITE_FPGA
			skw_sdio_info(" FPGA DEUBG NO Down CP--!!!\n");
			skw_sdio_boot_cp(RECOVERY_BOOT);
#else
			skw_sdio_info("first boot recovery dl firmware -!!!\n");
			skw_recovery_mode();
#endif
		}else{
			if(skw_sdio->boot_data->dl_base_img){
				skw_sdio_info("the dloader CP IMG dl_done_signal =%d the dl_base_addr =0x%x,offset=0x%x dl_size=0x%x---!\n",
				skw_sdio->boot_data->dl_done_signal,skw_sdio->boot_data->dl_base_addr,skw_sdio->boot_data->dl_offset_addr,
				skw_sdio->boot_data->dl_size);
#if 0
				print_hex_dump(KERN_ERR, "dump_data:", 0, 16, 1,
				skw_sdio->boot_data->dl_base_img+skw_sdio->boot_data->dl_offset_addr, 32, 1);
#endif
				if(skw_sdio->boot_data->dl_size){
					ret = skw_sdio_dt_write(skw_sdio->boot_data->dl_base_addr,
							skw_sdio->boot_data->dl_base_img+skw_sdio->boot_data->dl_offset_addr,
							skw_sdio->boot_data->dl_size);
					if(ret)
						goto FAIL;
				}
				ret = skw_sdio_writeb(skw_sdio->boot_data->save_setup_addr,skw_sdio->boot_data->dl_done_signal);
				if(ret)
					goto FAIL;
			}
		}
		ret=skw_sdio_cp_service_ops(skw_sdio->boot_data->service_ops);
	}
#else
	skw_sdio->boot_data= boot_data;
	if (skw_sdio->power_off)
		boot_data->dl_module = RECOVERY_BOOT;
	/*--------CP RESET RESCAN------------*/
	if (boot_data->dl_module== RECOVERY_BOOT) {
		skw_sdio_info("rescan and reset CHIP\n");
		skw_recovery_mode();
	} else {
	/*-------FIRST AP BOOT--------------*/
		if (!skw_sdio->boot_data->first_dl_flag) {
			if (!strncmp((char *)skw_sdio->chip_id,"SV6160",10)){
				boot_data->chip_id = 0x6160;
				skw_sdio_info("boot chip id 0x%x\n", boot_data->chip_id);
			}
			skw_sdio->chip_en = boot_data->chip_en;
			if (skw_sdio->boot_data->iram_dl_size&&
					skw_sdio->boot_data->dram_dl_size){
				ret=skw_sdio_boot_cp(SKW_FIRST_BOOT);
			} else
				ret=skw_sdio_cpdebug_boot();

			if(ret !=0)
				goto FAIL;
		}
	}
	ret=skw_sdio_cp_service_ops(skw_sdio->boot_data->service_ops);
#endif
	if(ret !=0)
		goto FAIL;

	skw_sdio_info("boot loader ops end!!!\n");
	return 0;
FAIL:
	skw_sdio_err("line:%d  fail ret=%d\n", __LINE__, ret);
	return ret;
}

void get_bt_antenna_mode(char *mode)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct seekwave_device *boot_data = skw_sdio->boot_data;
	u32 bt_antenna = boot_data->bt_antenna;

	if(bt_antenna==0)
		return;
	bt_antenna--;
	if(!mode)
		return;
	if (bt_antenna)
		sprintf(mode,"bt_antenna : alone\n");
	else
		sprintf(mode,"bt_antenna : share\n");
}

void reboot_to_change_bt_antenna_mode(char *mode)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct seekwave_device *boot_data = skw_sdio->boot_data;
	u32 *data = (u32 *) &boot_data->iram_img_data[boot_data->head_addr-4];
	u32 bt_antenna;

	if(boot_data->bt_antenna == 0)
		return;

	bt_antenna = boot_data->bt_antenna - 1;
	bt_antenna = 1 - bt_antenna;
	data[0] = (bt_antenna) | 0x80000000;
	if(!mode)
		return;
	if (bt_antenna==1) {
		boot_data->bt_antenna = 2;
		sprintf(mode,"bt_antenna : alone\n");
	} else {
		boot_data->bt_antenna = 1;
		sprintf(mode,"bt_antenna : share\n");
	}
	//skw_recovery_mode();
	send_modem_assert_command();
}

void reboot_to_change_bt_uart1(char *mode)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct seekwave_device *boot_data = skw_sdio->boot_data;
	u32 *data = (u32 *) &boot_data->iram_img_data[boot_data->head_addr-4];

	if(data[0] & 0x80000000)
		data[0] |=  0x0000008;
	else
		data[0] = 0x80000008;
	//skw_recovery_mode();
	send_modem_assert_command();
}

/****************************************************************
*Description:check dev ready
*Func:used the ap boot cp interface;
*Calls:sdio or usb
*Call By:host dev ready
*Input:NULL
*Output:pass :0 or fail ENODEV
*Others:
*Author：JUNWEI.JIANG
*Date:2022-06-09
****************************************************************/
int skw_reset_bus_dev(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if (skw_sdio->chip_en) {
		skw_chip_power_reset();
	} else {
		skw_sdio_err("the chip_en is NULL\n");
		skw_sdio_reg_reset_cp();
	}
	return 0;
}
static int skw_sdio_reg_reset_cp(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret = 0;
	if (skw_sdio->chip_en >= 0)
		return ret;
	/*reset CP register set value:0x4010001C 0x20:CP sys hif reset*/
	if (!strncmp((char *)skw_sdio->chip_id, "SV6160LITE", 10) ||
	    !strncmp((char *)skw_sdio->chip_id, "SV6316", 6)) {
		ret = skw_sdio_writeb(0x125, BIT(7));
	} else if (!strncmp((char *)skw_sdio->chip_id, "SV6160", 6)) {
		ret = skw_sdio_writeb(0x125, BIT(0));
	}
	if(ret < 0){
		skw_sdio_err("cia the chip %s reset fail\n",(char *)skw_sdio->chip_id);
		return ret;
	}
	skw_sdio_info(" %s cia reset !\n", (char *)skw_sdio->chip_id);
	return ret;
}

/****************************************************************
*Description:skw_sdio_reset_card
*Func:used the ap boot cp interface;
*Calls:boot data
*Call By:the ap host
*Input:the boot data informations
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Others:
*Author：JUNWEI.JIANG
*Date:2021-10-11
****************************************************************/

static int skw_sdio_reset_card(void)
{
	int ret;
	int skw_sd_id = SKW_MMC_HOST_SD_INDEX;
	skw_sdio_info(" [+]\n");
	skw_sdio_reg_reset_cp();
	ret = skw_sdio_mmc_rescan(skw_sd_id);
	if (ret < 0) {
		skw_sdio_err("the reset card fail try again\n");
		ret = skw_sdio_mmc_rescan(skw_sd_id);
	}
	skw_sdio_info(" [-]\n");
	return ret;
}

static int skw_sdio_cp_reset(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret;
	struct sdio_func *func = skw_sdio->sdio_func[FUNC_1];
	skw_sdio_info(" [+]\n");
	/* Disable Function 1 */
	if (skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ){
		sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
		ret=sdio_release_irq(skw_sdio->sdio_func[FUNC_1]);
		sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		if(ret < 0)
			skw_sdio_err(" sdio_release_irq ret = %d\n", ret);
	}

	ret = skw_sdio_reset_card();
	if (ret < 0) {
		skw_sdio_err("the reset card fail \n");
		return ret;
	}
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_set_block_size(func, SKW_SDIO_BLK_SIZE);
	func->max_blksize = SKW_SDIO_BLK_SIZE;
	/* Enable Function 1 */
	if (skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ){
		ret=sdio_claim_irq(skw_sdio->sdio_func[FUNC_1], skw_sdio_inband_irq_handler);
		if(ret < 0) {
			skw_sdio_err(" sdio_claim_irq ret = %d\n", ret);
		} else {
			ret = skw_sdio_enable_async_irq();
			if (ret < 0)
				skw_sdio_err("enable sdio async irq fail ret = %d\n", ret);
		}
	} else {
		skw_sdio_info("the CP is external irq\n");
	}
	sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
	if ( ret < 0) {
		skw_sdio_err("sdio_claim_irq fail\n");
		return ret;
	}
	skw_sdio_info("CP RESET OK!\n");
	return 0;
}
/****************************************************************
*Description:skw_sdio_cpdebug_boot
*Func:used the ap boot cp interface;
*Others:
*Author：JUNWEI.JIANG
*Date:2022-07-15
****************************************************************/
static int skw_sdio_cpdebug_boot(void)
{
	int ret =0;
	struct sdio_func *func;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("not download CP from AP!!!!\n");
	skw_sdio_set_dma_type(skw_sdio->boot_data->dma_type_addr,
			skw_sdio->boot_data->dma_type);
	skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,
			skw_sdio->boot_data->slp_disable);
	if(skw_sdio->gpio_in >=0) {
		func = skw_sdio->sdio_func[FUNC_1];
		sdio_claim_host(func);
		try_to_wakeup_modem(max_ch_num);
		ret = sdio_release_irq(func);
		sdio_release_host(func);
		skw_sdio->irq_type = SKW_SDIO_EXTERNAL_IRQ;
		ret = skw_sdio_host_irq_init(skw_sdio->gpio_in);
	}
	skw_sdio_info(" CP DUEBGBOOT Done!!!\n");
	return ret;
}

/****************************************************************
*Description:skw_recovery_mode
*Func:used the ap boot cp interface;
*Calls:boot data
*Call By:the ap host
*Input:the boot data informations
*Output:reset cp
*Return：0:pass; other : fail
*Others:
*Author：JUNWEI.JIANG
*Date:2022-07-15
****************************************************************/
int skw_recovery_mode(void)
{
	int ret=0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("the CHIPID:%s \n", (char *)&skw_sdio->chip_id);
	ret = skw_sdio_recovery_debug_status();
	if(!skw_sdio->boot_data->dram_dl_size ||
		!skw_sdio->boot_data->iram_dl_size || ret) {
		skw_sdio_err("CP DEBUG BOOT,AND NO NEED RECOVERY!!! \n");
		return -1;
	}
	skw_sdio->power_off = 1;
	if (skw_sdio->irq_type == SKW_SDIO_EXTERNAL_IRQ &&
	    skw_sdio->irq_num >= 0) {
		disable_irq(skw_sdio->irq_num);
	}
	ret=skw_sdio_cp_reset();
	if(ret!=0){
		skw_sdio_err("CP RESET fail \n");
		return -1;
	}
	skw_sdio->power_off = 0;
	skw_sdio->service_index_map = 0;
	skw_sdio->cp_fifo_status = 0;

	ret = skw_sdio_boot_cp(RECOVERY_BOOT);
	if(ret!=0){
		skw_sdio_err("CP RESET fail \n");
		return -1;
	}
	skw_sdio_info("Recovery ok\n");
	return ret;
}

/****************************************************************
*Description:poweron_bt_mem
*Func:used the ap boot cp interface;
*Calls:boot data
*Call By:the ap host
*Input:the boot data informations
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Others:
*Author：JUNWEI.JIANG
*Date:2021-09-07
****************************************************************/
static int poweron_bt_mem(struct seekwave_device *boot_data)
{
	int ret=0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	ktime_t cur, stop;

#if !defined(SKW_BOOT_DEBUG)
	//ops the power on wifi reg;
	unsigned char bt_poweron_reg = 0;
	unsigned char tmp_val=0;
#endif
	if (skw_sdio->service_state_map & (1<<BT_SERVICE)){
		skw_sdio_info("No need power on BT mem!!!\n");
		return 0;
	}
	skw_sdio_info("---Enter---\n");
	//set the poweron wifi flag 0x164
	skw_sdio_readb(SKW_SDIO_DL_CP2AP_BSP, &tmp_val);
	ret = skw_sdio_writeb(SKW_SDIO_DL_POWERON_MODULE, SKW_BT);
	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(6));
	if(ret !=0){
		skw_sdio_err("%s poweron fail \n", __func__);
		return -1;
	}
	//the flag 164 tell the CP poweron the
	stop = ktime_add_ns(ktime_get(), 2000);
#if !defined(SKW_BOOT_DEBUG)
	do{
		//SDIO AON 180
		ret = skw_sdio_readb(SKW_SDIO_DL_CP2AP_BSP, &bt_poweron_reg);
		cur = ktime_get();
		// get the cp poweron the wifi signal /184
	}while(tmp_val==bt_poweron_reg && !ret && ktime_before(cur, stop));
#endif
	if(ret !=0 ){
		skw_sdio_info("%s power on BT fail \n", __func__);
		return -1;
	}
	//Poweron_Module = boot_data->dl_module;
	skw_sdio->boot_data->dl_module = SKW_BT;
	//ops the poweron the bt reg
	skw_sdio_info("%s power on pass---time=%lld \n", __func__, ktime_to_us(ktime_sub(stop, cur)));
	return 0;
}
/****************************************************************
*Description:poweron_wifi_mem
*Func:used the ap boot cp interface;
*Calls:boot data
*Call By:the ap host
*Input:the boot data informations
*Output:the dloader the bin to cp
*Return：0:pass; other : fail
*Others: 
*Author：JUNWEI.JIANG
*Date:2021-09-07
****************************************************************/
static int poweron_wifi_mem(struct seekwave_device *boot_data)
{
	int ret=0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	ktime_t cur, stop;

#if !defined(SKW_BOOT_DEBUG)
	//ops the power on wifi reg;
	unsigned char wifi_poweron_reg = 0;
	unsigned char tmp_val=0;
#endif
	if (skw_sdio->service_state_map & (1<<WIFI_SERVICE)){
		skw_sdio_info(" No need power on WIFI mem!!! \n");
		return ret;
	}

	skw_sdio_info("---Enter---\n");
	//set the poweron wifi flag 0x164
	msleep(5);//waiting cp ready...
	skw_sdio_readb(SKW_SDIO_DL_CP2AP_BSP, &tmp_val);
	ret = skw_sdio_writeb(SKW_SDIO_DL_POWERON_MODULE, SKW_WIFI);
	ret = skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(6));
	if(ret !=0){
		skw_sdio_err("%s poweron fail \n", __func__);
		return -1;
	}
	//the flag 164 tell the CP poweron the
	stop = ktime_add_ns(ktime_get(), 2000);
#if !defined(SKW_BOOT_DEBUG)
	do{
		//SDIO AON 180
		ret = skw_sdio_readb(SKW_SDIO_DL_CP2AP_BSP, &wifi_poweron_reg);
		cur = ktime_get();
		// get the cp poweron the wifi signal /184
	}while(tmp_val==wifi_poweron_reg && ktime_before(cur, stop));
#endif
	//ret = ktime_before(cur, stop);
	if(ret !=0 ){

		skw_sdio_info(" power on WIFI fail \n");
		return -1;
	}
	skw_sdio->boot_data->dl_module = SKW_WIFI;

	//Poweron_Module = boot_data->dl_module;
	skw_sdio_info("%s power on wifi pass \n", __func__);
	return 0;
}

/****************************************************************
*Description:skw_sdio_poweron_mem
*Func:used the ap boot cp interface;
*Others:
*Author：JUNWEI.JIANG
*Date:2022-07-15
****************************************************************/
int skw_sdio_poweron_mem(int index)
{
	int ret=0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info(" Enter\n");
	//skw_sdio_poweron_mem(index);
	switch(index)
	{
		case SKW_WIFI:
			poweron_wifi_mem(skw_sdio->boot_data);
		break;
		case SKW_BT:
			poweron_bt_mem(skw_sdio->boot_data);
		break;
		default:
			skw_sdio_info("no need poweron service mem\n");
		break;
	}
	return ret;

}
/****************************************************************
*Description:skw_sdio_dloader
*Func:used the ap boot cp interface;
*Others:
*Author：JUNWEI.JIANG
*Date:2022-07-15
****************************************************************/
int skw_sdio_dloader(int service_index)
{
	int ret=0;
#if defined(SKW_BOOT_MEMPOWERON)
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info(" Enter\n");
	mutex_lock(&dloader_mutex);
	ret=skw_sdio_poweron_mem(service_index);
	if(ret){
		skw_sdio_err("power the module=%d fail\n", service_index);
		mutex_unlock(&dloader_mutex);
		return ret;
	}
	switch(service_index)
	{
		case SKW_BOOT:
			//skw_sdio->boot_data->skw_dloader_module(SKW_BOOT);
            skw_recovery_mode();
		break;
		case SKW_WIFI:
			if(skw_sdio->gpio_in  >= 0&&skw_sdio->gpio_out >= 0){
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,1);
				send_cp_wakeup_signal(skw_sdio);
				skw_sdio->boot_data->skw_dloader_module(SKW_WIFI);
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,0);
				send_cp_wakeup_signal(skw_sdio);
			}else{
				skw_sdio_warn("the gpio_in=%d gpio_out=%d is not set\n",skw_sdio->gpio_in,skw_sdio->gpio_out);
			}
		break;
		case SKW_BT:
			if(skw_sdio->gpio_in  >= 0&&skw_sdio->gpio_out >= 0){
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,1);
				send_cp_wakeup_signal(skw_sdio);
				skw_sdio->boot_data->skw_dloader_module(SKW_BT);
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,0);
				send_cp_wakeup_signal(skw_sdio);
			}else{
				skw_sdio_warn("the gpio_in=%d gpio_out=%d is not set\n",skw_sdio->gpio_in,skw_sdio->gpio_out);
			}
		break;
		case SKW_ALL:
			if(skw_sdio->gpio_in  >= 0&&skw_sdio->gpio_out >= 0){
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,1);
				send_cp_wakeup_signal(skw_sdio);
				skw_sdio->boot_data->skw_dloader_module(SKW_ALL);
				skw_sdio_slp_feature_en(skw_sdio->boot_data->slp_disable_addr,0);
				send_cp_wakeup_signal(skw_sdio);
			}else{
				skw_sdio_warn("the gpio_in=%d gpio_out=%d is not set\n",skw_sdio->gpio_in,skw_sdio->gpio_out);
			}
		break;
		default:
			skw_sdio_info("no need the dloader servce mem\n");
		break;
	}
	mutex_unlock(&dloader_mutex);
#else
	skw_sdio_info("no need the dloader servce mem\n");
#endif
	return ret;

}

static int check_chipid(void)
{
	int ret;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	ret = skw_sdio_dt_read(SKW_CHIP_ID0, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	if(!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",10)){
		skw_cp_ver = SKW_SDIO_V20;
		max_ch_num = SDIO2_MAX_CH_NUM;
		max_pac_size = MAX2_PAC_SIZE;
		skw_sdio_blk_size = 512;
		skw_sdio_info("Chip id:%s used SDIO20 ", (char *)skw_sdio->chip_id);
	}else if(!strncmp((char *)skw_sdio->chip_id,"SV6160",6)){
		skw_cp_ver = SKW_SDIO_V10;
		max_ch_num = MAX_CH_NUM;
		max_pac_size = MAX_PAC_SIZE;
		skw_sdio_blk_size = 256;
		skw_sdio_info("Chip id:%s used SDIO10",(char *)skw_sdio->chip_id);
	}else if(!strncmp((char *)skw_sdio->chip_id,"SV6316",6)){
		skw_cp_ver = SKW_SDIO_V20;
		max_ch_num = SDIO2_MAX_CH_NUM;
		max_pac_size = MAX2_PAC_SIZE;
		skw_sdio_blk_size = 512;
		skw_sdio_info("Chip id:%s used SDIO20 ", (char *)skw_sdio->chip_id);
	}else{
		skw_cp_ver = SKW_SDIO_V20;
		max_ch_num = SDIO2_MAX_CH_NUM;
		max_pac_size = MAX2_PAC_SIZE;
		skw_sdio_blk_size = 512;
		skw_sdio_info("Chip id:%s used SDIO20 ", (char *)skw_sdio->chip_id);
	}
	if(ret<0){
		skw_sdio_err("Get the chip id fail!!\n");
		return ret;
	}
	return 0;
}
#if 0
static int skw_get_chipid(char *chip_id)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	chip_id = (char *)&skw_sdio->chip_id;
	skw_sdio_info("---the chip id---%s\n", (char *)skw_sdio->chip_id);
	return 0;
}
#endif


void skw_get_sdio_config(char *buffer, int size)
{
	int ret = 0;

	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_func *func1 = skw_sdio->sdio_func[FUNC_1];
	struct mmc_host *host = func1->card->host;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};

	if(!buffer) {
		skw_sdio_info("buffer is null!\n");
		return;
	}
	ret += sprintf(&buffer[ret], "sdio infomation:\n");
	ret += sprintf(&buffer[ret], "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		ret += sprintf(&buffer[ret], "actual clock:\t%u Hz\n", host->actual_clock);
	ret += sprintf(&buffer[ret], "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		ret += sprintf(&buffer[ret], "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		ret += sprintf(&buffer[ret], "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		ret += sprintf(&buffer[ret], "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "power mode:\t%u (%s)\n", ios->power_mode, str);
	ret += sprintf(&buffer[ret], "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR12:
		str = "sd uhs SDR12";
		break;
	case MMC_TIMING_UHS_SDR25:
		str = "sd uhs SDR25";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_DDR52:
		str = "mmc DDR52";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc HS200";
		break;
	case MMC_TIMING_MMC_HS400:
		str = host->card->host->ios.enhanced_strobe ?
			"mmc HS400 enhanced strobe" : "mmc HS400";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "timing spec:\t%u (%s)\n", ios->timing, str);

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		str = "3.30 V";
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		str = "1.80 V";
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		str = "1.20 V";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "signal voltage:\t%u (%s)\n", ios->signal_voltage, str);

	switch (ios->drv_type) {
	case MMC_SET_DRIVER_TYPE_A:
		str = "driver type A";
		break;
	case MMC_SET_DRIVER_TYPE_B:
		str = "driver type B";
		break;
	case MMC_SET_DRIVER_TYPE_C:
		str = "driver type C";
		break;
	case MMC_SET_DRIVER_TYPE_D:
		str = "driver type D";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "driver type:\t%u (%s)\n", ios->drv_type, str);



	switch (skw_sdio->irq_type) {
	case SKW_SDIO_INBAND_IRQ:
		str = "INBAND_IRQ";
		break;
	case SKW_SDIO_EXTERNAL_IRQ:
		str = "EXTERNAL_IRQ";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "irq type:\t%u (%s)\n", skw_sdio->irq_type, str);

	if(!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",10)){
		str = "SV6160LITE";
	}else if(!strncmp((char *)skw_sdio->chip_id,"SV6160",6)){
		str = "SV6160";
	}else if(!strncmp((char *)skw_sdio->chip_id,"SV6316",6)){
		str = "SV6316";
	}else{
		str = "invalid";
	}

	ret += sprintf(&buffer[ret], "chipid:\t%s (%s)\n", (char *)skw_sdio->chip_id, str);

	switch (skw_cp_ver) {
	case SKW_SDIO_V10:
		str = "sdio v1.0";
		break;
	case SKW_SDIO_V20:
		str = "sdio v2.0";
		break;
	default:
		str = "invalid";
		break;
	}
	ret += sprintf(&buffer[ret], "skw_cp_ver:\t%u (%s)\n", skw_cp_ver,str);

	str = "byte";
	ret += sprintf(&buffer[ret], "max_pkg_size:\t%u (%s)\n", max_pac_size, str);
	ret += sprintf(&buffer[ret], "skw_sdio_blk_size:\t%u (%s)\n", skw_sdio_blk_size, str);
	ret += sprintf(&buffer[ret], "max_tx_pkg_cnt:\t%u\n", MAX_PAC_COUNT);
	ret += sprintf(&buffer[ret], "max_rx_pkg_cnt:\t%u\n", wifi_pdata.max_buffer_size/max_pac_size);


	ret += sprintf(&buffer[ret], "wifi data_port:\t%u\n", wifi_pdata.data_port);
	ret += sprintf(&buffer[ret], "wifi cmd_port:\t%u\n", wifi_pdata.cmd_port);


	ret += sprintf(&buffer[ret], "ucom data_port:\t%u\n", ucom_pdata.data_port);
	ret += sprintf(&buffer[ret], "ucom cmd_port:\t%u\n", ucom_pdata.cmd_port);
	ret += sprintf(&buffer[ret], "ucom audio_port:\t%u\n", ucom_pdata.audio_port);

	ret += sprintf(&buffer[ret], "max_ch_num:\t%u\n", max_ch_num);

	if(g_chipen_pin>=0){
		switch (gpio_get_value(g_chipen_pin)) {
		case 0:
			str = "low";
			break;
		case 1:
			str = "high";
			break;
		default:
			str = "invalid";
			break;
		}
	}
	ret += sprintf(&buffer[ret], "chipen gpio_out:\t%u (%s)\n", g_chipen_pin, str);

	if(skw_sdio->gpio_out>=0){
		switch (gpio_get_value(skw_sdio->gpio_out)) {
		case 0:
			str = "low";
			break;
		case 1:
			str = "high";
			break;
		default:
			str = "invalid";
			break;
		}
	}
	ret += sprintf(&buffer[ret], "ap2cp gpio_out:\t%u (%s)\n", skw_sdio->gpio_out, str);

	if(skw_sdio->gpio_in>=0){
		switch (gpio_get_value(skw_sdio->gpio_in)) {
		case 0:
			str = "low";
			break;
		case 1:
			str = "high";
			break;
		default:
			str = "invalid";
			break;
		}
	}
	ret += sprintf(&buffer[ret], "cp2ap gpio_in:\t%u (%s)\n", skw_sdio->gpio_in, str);

#if defined(CONFIG_NO_SERVICE_PD) && (CONFIG_NO_SERVICE_PD == 1)
    str = "Enable";
#else
    str = "Disable";
#endif

	ret += sprintf(&buffer[ret], "sleep with cp powerdown:\t(%s)\n",  str);


	switch (skw_use_sdma) {
	case 1:
		str = "sdma";
		break;
	case 0:
		str = "adma";
		break;
	default:
		str = "adma";
		break;
	}
	ret += sprintf(&buffer[ret], "dma type:\t%u (%s)\n", skw_use_sdma, str);


	switch (skw_sdio->boot_data->slp_disable) {
	case 1:
		str = "Disable";
		break;
	case 0:
		str = "Enable";
		break;
	default:
		str = "Enable";
		break;
	}
	ret += sprintf(&buffer[ret], "fw deepsleep:\t%u (%s)\n", skw_sdio->boot_data->slp_disable, str);

	ret += sprintf(&buffer[ret], "host_active:\t%u\n", skw_sdio->host_active);
	ret += sprintf(&buffer[ret], "device_active:\t%u\n", skw_sdio->device_active);
	ret += sprintf(&buffer[ret], "resume_com:\t%u\n", skw_sdio->resume_com);
	ret += sprintf(&buffer[ret], "cp_state:\t%u\n", skw_sdio->cp_state);


	ret += sprintf(&buffer[ret], "VID:\t0x%x\n", skw_sdio->sdio_func[FUNC_0]->vendor);
	ret += sprintf(&buffer[ret], "PID:\t0x%x\n", skw_sdio->sdio_func[FUNC_0]->device);


	ret += sprintf(&buffer[ret], "wifi_device name:\t%s\n", sdio_ports[WIFI_DATA_PORT].pdev->name);


	if(ret >= size)
		skw_sdio_info("ret bigger than size %d %d\n", ret, size);

}


static int skw_sdio_host_check(struct skw_sdio_data_t *skw_sdio)
{
	int ret = 0;

	struct sdio_func *func1 = skw_sdio->sdio_func[FUNC_1];
	struct mmc_host *host = func1->card->host;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 15))
	if ((SKW_SDIO_INBAND_IRQ == skw_sdio->irq_type) && (0 == (host->caps & MMC_CAP_SDIO_IRQ))) {
		skw_sdio_err("Please add cap-sdio-irq to dts! irq_type=%d caps=0x%x\n", skw_sdio->irq_type, host->caps);
		ret = -EPERM;
	} else if ((host->ios.clock > 50000000UL) && (0 == (host->caps & MMC_CAP_UHS_SDR104))) {
		skw_sdio_err("please add sd-uhs-sdr104 to dts! clock=%d cap=0x%x\n", host->ios.clock, host->caps);
		ret = -EPERM;
	} else if ((host->ios.clock <= 50000000UL) && (0 != (host->caps & MMC_CAP_UHS_SDR104))) {
		skw_sdio_err("please remove sd-uhs-sdr104 from dts! clock=%d cap=0x%x\n", host->ios.clock, host->caps);
		ret = -EPERM;
	} else if (host->ios.clock != host->f_max) {
		skw_sdio_err("actual clock is not equal to max clock! clock=%d f_max=%d\n", host->ios.clock, host->f_max);
		ret = -EPERM;
	} else if (host->ios.timing != MMC_TIMING_UHS_SDR104) {
		skw_sdio_err("actual timing is not equal to max timing! timing=%d t_max=%d\n", host->ios.timing, MMC_TIMING_UHS_SDR104);
		ret = -EPERM;
	}
#endif

	return ret;
}

static int __init skw_sdio_io_init(void)
{
	struct skw_sdio_data_t *skw_sdio;
	int ret = 0;
	int skw_sd_id = SKW_MMC_HOST_SD_INDEX;
	if (card_id != SKW_MMC_HOST_SD_INDEX)
		skw_sd_id = card_id;
	skw_sdio_mmc_scan(skw_sd_id);
	memset(&debug_infos, 0, sizeof(struct debug_vars));
	skw_sdio_log_level_init();

	skw_sdio = kzalloc(sizeof(struct skw_sdio_data_t), GFP_KERNEL);
	if (!skw_sdio) {
		WARN_ON(1);
		return -ENOMEM;
	}

	/* card not ready */
	g_skw_sdio_data = skw_sdio;
	mutex_init(&skw_sdio->transfer_mutex);
	mutex_init(&skw_sdio->except_mutex);
	mutex_init(&dloader_mutex);
	mutex_init(&skw_sdio->service_mutex);
	atomic_set(&skw_sdio->resume_flag, 1);
	atomic_set(&skw_sdio->threads_exit, 0);
	skw_sdio->next_size_buf = kzalloc(SKW_BUF_SIZE, GFP_KERNEL);
	if(skw_sdio->next_size_buf == NULL){
		kfree(skw_sdio);
		skw_sdio = NULL;
		return -ENOMEM;
	}
	skw_sdio->eof_buf = kzalloc(SKW_BUF_SIZE, GFP_KERNEL);
	if(skw_sdio->eof_buf == NULL){
		kfree(skw_sdio->next_size_buf);
		skw_sdio->next_size_buf = NULL;
		kfree(skw_sdio);
		skw_sdio = NULL;
		return -ENOMEM;
	}
	atomic_set(&skw_sdio->online, SKW_SDIO_CARD_OFFLINE);
	if(!bind_device){
		skw_sdio->adma_rx_enable = 1;
	}
	//kzmalloc the log_data
	skw_sdio->log_data = kzalloc(sizeof(struct skw_log_data_t), GFP_KERNEL);
	if (skw_sdio->log_data == NULL) {
		kfree(skw_sdio->next_size_buf);
		skw_sdio->next_size_buf = NULL;
		kfree(skw_sdio->eof_buf);
		skw_sdio->eof_buf = NULL;
		kfree(skw_sdio);
		skw_sdio = NULL;
		skw_sdio_err("kzalloc the log_data fail\n");
		return -ENOMEM;
	}
	skw_sdio->irq_num = -1;
	INIT_DELAYED_WORK(&skw_sdio->skw_except_work, skw_sdio_exception_work);
	ret = skw_sdio_scan_card();
	if (ret < 0) {
		skw_sdio_remove_card();
		skw_sdio_err("scan card fail\n");
		kfree(skw_sdio->log_data);
		skw_sdio->log_data = NULL;
		kfree(skw_sdio->next_size_buf);
		skw_sdio->next_size_buf = NULL;
		kfree(skw_sdio->eof_buf);
		skw_sdio->eof_buf = NULL;
		kfree(skw_sdio);
		skw_sdio = NULL;
		return ret;
	}
	if (skw_sdio->sdio_dev_host) {
		seekwave_boot_init((char *)skw_sdio->chip_id);
	}
	return ret;
}

static void __exit  skw_sdio_io_exit(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if (skw_sdio->sdio_dev_host)
		seekwave_boot_exit();
	skw_sdio_debugfs_deinit();
	if (skw_sdio) {
		skw_sdio_stop_thread();
		if (!skw_sdio->suspend_wake_unlock_enable) {
			skw_sdio_unlock_rx_ws(skw_sdio);
		}
		if (skw_sdio->sdio_dev_host)
			skw_sdio_reset_card();

		skw_sdio_remove_card();
		cancel_delayed_work_sync(&skw_sdio->skw_except_work);
		mutex_destroy(&skw_sdio->transfer_mutex);
		mutex_destroy(&skw_sdio->except_mutex);
		mutex_destroy(&dloader_mutex);
		mutex_destroy(&skw_sdio->service_mutex);

		kfree(skw_sdio->next_size_buf);
		kfree(skw_sdio->eof_buf);
		skw_sdio->boot_data = NULL;
		skw_sdio->sdio_dev_host = NULL;
		kfree(skw_sdio->log_data);
		skw_sdio->log_data = NULL;
		kfree(skw_sdio);
		skw_sdio = NULL;
	}
	skw_sdio_info(" OK\n");
}
module_init(skw_sdio_io_init)
module_exit(skw_sdio_io_exit)
MODULE_LICENSE("GPL v2");
