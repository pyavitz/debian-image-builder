#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/skbuff.h>
#include "skw_sdio.h"
#include "skw_sdio_log.h"
#include <linux/workqueue.h>
#if  LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#include <linux/bits.h>
#else
#include <linux/bitops.h>
#endif
#ifndef GENMASK
#ifdef CONFIG_64BIT
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif /* CONFIG_64BIT */
#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif
#define MODEM_ASSERT_TIMEOUT_VALUE  2*HZ
#define MAX_SG_COUNT	100
#define SDIO_BUFFER_SIZE	(16*1024)
#define FRAGSZ_SIZE (3*1024)
#define MAX_FIRMWARE_SIZE 256
#define PORT_STATE_IDLE	0
#define PORT_STATE_OPEN	1
#define PORT_STATE_CLSE	2
#define PORT_STATE_ASST	3

#define CRC_16_L_SEED	0x80
#define CRC_16_L_POLYNOMIAL  0x8000
#define CRC_16_POLYNOMIAL  0x1021

int recovery_debug_status=0;
int wifi_serv_debug_status=0;
int bt_serv_debug_status=0;

/***********************************************************/
char firmware_version[128];
char assert_context[1024];
int  assert_context_size=0;
static int assert_info_print;
static u64 port_dmamask = DMA_BIT_MASK(32);
struct sdio_port sdio_ports[SDIO2_MAX_CH_NUM];
static u8 fifo_ind;
struct debug_vars debug_infos;
static BLOCKING_NOTIFIER_HEAD(modem_notifier_list);
unsigned int crc_16_l_calc(char *buf_ptr,unsigned int len);
static int skw_sdio_rx_port_follow_ctl(int portno, int rx_fctl);
//add the crc api the same as cp crc_16 api
extern void kernel_restart(char *cmd);
static int skw_sdio_irq_ops(int irq_enable);
static int bt_service_start(void);
static int bt_service_stop(void);
static int wifi_service_start(void);
static int wifi_service_stop(void);
void send_cp_wakeup_signal(struct skw_sdio_data_t *skw_sdio);
static int skw_sdio_dump(unsigned int address, void *buf, unsigned int len);
char skw_cp_ver = SKW_SDIO_V10;
int max_ch_num = MAX_CH_NUM;
int max_pac_size = MAX_PAC_SIZE;
int skw_sdio_blk_size = 256;
int cp_detect_sleep_mode;
static u8 is_timeout_kick;
#ifdef CONFIG_PRINTK_TIME_FROM_ARM_ARCH_TIMER
#include <clocksource/arm_arch_timer.h>
u64 skw_local_clock(void)
{
        u64 ns;

        ns = arch_timer_read_counter() * 1000;
        do_div(ns, 24);

        return ns;
}
#else
#if KERNEL_VERSION(4,11,0) <= LINUX_VERSION_CODE
#include <linux/sched/clock.h>
#else
#include <linux/sched.h>
#endif
u64 skw_local_clock(void)
{
        return local_clock();
}
#endif
#define IS_LOG_PORT(portno)  ((skw_cp_ver == SKW_SDIO_V10)?(portno==1):(portno==SDIO2_BSP_LOG_PORT))


void skw_get_assert_print_info(char *buffer, int size)
{
	int ret = 0;
	int j = 0;
	u64 ts;
	u64 rem_nsec;
	u64 rem_usec;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if(!buffer) {
		skw_sdio_info("buffer is null!\n");
		return;
	}
	ret += sprintf(&buffer[ret], "last irq times: [%6d] [%6d] ", debug_infos.rx_inband_irq_cnt, debug_infos.rx_gpio_irq_cnt);
	for (j = 0; j < CHN_IRQ_RECORD_NUM; j++) {
		ts = debug_infos.last_irq_times[j];
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		ret += sprintf(&buffer[ret], "[%5lu.%06llu] ", (unsigned long)ts, rem_usec);
	}
	if (SKW_SDIO_INBAND_IRQ == skw_sdio->irq_type) {
		ret += sprintf(&buffer[ret], "\nlast clear irq times:    [%6d] ", debug_infos.rx_inband_irq_cnt);
		for (j = 0; j < CHN_IRQ_RECORD_NUM; j++) {
			ts = debug_infos.last_clear_irq_times[j];
			rem_nsec = do_div(ts, 1000000000);
			rem_usec = do_div(rem_nsec, 1000);
			ret += sprintf(&buffer[ret], "[%5lu.%06llu] ", (unsigned long)ts, rem_usec);
		}
	}
	ret += sprintf(&buffer[ret], "\nlast rx read times:      [%6d] ", debug_infos.rx_read_cnt);
	for (j = 0; j < CHN_IRQ_RECORD_NUM; j++) {
		ts = debug_infos.last_rx_read_times[j];
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		ret += sprintf(&buffer[ret], "[%5lu.%06llu] ", (unsigned long)ts, rem_usec);
	}

	if(ret >= size)
		skw_sdio_info("ret bigger than size %d %d\n", ret, size);
}

void skw_get_sdio_debug_info(char *buffer, int size)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u64 ts;
	u64 rem_nsec;
	u64 rem_usec;
	u32 irq_cnt = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if(!buffer) {
		skw_sdio_info("buffer is null!\n");
		return;
	}

	if (0 == debug_infos.rx_irq_statistics_cnt) {
		if (SKW_SDIO_EXTERNAL_IRQ == skw_sdio->irq_type)
			debug_infos.rx_irq_statistics_cnt = debug_infos.rx_gpio_irq_cnt;
		else
			debug_infos.rx_irq_statistics_cnt = debug_infos.rx_inband_irq_cnt;
		debug_infos.rx_irq_statistics_time = skw_local_clock();
		skw_sdio_info("===============rx irq statistics start:%d!===============\n", debug_infos.rx_irq_statistics_cnt);
	} else {
		ret += sprintf(&buffer[ret], "rx irq statistics:\n");
		ts = skw_local_clock() - debug_infos.rx_irq_statistics_time;
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		if (SKW_SDIO_EXTERNAL_IRQ == skw_sdio->irq_type)
			irq_cnt = debug_infos.rx_gpio_irq_cnt - debug_infos.rx_irq_statistics_cnt;
		else
			irq_cnt = debug_infos.rx_inband_irq_cnt - debug_infos.rx_irq_statistics_cnt;	
		skw_sdio_info("===============rx irq statistics end:%d!===============\n", debug_infos.rx_irq_statistics_cnt + irq_cnt);
		ret += sprintf(&buffer[ret], "rx irq time: [%5lu.%06llu] count:%d count per second:%lu\n", (unsigned long)ts, rem_usec, irq_cnt, irq_cnt /(unsigned long)ts);

		debug_infos.rx_irq_statistics_cnt = 0;
		debug_infos.rx_irq_statistics_time = 0;
	}

	ret += sprintf(&buffer[ret], "channel irq times:\n");
	for (i = 0; i < max_ch_num; i++) {
		ret += sprintf(&buffer[ret], "channel[%2d]: [%6d] ", i, debug_infos.chn_irq_cnt[i]);
		for (j = 0; j < CHN_IRQ_RECORD_NUM; j++) {
			ts = debug_infos.chn_last_irq_time[i][j];
			rem_nsec = do_div(ts, 1000000000);
			rem_usec = do_div(rem_nsec, 1000);
			ret += sprintf(&buffer[ret], "[%5lu.%06llu] ", (unsigned long)ts, rem_usec);
		}
		ret += sprintf(&buffer[ret], "\n");
	}
	ret += sprintf(&buffer[ret], "cmd_timeout_cnt: %d\n", debug_infos.cmd_timeout_cnt);
	ret += sprintf(&buffer[ret], "last_sent_wifi_cmd[0]: 0x%x\n", debug_infos.last_sent_wifi_cmd[0]);
	ret += sprintf(&buffer[ret], "last_sent_wifi_cmd[1]: 0x%x\n", debug_infos.last_sent_wifi_cmd[1]);
	ret += sprintf(&buffer[ret], "last_sent_wifi_cmd[2]: 0x%x\n", debug_infos.last_sent_wifi_cmd[2]);
	ts = debug_infos.last_sent_time;
	rem_nsec = do_div(ts, 1000000000);
	rem_usec = do_div(rem_nsec, 1000);
	ret += sprintf(&buffer[ret], "last_sent_time: [%5lu.%06llu]\n", (unsigned long)ts, rem_usec);
	ts = debug_infos.last_rx_submit_time;
	rem_nsec = do_div(ts, 1000000000);
	rem_usec = do_div(rem_nsec, 1000);
	ret += sprintf(&buffer[ret], "last_rx_submit_time: [%5lu.%06llu]\n", (unsigned long)ts, rem_usec);
	if (debug_infos.host_assert_cp_time) {
		ts = debug_infos.host_assert_cp_time;
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		ret += sprintf(&buffer[ret], "host_assert_cp_time: [%5lu.%06llu]\n", (unsigned long)ts, rem_usec);
	}
	if (debug_infos.cp_assert_time) {
		ts = debug_infos.cp_assert_time;
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		ret += sprintf(&buffer[ret], "cp_assert_time: [%5lu.%06llu]\n", (unsigned long)ts, rem_usec);
	}
	if (debug_infos.host_assert_cp_time > debug_infos.last_sent_time) {
		ts = debug_infos.host_assert_cp_time - debug_infos.last_sent_time;
		rem_nsec = do_div(ts, 1000000000);
		rem_usec = do_div(rem_nsec, 1000);
		ret += sprintf(&buffer[ret], "timeout: [%5lu.%06llu]\n", (unsigned long)ts, rem_usec);
	}

	if(ret >= size)
		skw_sdio_info("ret bigger than size %d %d\n", ret, size);
}
//=======================================================
//debug sdio macro and Variable
int glb_wifiready_done;
//#define SKW_WIFIONLY_DEBUG 1
//=======================================================

/********************************************************
 * skw_sdio_update img crc checksum
 * For update the CP IMG
 *Author: JUNWEI JIANG
 *Date:2022-08-11
 * *****************************************************/

int skw_log_port(void)
{
	return (skw_cp_ver == SKW_SDIO_V10)?(BSP_LOG_PORT):(SDIO2_BSP_LOG_PORT);
}

void skw_get_port_statistic(char *buffer, int size)
{
	int ret = 0;
	int i;

	if(!buffer)
		return;
	ret += sprintf(&buffer[ret], "%s", firmware_version);
	for(i=0; i<max_ch_num; i++) {
		if(ret >= size)
			break;
		if(sdio_ports[i].state)
			ret += sprintf(&buffer[ret], "port%d: rx %d %d, tx %d %d\n",
				i, sdio_ports[i].rx_count, sdio_ports[i].rx_packet,
				sdio_ports[i].total, sdio_ports[i].sent_packet);
	}
}

unsigned int crc_16_l_calc(char *buf_ptr,unsigned int len)
{
	unsigned int i;
	unsigned short crc=0;

	while(len--!=0)
	{
		for(i= CRC_16_L_SEED;i!=0;i=i>>1)
		{
			if((crc &CRC_16_L_POLYNOMIAL)!=0)
			{
				crc= crc<<1;
				crc= crc ^ CRC_16_POLYNOMIAL;
			}else{
				crc = crc <<1;
			}

			if((*buf_ptr &i)!=0)
			{
				crc = crc ^ CRC_16_POLYNOMIAL;
			}
		}
		buf_ptr++;
	}
	return (crc);
}

static int skw_sdio_rx_port_follow_ctl(int portno, int rx_fctl)
{
	char ftl_val = 0;
	int ret = 0;

	skw_sdio_info(" portno:%d, rx_fctl:%d \n", portno, rx_fctl);

	if((portno < 0) || (portno > max_ch_num))
		return -1;

	if(portno < 8){
		ret = skw_sdio_readb(SKW_SDIO_RX_CHANNEL_FTL0, &ftl_val);
		if(ret)
			return -1;

		if(rx_fctl)
			ftl_val = ftl_val | (1 << portno);
		else
			ftl_val = ftl_val & (~(1 << portno));
		ret = skw_sdio_writeb(SKW_SDIO_RX_CHANNEL_FTL0, ftl_val);
	}
	else{
		portno = portno - 8;
		ret = skw_sdio_readb(SKW_SDIO_RX_CHANNEL_FTL1, &ftl_val);
		if(ret)
			return -1;

		if(rx_fctl)
			ftl_val = ftl_val | (1 << portno);
		else
			ftl_val = ftl_val & (~(1 << portno));
		ret = skw_sdio_writeb(SKW_SDIO_RX_CHANNEL_FTL1, ftl_val);
	}
	return ret;
}

void modem_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&modem_notifier_list, nb);
}
void modem_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&modem_notifier_list, nb);
}
void modem_notify_event(int event)
{
	blocking_notifier_call_chain(&modem_notifier_list, event, NULL);
}

void skw_sdio_exception_work(struct work_struct *work)
{
	int i=0;
	int port_num=5;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info(" entern..cp_state = %d.\n", skw_sdio->cp_state);
	mutex_lock(&skw_sdio->except_mutex);
	if(skw_sdio->cp_state!=1)
	{
		skw_sdio_warn("cp assert already\n");
		mutex_unlock(&skw_sdio->except_mutex);
		return;
	}
	skw_sdio->cp_state = DEVICE_BLOCKED_EVENT;
	mutex_unlock(&skw_sdio->except_mutex);
	modem_notify_event(DEVICE_BLOCKED_EVENT);
	for (i=0; i<port_num; i++)
	{
		if(!sdio_ports[i].state || sdio_ports[i].state==PORT_STATE_CLSE)
			continue;

		sdio_ports[i].state = PORT_STATE_ASST;
		complete(&(sdio_ports[i].rx_done));

		if(i!=1)
			complete(&(sdio_ports[i].tx_done));
		if(i==0 || i==skw_log_port())
			sdio_ports[i].next_seqno= 1;

	}
	skw_sdio->service_state_map=0;
	skw_recovery_mode();
}

static inline void *skw_sdio_alloc_frag(size_t fragsz, gfp_t gfp_mask)
{
	void *addr;
	struct page *page;
	addr = netdev_alloc_frag(fragsz);
	if (!addr)
		return NULL;

	page = virt_to_head_page(addr);

	skw_sdio_dbg(
		"dbg: alloc addr: 0x%lx, size: %ld, page addr: 0x%lx, ref: %d\n",
		(long)addr, (long int)fragsz, (long)page, page_count(page));

	return addr;
}
static inline void skw_page_frag_free(void *addr)
{
	struct page *page = NULL;
	if (!addr) {
		skw_sdio_warn("dbg: free addr is NULL\n");
		return;
	}

	page = virt_to_head_page(addr);
	skw_sdio_dbg("dbg: free addr: 0x%lx, page addr: 0x%lx, ref: %d\n",
		     (long)addr, (long)page, page_count(page));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	skb_free_frag(addr);
#else
	put_page(virt_to_head_page(addr));
#endif
}

static void skw_sdio_rx_down(struct skw_sdio_data_t * skw_sdio)
{
	wait_for_completion_interruptible(&skw_sdio->rx_completed);
}
void skw_sdio_rx_up(struct skw_sdio_data_t * skw_sdio)
{
	skw_reinit_completion(skw_sdio->rx_completed);
	complete(&skw_sdio->rx_completed);
}
void skw_sdio_dispatch_packets(struct skw_sdio_data_t * skw_sdio)
{
	int i;
	struct sdio_port *port;

	for(i=0; i<max_ch_num; i++) {
		port = &sdio_ports[i];
		if(!port->state)
			continue;
		if(port->rx_rp!=port->rx_wp)
			skw_sdio_dbg("port[%d] sg_index=%d (%d,%d)\n", i,
				port->sg_index, port->rx_rp, port->rx_wp);
		if(port->rx_submit && port->sg_index) {
			debug_infos.last_rx_submit_time = skw_local_clock();
			port->rx_submit(port->channel, port->sg_rx, port->sg_index, port->rx_data);
			skw_sdio_dbg("port[%d] sg_index=%d (%d,%d)\n", i,
				port->sg_index, port->rx_rp, port->rx_wp);
			port->sg_index = 0;
		}
	}
}
static void skw_sdio_sdma_set_nsize(unsigned int size)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int count;
	if (size == 0) {
		skw_sdio->next_size = max_pac_size;
		return;
	}

	count = (size >> 10) + 9;
	size = SKW_SDIO_ALIGN_BLK(size + (count<<3));
	skw_sdio->next_size = (size>SDIO_BUFFER_SIZE) ? SDIO_BUFFER_SIZE:size;
}

static void skw_sdio_adma_set_packet_num(unsigned int num)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if (num == 0)
		num = 1;

	if (num >= MAX_SG_COUNT)
		skw_sdio->remain_packet = MAX_SG_COUNT;
	else 
		skw_sdio->remain_packet = num;
}

/************************************************************************
 *Decription:release debug recovery auto test
 *Author:junwei.jiang
 *Date:2023-05-30
 *Modfiy:
 *
 ********************************************************************* */
int skw_sdio_recovery_debug(int disable)
{
	recovery_debug_status = disable;
	skw_sdio_info("the recovery status =%d\n", recovery_debug_status);
	return 0;
}

int skw_sdio_recovery_debug_status(void)
{
	skw_sdio_info("the recovery val =%d\n", recovery_debug_status);
	return recovery_debug_status;
}

int skw_sdio_bt_serv_debug(int enable)
{
	bt_serv_debug_status = enable;

	skw_sdio_info("the bt_service status =%d\n", bt_serv_debug_status);
	if(enable){
		bt_service_start();
	}else{
		bt_service_stop();
	}
	return 0;
}

int skw_sdio_bt_serv_debug_status(void)
{
	skw_sdio_info("the bt_service val =%d\n", bt_serv_debug_status);

	return bt_serv_debug_status;
}
int skw_sdio_wifi_serv_debug(int enable)
{
	wifi_serv_debug_status = enable;

	skw_sdio_info("the wifi_service status =%d\n", wifi_serv_debug_status);
	if(enable){
		wifi_service_start();
	}else{
		wifi_service_stop();
	}
	return 0;
}

int skw_sdio_wifi_serv_debug_status(void)
{
	skw_sdio_info("the wifi_service val =%d\n", wifi_serv_debug_status);
	return wifi_serv_debug_status;
}

static int force_cp_wakeup(void)
{
	int ret = 0;
	u32 val = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	send_cp_wakeup_signal(skw_sdio);
	ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
	if (ret <= 0) {
		skw_sdio_err("wait for device wakeup timeout\n");
		return -ETIMEDOUT;
	}
	val = gpio_get_value(skw_sdio->gpio_in);
	if(val)
		return 0;
	else {
		send_cp_wakeup_signal(skw_sdio);
		ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
		if (ret == 0)
			return -ETIMEDOUT;
		val = gpio_get_value(skw_sdio->gpio_in);
		if(val)
			return 0;
		else
			return -ETIMEDOUT;
	}

	return -ETIMEDOUT;
}

static int skw_sdio_dump(unsigned int address, void *buf, unsigned int len)
{
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	ret = skw_sdio_smem_poweron();
	if (ret != 0) {
		skw_sdio_err(" pweron fail ret:%d, addr=0x%x\n", ret, address);
		return ret;
	}
	send_cp_wakeup_signal(skw_sdio);
	ret = skw_sdio_dt_read(address, buf, len);
	if (ret != 0) {
		skw_sdio_err(" dt read fail ret:%d, addr=0x%x\n", ret, address);
		return ret;
	}
	return ret;
}
int update_download_flag(bool enable)
{
	int ret;
	u32 buffer;

	/* 1. read DL_FLAG reg */
	ret = force_cp_wakeup();
	if (ret) {
		skw_sdio_err("force cp wakeup failed %d\n", __LINE__);
		return ret;
	}
	ret = skw_sdio_dt_read(SKW_DL_FLAG_BASE, &buffer, 4);
	if (ret) {
		ret = force_cp_wakeup();
		if (ret) {
			skw_sdio_err("force cp wakeup failed %d\n", __LINE__);
			return ret;
		}
		ret = skw_sdio_dt_read(SKW_DL_FLAG_BASE, &buffer, 4);
		if (ret) {
			skw_sdio_err("read dl flag failed\n");
			return ret;
		}
	}

	if (enable == 1)
		buffer |= SKW_DL_FLAG_BIT_MASK;
	else
		buffer &= ~SKW_DL_FLAG_BIT_MASK;

	/* 2. update DL_FLAG bit */
	ret = force_cp_wakeup();
	if (ret) {
		skw_sdio_err("force cp wakeup failed %d\n", __LINE__);
		return ret;
	}
	ret = skw_sdio_dt_write(SKW_DL_FLAG_BASE, &buffer, 4);
	if (ret) {
		ret = force_cp_wakeup();
		if (ret) {
			skw_sdio_err("force cp wakeup failed %d\n", __LINE__);
			return ret;
		}
		ret = skw_sdio_dt_write(SKW_DL_FLAG_BASE, &buffer, 4);
		if (ret) {
			skw_sdio_err("write dl flag failed\n");
			return ret;
		}
	}
	/* 3. Make sure CP update flag successfully */
	ret = force_cp_wakeup();
	if (ret) {
		skw_sdio_err("force cp wakeup failed %d\n", __LINE__);
		return ret;
	}

	return 0;
}

static int skw_pin_config(void)
{
	int i, ret;
	u32 val;
	int func_index = 0;
	int g_offset = 0;
	u32 pin_group_index = 0;
	u32 func_group_index = 0;
	u32 func_sel_val[2] = {0};
	u32 func_sel_off[] = {PN_FUNC_SEL0_OFFSET, PN_FUNC_SEL1_OFFSET};
	u32 *pin_val;
	u8 pin_off[PN_CNT] = {0};
	u32 buffer;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();


	ret = update_download_flag(1);
	if (ret) {
		skw_sdio_err("set download flag failed\n");
		return ret;
	}

	pin_val = (u32 *)kzalloc(PN_CNT * sizeof(u32), GFP_KERNEL);
	if (!pin_val) {
		skw_sdio_err("pin_val alloc failed\n");
		return -ENOMEM;
	}

	while (g_offset < skw_sdio->boot_data->nv_mem_pnfg_size) {
		//1. pin offset
		pin_off[pin_group_index] = skw_sdio->boot_data->nv_mem_pnfg_data[g_offset];

		//2. func_sel value
		val = skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 1];
		func_sel_val[func_group_index] |= (val << (3 * func_index)) & (7 << (3 * func_index));

		//3. pin config value
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 2] << BIT_PN_DSLP_EN_START) & \
				GENMASK(BIT_PN_DSLP_EN_END, BIT_PN_DSLP_EN_START);//dslp_en
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 3] << BIT_DRV_STREN_START) & \
				GENMASK(BIT_DRV_STREN_END, BIT_DRV_STREN_START);//drv_strength
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 4] << BIT_NORMAL_WP_START) & \
				GENMASK(BIT_NORMAL_WP_END, BIT_NORMAL_WP_START);//normal:wpu/wpd
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 5] << BIT_SCHMITT_START) & \
				GENMASK(BIT_SCHMITT_END, BIT_SCHMITT_START);//schmitt trigger enable
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 6] << BIT_SLP_WP_START) & \
				GENMASK(BIT_SLP_WP_END, BIT_SLP_WP_START);//sleep:wpu/wpd
		pin_val[pin_group_index] |=
				((u32)skw_sdio->boot_data->nv_mem_pnfg_data[g_offset + 7]) & \
				GENMASK(BIT_SLEEP_IE_OE_END, BIT_SLEEP_IE_OE_START);//sleep:ie/oe

		g_offset += 8; // 1(offset) + 1(pin sel) + 6(pin config)
		func_index++;
		pin_group_index++;
		if (func_index == PN_FUNCSEL_ONEGRP_CNT){
			func_group_index++;
			func_index = 0;
		}
	}

	//function select
	for (i = 0;i < sizeof(func_sel_off) / sizeof(u32);i++) {
		buffer = func_sel_val[i];
		skw_sdio_dbg("sel_off,sel_val::0x%08x,0x%08x\n", func_sel_off[i], buffer);
		ret = skw_sdio_dt_write(SKW_PINREG_BASE + func_sel_off[i], &buffer, 4);
		if (ret) {
			kfree(pin_val);
			pin_val = NULL;
			skw_sdio_err("write pinreg[0x%x] failed\n", func_sel_off[i]);
			return ret;
		}
	}

	//pin config
	for (i = 0;i < PN_CNT;i++) {
		buffer = pin_val[i];
		skw_sdio_dbg("pin_off,pin_val:0x%08x,0x%08x\n", pin_off[i], buffer);
		ret = skw_sdio_dt_write(SKW_PINREG_BASE + pin_off[i], &buffer, 4);
		if (ret) {
			kfree(pin_val);
			pin_val = NULL;
			skw_sdio_err("write pinreg[0x%x] failed\n", pin_off[i]);
			return ret;
		}
	}
	kfree(pin_val);
	pin_val = NULL;
	ret = update_download_flag(0);
	if (ret) {
		skw_sdio_err("clear download flag failed\n");
		return ret;
	}
	return 0;
}

static int skw_sdio_handle_packet(struct skw_sdio_data_t *skw_sdio,
		struct scatterlist *sg, struct skw_packet_header *header, int portno)
{
	struct sdio_port  *port;
	int buf_size, i, ret;
	int log_port_num = 0;
	char *addr;
	u32 *data;
	if (portno >= max_ch_num)
		return -EINVAL;
	log_port_num = skw_log_port();
	port = &sdio_ports[portno];
	port->rx_packet++;
	port->rx_count += header->len;
	if(portno == LOOPCHECK_PORT) {
		char *cmd = (char *)(header+4);
		cmd[header->len - 12] = 0;
		skw_sdio_info("LOOPCHECK channel received: %s\n", (char *)cmd);
		if (header->len==19 && !strncmp(cmd, "BTREADY", 7)) {
			skw_sdio->service_state_map |= 2;
			//kernel_restart(0);
			skw_sdio->device_active = 1;
			complete(&skw_sdio->download_done);
		}else if(header->len==21 && !strncmp(cmd, "WIFIREADY", 9)){
			skw_sdio->service_state_map |= 1;
			//kernel_restart(0);
			skw_sdio->device_active = 1;
			complete(&skw_sdio->download_done);
		} else if (!strncmp((char *)cmd, "BSPASSERT", 9)){
			sprintf(firmware_version, "%s:%s\n", firmware_version, cmd);
			debug_infos.cp_assert_time = skw_local_clock();
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
			if(!skw_sdio->cp_state && sdio_ports[log_port_num].state != PORT_STATE_OPEN)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(800));
			else if(!skw_sdio->cp_state && sdio_ports[log_port_num].state == PORT_STATE_OPEN)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(8000));
#else
			if(!skw_sdio->cp_state)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(12000));
#endif
			mutex_lock(&skw_sdio->except_mutex);
			if(skw_sdio->cp_state==DEVICE_BLOCKED_EVENT){
				if(skw_sdio->adma_rx_enable)
					skw_page_frag_free(header);

				mutex_unlock(&skw_sdio->except_mutex);
				return 0;
			}
			skw_sdio->cp_state=1;/*cp except set value*/
			mutex_unlock(&skw_sdio->except_mutex);
			skw_sdio->service_state_map = 0;
			memset(assert_context, 0, 1024);
			assert_context_size = 0;
			modem_notify_event(DEVICE_ASSERT_EVENT);
			skw_sdio_err(" bsp assert !!!\n");
		}else if (header->len==20 && !strncmp(cmd, "DUMPDONE",8)){
			mutex_lock(&skw_sdio->except_mutex);
			if(skw_sdio->cp_state==DEVICE_BLOCKED_EVENT){
				if(skw_sdio->adma_rx_enable)
					skw_page_frag_free(header);

				mutex_unlock(&skw_sdio->except_mutex);
				return 0;
			}
			skw_sdio->cp_state=DEVICE_DUMPDONE_EVENT;/*cp except set value 2*/
			mutex_unlock(&skw_sdio->except_mutex);
			cancel_delayed_work_sync(&skw_sdio->skw_except_work);
			if(sdio_ports[log_port_num].state == PORT_STATE_OPEN)
				modem_notify_event(DEVICE_DUMPDONE_EVENT);
			skw_sdio_err("The CP DUMPDONE OK : \n %d::%s\n",assert_context_size, assert_context);
			for (i=0; i<5; i++) {
				if(!sdio_ports[i].state || sdio_ports[i].state==PORT_STATE_CLSE)
					continue;

				sdio_ports[i].state = PORT_STATE_ASST;
				complete(&(sdio_ports[i].rx_done));
				if(i!=1)
					complete(&(sdio_ports[i].tx_done));
				if(i==0 || i==skw_log_port())
					sdio_ports[i].next_seqno= 1;
			}
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
			skw_recovery_mode();//recoverymode open api
#else
			if(!strncmp((char *)skw_sdio->chip_id,"SV6160",6) && !recovery_debug_status
					&&skw_sdio->cp_state !=DEVICE_BLOCKED_EVENT){
				skw_recovery_mode();//recoverymode open api
			}
#endif
		}else if (!strncmp("trunk_W", cmd, 7)) {
			memset(firmware_version, 0 , sizeof(firmware_version));
			if (strlen(cmd) > strlen(firmware_version)) {
				strncpy(firmware_version, cmd, sizeof(firmware_version)-1);
				firmware_version[sizeof(firmware_version)-1] = '\0';
			} else {
				strncpy(firmware_version, cmd, strlen(cmd));
				//strcpy(firmware_version, cmd);
			}
			cmd = strstr(firmware_version, "slp=");
			if (cmd)
				cp_detect_sleep_mode = cmd[4] - 0x30;
			else
				cp_detect_sleep_mode = 4;

			if(!skw_sdio->boot_data->first_dl_flag){
				skw_sdio_gpio_irq_pre_ops();
			}
			if(!skw_sdio->cp_state)
				complete(&skw_sdio->download_done);
			if(skw_sdio->cp_state){
				assert_info_print = 0;
				if(sdio_ports[0].state == PORT_STATE_ASST)
					sdio_ports[0].state = PORT_STATE_OPEN;
				modem_notify_event(DEVICE_BSPREADY_EVENT);
				skw_sdio_info("send the bsp state to log service or others\n");
			}
			skw_sdio->cp_state = 0;
			skw_sdio->log_data->smem_poweron = 0;
			wake_up(&skw_sdio->wq);
			skw_sdio_info("cp_state = %d \n", skw_sdio->cp_state);
			//skw_sdio_info("firmware version: %s:%s \n",cmd, firmware_version);
			if (skw_sdio->boot_data->nv_mem_pnfg_data != NULL && skw_sdio->boot_data->nv_mem_pnfg_size != 0) {
				skw_sdio_info("UPDATE '%s' PINCFG from %s\n", (char *)skw_sdio->chip_id, skw_sdio->boot_data->skw_nv_name);
				ret = skw_pin_config();
				if (ret)
					skw_sdio_err("Update pin config failed!!!\n");
			}
		} else if (!strncmp(cmd, "BSPREADY",8)) {
			loopcheck_send_data("RDVERSION", 9);
		}
		skw_sdio_dbg("Line:%d the port=%d \n", __LINE__, port->channel);
		if(skw_sdio->adma_rx_enable)
			skw_page_frag_free(header);
		return 0;
	}
	if(!port->state) {
		if(skw_sdio->adma_rx_enable){
			if (!IS_LOG_PORT(portno))
				skw_sdio_err("port%d discard data for wrong state\n", portno);
			skw_page_frag_free(header);
			return 0;
		}
	}
	if (port->sg_rx && port->rx_data){
		if (port->sg_index >= MAX_SG_COUNT){
			skw_sdio_err(" rx sg_buffer is overflow!\n");
		}else{
			sg_set_buf(&port->sg_rx[port->sg_index++], header, header->len+4);
		}
	}else {
		int packet=0, total=0;
		mutex_lock(&port->rx_mutex);
		buf_size = (port->length + port->rx_wp - port->rx_rp)%port->length;
		buf_size = port->length - 1 - buf_size;
		addr = (char *)(header+1);
		data = (u32 *) addr;
		if(((data[2] & 0xffff) != port->next_seqno) &&
			(header->len > 12) && !IS_LOG_PORT(portno)) {
			skw_sdio_err("portno:%d, packet lost recv seqno=%d expected %d\n", port->channel,
					data[2] & 0xffff, port->next_seqno);
			if(skw_sdio->adma_rx_enable)
				skw_page_frag_free(header);
			mutex_unlock(&port->rx_mutex);
			return 0;
		}
		if(header->len > 12) {
			port->next_seqno++;
			addr += 12;
			header->len -= 12;
			total = data[1] >> 8;
			packet = data[2] & 0xFFFF;
		} else if (header->len == 12) {
			header->len = 0;
			port->tx_flow_ctrl--;
			complete(&port->tx_done);
			skw_port_log(portno,"%s link msg: 0x%x 0x%x port%d: %d \n", __func__,
					data[0], data[1], portno, port->tx_flow_ctrl);
		}
		if(skw_sdio->cp_state){
			if(header->len!=245 || buf_size < 2048) {
				if(assert_info_print++ < 28 && strncmp((const char *)addr, "+LOG", 4)) {
					if (assert_context_size + header->len < sizeof(assert_context)) {
						memcpy(assert_context + assert_context_size, addr, header->len);
						assert_context_size += header->len;
					}
				}
			}
			if(buf_size <2048)
				msleep(10);
		}
		if (port->rx_submit && !port->sg_rx) {
			if (header->len && port->pdev)
				port->rx_submit(portno, port->rx_data, header->len, addr);
		} else if (buf_size < header->len) {
			skw_port_log(portno,"%s port%d overflow:buf_size %d-%d, packet size %d (w,r)=(%d, %d)\n",
					__func__, portno, buf_size, port->length, header->len,
					port->rx_wp,  port->rx_rp);
		} else if(port->state && header->len) {
			if(port->length - port->rx_wp > header->len){
				memcpy(&port->read_buffer[port->rx_wp], addr, header->len);
				port->rx_wp += header->len;
			} else {
				memcpy(&port->read_buffer[port->rx_wp], addr, port->length - port->rx_wp);
				memcpy(&port->read_buffer[0], &addr[port->length - port->rx_wp],
						header->len - port->length + port->rx_wp);
				port->rx_wp = header->len - port->length + port->rx_wp;
			}

			if(!port->rx_flow_ctrl && buf_size-header->len < (port->length/3)) {
				port->rx_flow_ctrl = 1;
				skw_sdio_rx_port_follow_ctl(portno, port->rx_flow_ctrl);
			}
			mutex_unlock(&port->rx_mutex);
			complete(&port->rx_done);
			if(skw_sdio->adma_rx_enable)
				skw_page_frag_free(header);
			return 0;
		}
		mutex_unlock(&port->rx_mutex);
		if(skw_sdio->adma_rx_enable)
			skw_page_frag_free(header);
	}
	return 0;
}

static int skw_sdio2_handle_packet(struct skw_sdio_data_t *skw_sdio,
		struct scatterlist *sg, struct skw_packet2_header *header, int portno)
{
	struct sdio_port  *port;
	int buf_size, i, ret;
	int log_port_num = 0;
	char *addr;
	u32 *data;

	if (portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	log_port_num = skw_log_port();
	port->rx_packet++;
	port->rx_count += header->len;
	if(portno == SDIO2_LOOPCHECK_PORT) {
		char *cmd = (char *)(header+4);
		cmd[header->len - 12] = 0;
		skw_sdio_info("LOOPCHECK channel received: %s\n", (char *)cmd);
		if (header->len==19 && !strncmp(cmd, "BTREADY", 7)) {
			skw_sdio->service_state_map |= 2;
			//kernel_restart(0);
			skw_sdio->device_active = 1;
			complete(&skw_sdio->download_done);
		}else if(header->len==21 && !strncmp(cmd, "WIFIREADY", 9)){
			skw_sdio->service_state_map |= 1;
			//kernel_restart(0);
			skw_sdio->device_active = 1;
			complete(&skw_sdio->download_done);
		} else if (!strncmp((char *)cmd, "BSPASSERT", 9)) {
			sprintf(firmware_version, "%s:%s\n", firmware_version, cmd);
			debug_infos.cp_assert_time = skw_local_clock();
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
			if(!skw_sdio->cp_state && sdio_ports[log_port_num].state != PORT_STATE_OPEN)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(800));
			else if(!skw_sdio->cp_state && sdio_ports[log_port_num].state == PORT_STATE_OPEN)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(8000));
#else
			if(!skw_sdio->cp_state)
				schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(12000));
#endif
			mutex_lock(&skw_sdio->except_mutex);
			if(skw_sdio->cp_state==DEVICE_BLOCKED_EVENT){
				if (skw_sdio->adma_rx_enable) {
					skw_page_frag_free(header);
				}

				mutex_unlock(&skw_sdio->except_mutex);
				return 0;
			}
			skw_sdio->cp_state=1;/*cp except set value*/
			mutex_unlock(&skw_sdio->except_mutex);
			skw_sdio->service_state_map = 0;
			memset(assert_context, 0, 1024);
			assert_context_size = 0;
			modem_notify_event(DEVICE_ASSERT_EVENT);
			skw_sdio_err(" bsp assert !!!\n");
		}else if (header->len==20 && !strncmp(cmd, "DUMPDONE",8)){
			mutex_lock(&skw_sdio->except_mutex);
			if(skw_sdio->cp_state==DEVICE_BLOCKED_EVENT){
				if (skw_sdio->adma_rx_enable) {
					skw_page_frag_free(header);
				}

				mutex_unlock(&skw_sdio->except_mutex);
				return 0;
			}
			skw_sdio->cp_state=DEVICE_DUMPDONE_EVENT;/*cp except set value 2*/
			mutex_unlock(&skw_sdio->except_mutex);
			cancel_delayed_work_sync(&skw_sdio->skw_except_work);
			if(sdio_ports[log_port_num].state == PORT_STATE_OPEN)
				modem_notify_event(DEVICE_DUMPDONE_EVENT);
			skw_sdio_err("The CP DUMPDONE OK : \n %d::%s\n",assert_context_size, assert_context);
			for (i=0; i<5; i++) {
				if(!sdio_ports[i].state || sdio_ports[i].state==PORT_STATE_CLSE)
					continue;

				sdio_ports[i].state = PORT_STATE_ASST;
				complete(&(sdio_ports[i].rx_done));
				if(i!=1)
					complete(&(sdio_ports[i].tx_done));
				if(i==1)
					sdio_ports[i].next_seqno= 1;
			}
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
			skw_recovery_mode();//recoverymode open api
#else
			if(!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",12) && !recovery_debug_status
					&&skw_sdio->cp_state !=DEVICE_BLOCKED_EVENT){
				skw_recovery_mode();//recoverymode open api
			}
#endif

		}else if (!strncmp("trunk_W", cmd, 7)) {
			memset(firmware_version, 0 , sizeof(firmware_version));
			if (strlen(cmd) > strlen(firmware_version)) {
				strncpy(firmware_version, cmd, sizeof(firmware_version)-1);
				firmware_version[sizeof(firmware_version)-1] = '\0';
			} else {
				strncpy(firmware_version, cmd, strlen(cmd));
				//strcpy(firmware_version, cmd);
			}
			cmd = strstr(firmware_version, "slp=");
			if (cmd)
				cp_detect_sleep_mode = cmd[4] - 0x30;
			else
				cp_detect_sleep_mode = 4;

			if(!skw_sdio->boot_data->first_dl_flag){
				skw_sdio_gpio_irq_pre_ops();
			}
			if(!skw_sdio->cp_state)
				complete(&skw_sdio->download_done);

			if(skw_sdio->cp_state){
				assert_info_print = 0;
				if(sdio_ports[0].state == PORT_STATE_ASST)
					sdio_ports[0].state = PORT_STATE_OPEN;
				modem_notify_event(DEVICE_BSPREADY_EVENT);
			}
			skw_sdio->cp_state = 0;
			skw_sdio->log_data->smem_poweron = 0;
			if (skw_sdio->boot_data->nv_mem_pnfg_data != NULL && skw_sdio->boot_data->nv_mem_pnfg_size != 0) {
				skw_sdio_info("UPDATE '%s' PINCFG from %s\n", (char *)skw_sdio->chip_id, skw_sdio->boot_data->skw_nv_name);
				ret = skw_pin_config();
				if (ret)
					skw_sdio_err("Update pin config failed!!!\n");
			}
		} else if (!strncmp(cmd, "BSPREADY",8)) {
			loopcheck_send_data("RDVERSION", 9);
		}
		skw_sdio_dbg("Line:%d the port=%d \n", __LINE__, port->channel);
		if (skw_sdio->adma_rx_enable) {
			skw_page_frag_free(header);
		}
		return 0;
	}
	//skw_sdio_info("Line:%d the port=%d \n", __LINE__, port->channel);
	if(!port->state) {
		if(skw_sdio->adma_rx_enable){
			if (!IS_LOG_PORT(portno))
				skw_sdio_err(
					"port%d discard data for wrong state\n",
					portno);
			skw_page_frag_free(header);
			return 0;
		}
	}
	if (port->sg_rx){
		if (port->sg_index >= MAX_SG_COUNT){
			skw_sdio_err(" rx sg_buffer is overflow!\n");
		}else{
			sg_set_buf(&port->sg_rx[port->sg_index++], header, header->len+4);
		}
	}else {
		int packet=0, total=0;
		mutex_lock(&port->rx_mutex);
		buf_size = (port->length + port->rx_wp - port->rx_rp)%port->length;
		buf_size = port->length - 1 - buf_size;
		addr = (char *)(header+1);
		data = (u32 *) addr;
		if(((data[2] & 0xffff) != port->next_seqno) && header->len > 12) {
			skw_sdio_err("portno:%d, packet lost recv seqno=%d expected %d\n", port->channel,
					data[2] & 0xffff, port->next_seqno);
			port->next_seqno = (data[2] & 0xffff);
		}
		if(header->len > 12) {
			port->next_seqno++;
			addr += 12;
			header->len -= 12;
			total = data[1] >> 8;
			packet = data[2] & 0xFFFF;
		} else if (header->len == 12) {
			header->len = 0;
			port->tx_flow_ctrl--;
			skw_port_log(portno,"%s link msg: 0x%x 0x%x 0x%x: %d\n", __func__,
					data[0], data[1], data[2], port->tx_flow_ctrl);
			complete(&port->tx_done);
		}
		if (skw_sdio->cp_state && !IS_LOG_PORT(portno)) {
			if (header->len != 245 || buf_size < 2048)
				skw_sdio_info("(%d.%d) (%d,%d) len=%d : 0x%x\n",
					      portno, port->next_seqno,
					      port->rx_wp, port->rx_rp,
					      header->len, data[3]);
			if (buf_size < 2048)
				msleep(10);
		}
		if (port->rx_submit && !port->sg_rx) {
#ifdef CONFIG_BT_SEEKWAVE
			if (header->len)
#else
			if (header->len && port->pdev)
#endif
				port->rx_submit(portno, port->rx_data, header->len, addr);
		} else 	if (buf_size < header->len) {
			skw_port_log(portno,"%s port%d overflow:buf_size %d-%d, packet size %d (w,r)=(%d, %d)\n",
					__func__, portno, buf_size, port->length, header->len,
					port->rx_wp,  port->rx_rp);
		} else if(port->state && header->len) {
			if (port->read_buffer != NULL) {
				if(port->length - port->rx_wp > header->len){
					memcpy(&port->read_buffer[port->rx_wp], addr, header->len);
					port->rx_wp += header->len;
				} else {
					memcpy(&port->read_buffer[port->rx_wp], addr, port->length - port->rx_wp);
					memcpy(&port->read_buffer[0], &addr[port->length - port->rx_wp],
							header->len - port->length + port->rx_wp);
					port->rx_wp = header->len - port->length + port->rx_wp;
				}
			}

			if(!port->rx_flow_ctrl && buf_size-header->len < (port->length/3)) {
				port->rx_flow_ctrl = 1;
				skw_sdio_rx_port_follow_ctl(portno, port->rx_flow_ctrl);
			}
			mutex_unlock(&port->rx_mutex);
			complete(&port->rx_done);
			if(skw_sdio->adma_rx_enable){
				skw_page_frag_free(header);
			}
			return 0;
		}
		mutex_unlock(&port->rx_mutex);
		if (skw_sdio->adma_rx_enable) {
			skw_page_frag_free(header);
		}
	}
	return 0;
}
int send_modem_assert_command(void)
{
	int ret =0;
	u32 *cmd = debug_infos.last_sent_wifi_cmd;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	char *statistic = kzalloc(2048, GFP_KERNEL);
	if(!statistic){
		skw_sdio_err("the statistic malloc fail !!\n");
		return -1;
	}
	if(skw_sdio->cp_state) {
		skw_sdio_err("the cp state is %d !!\n", skw_sdio->cp_state);
		kfree(statistic);
		statistic = NULL;
		return ret;
	}
	skw_sdio->cp_state=1;/*cp except set value*/
	ret =skw_sdio_writeb(SKW_AP2CP_IRQ_REG, BIT(4));
	if (ret != 0) {
		skw_sdio_err("the sdio writeb fail ret= %d !!\n",ret);
	}
	debug_infos.host_assert_cp_time = skw_local_clock();
	skw_sdio_err("%s ret=%d cmd: 0x%x 0x%x 0x%x :%d-%d %ums-%ums\n", __func__,
			 ret, cmd[0], cmd[1], cmd[2], skw_sdio->cp_fifo_status, fifo_ind, jiffies_to_msecs(debug_infos.last_sent_time), jiffies_to_msecs(debug_infos.host_assert_cp_time));
	skw_get_assert_print_info(statistic, 2048);
	skw_sdio_info("sdio last irqs information:\n%s\n", statistic);
	kfree(statistic);
	statistic = NULL;
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
	schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(1000));
#else
	schedule_delayed_work(&skw_sdio->skw_except_work , msecs_to_jiffies(8000));
#endif
	return ret;
}

/* for adma */
static int skw_sdio_adma_parser(struct skw_sdio_data_t *skw_sdio, struct scatterlist *sgs,
				int packet_count)
{
	struct skw_packet_header *header = NULL;
	unsigned int i;
	int channel = 0;
	unsigned int parse_len = 0;
	uint32_t *data;
	struct sdio_port *port;

	port = &sdio_ports[0];
	for (i = 0; i < packet_count; i++) {
		header = (struct skw_packet_header *)sg_virt(sgs + i);
		data = (uint32_t *)header;
		if (atomic_read(&skw_sdio->suspending))
			skw_sdio_info("ch:%d len:%d 0x%x 0x%x\n", header->channel, header->len, data[2], data[3]);
		skw_port_log(header->channel, "%s[%d]:ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n", __func__,
			i,	header->channel, header->len, data[2], data[3], data[5], data[6], data[7]);
		channel = header->channel;

		if (!header->eof && (channel < max_ch_num) && header->len) {
			parse_len += header->len;
			data = (uint32_t *)(header+1);
			if ((channel >= max_ch_num) || (header->len >
				(max_pac_size - sizeof(struct skw_packet_header))) ||
				(header->len == 0)) {
				if (channel!=0xff)
					skw_sdio_err("%s invalid header[%d]len[%d]: 0x%x 0x%x\n",
						__func__,  header->channel, header->len, data[0], data[1]);
				skw_page_frag_free(header);
				continue;
			}
			skw_sdio->rx_packer_cnt++;
			skw_sdio_handle_packet(skw_sdio, sgs+i, header, channel);
		} else {
			skw_sdio_err("%s[%d]:ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n", __func__,
			i,	header->channel, header->len, data[2], data[3], data[5], data[6], data[7]);
#if 0
			print_hex_dump(KERN_ERR, "PACKET ERR:", 0, 16, 1,
					header, 1792, 1);
			skw_sdio_err("%s PUB HAEAD ERROR: packet[%d/%d] channel=%d,size=%d eof=%d!!!",
					__func__, i, packet_count, channel, header->len, header->eof);
#endif
			skw_page_frag_free(header);
			continue;
		}
	}
	if (channel >= max_ch_num)
		skw_sdio_err("line: %d channel number error %d %d\n", __LINE__, channel, max_ch_num);
	if (debug_infos.last_irq_time && (channel > 0 && channel < max_ch_num)) {
		debug_infos.chn_last_irq_time[channel][debug_infos.chn_irq_cnt[channel] % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.chn_irq_cnt[channel]++;
	}
	atomic_set(&skw_sdio->suspending, 0);
	return 0;
}

static int skw_sdio2_adma_parser(struct skw_sdio_data_t *skw_sdio, struct scatterlist *sgs,
				int packet_count)
{
	struct skw_packet2_header *header = NULL;
	unsigned int i;
	int channel = 0;
	unsigned int parse_len = 0;
	uint32_t *data;
	struct sdio_port *port;

	port = &sdio_ports[0];
	for (i = 0; i < packet_count; i++) {
		header = (struct skw_packet2_header *)sg_virt(sgs + i);
		data = (uint32_t *)header;
		if (atomic_read(&skw_sdio->suspending))
			skw_sdio_info("ch:%d len:%d 0x%x 0x%x\n", header->channel, header->len, data[2], data[3]);

		skw_sdio_dbg("[%d]:protno:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n",
			i,      header->channel, header->len, data[2], data[3], data[5], data[6], data[7]);
		skw_port_log(header->channel, "%s[%d]:ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n", __func__,
			i,	header->channel, header->len, data[2], data[3], data[5], data[6], data[7]);

		channel = header->channel;

		if (!header->eof && (channel < max_ch_num) && header->len) {
			parse_len += header->len;
			data = (uint32_t *)(header+1);
			if ((channel >= max_ch_num) || (header->len >
				(max_pac_size - sizeof(struct skw_packet2_header))) ||
				(header->len == 0)) {
				if (channel!=0xff)
					skw_sdio_err("%s invalid header[%d]len[%d]: 0x%x 0x%x\n",
						__func__,  header->channel, header->len, data[0], data[1]);
				skw_page_frag_free(header);
				continue;
			}
			skw_sdio->rx_packer_cnt++;
			skw_sdio2_handle_packet(skw_sdio, sgs+i, header, channel);
		} else {
			if (channel!=0xff)
				skw_sdio_err("%s[%d]:ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n", __func__,
				i,	header->channel, header->len, data[2], data[3], data[5], data[6], data[7]);
			skw_page_frag_free(header);
			continue;
		}
	}
	if (channel >= max_ch_num && channel!=0xff)
		skw_sdio_err("line: %d channel number error %d %d\n", __LINE__, channel, max_ch_num);
	if (debug_infos.last_irq_time && (channel > 0 && channel < max_ch_num)) {
		debug_infos.chn_last_irq_time[channel][debug_infos.chn_irq_cnt[channel] % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.chn_irq_cnt[channel]++;
	}
	atomic_set(&skw_sdio->suspending, 0);
	return 0;
}
/* for normal dma */
static int skw_sdio_sdma_parser(char *data_buf, int total)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct skw_packet_header *header = NULL;
	int channel = 0;
	uint32_t *data;
	unsigned char *p = NULL;
	unsigned int parse_len = 0;
	int current_len=0;
#if 0
	print_hex_dump(KERN_ERR, "skw_rx_buf:", 0, 16, 1,
			data_buf, total, 1);
#endif
	header = (struct skw_packet_header *)data_buf;
	for (parse_len = 0; parse_len < total;) {
		if (header->eof != 0)
			break;
		p = (unsigned char *)header;
		data = (uint32_t *)header;
		if (atomic_read(&skw_sdio->suspending))
			skw_sdio_info("ch:%d len:%d 0x%x 0x%x\n", header->channel, header->len, data[2], data[3]);
		skw_port_log(header->channel, "%s:ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n", __func__,
				header->channel, header->len, data[1], data[2], data[3], data[4], data[5]);
		channel = header->channel;
		current_len = header->len;
		parse_len += MAX_PAC_SIZE;
		if ((channel >= max_ch_num) || (current_len == 0) ||
			(current_len > (max_pac_size - sizeof(struct skw_packet_header)))) {
			skw_sdio_err("%s skip [%d]len[%d]\n",__func__, header->channel, current_len);
			break;
		}
		skw_sdio->rx_packer_cnt++;
		skw_sdio_handle_packet(skw_sdio, NULL, header, channel);
		skw_port_log(header->channel, "the -header->len----%d\n", current_len);
		/* pointer to next packet header*/
		p += MAX_PAC_SIZE;
		header = (struct skw_packet_header *)p;
	}
	if (channel >= max_ch_num)
		skw_sdio_err("line: %d channel number error %d %d\n", __LINE__, channel, max_ch_num);
	if (debug_infos.last_irq_time && (channel > 0 && channel < max_ch_num)) {
		debug_infos.chn_last_irq_time[channel][debug_infos.chn_irq_cnt[channel] % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.chn_irq_cnt[channel]++;
	}
	atomic_set(&skw_sdio->suspending, 0);
	return 0;
}
static int skw_sdio2_sdma_parser(char *data_buf, int total)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct skw_packet2_header *header = NULL;
	int channel = 0;
	uint32_t *data;
	unsigned char *p = NULL;
	unsigned int parse_len = 0;
	int current_len=0;
#if 0
	print_hex_dump(KERN_ERR, "skw_rx_buf:", 0, 16, 1,
			data_buf, total, 1);
#endif
	header = (struct skw_packet2_header *)data_buf;
	for (parse_len = 0; parse_len < total;) {
		if (header->eof != 0)
			break;
		p = (unsigned char *)header;
		data = (uint32_t *)header;
		if (atomic_read(&skw_sdio->suspending))
			skw_sdio_info("ch:%d len:%d 0x%x 0x%x\n", header->channel, header->len, data[2], data[3]);

		skw_sdio_dbg("ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n",
			header->channel, header->len, data[1], data[2], data[3],data[4], data[5]);
		skw_port_log(header->channel, "ch:%d len:0x%0x 0x%08X 0x%08X : 0x%08X 0x%08x 0x%08X\n",
				header->channel, header->len, data[1], data[2], data[3], data[4], data[5]);
		channel = header->channel;
		current_len = header->len;
		parse_len += MAX2_PAC_SIZE;
		if ((channel >= max_ch_num) || (current_len == 0) ||
			(current_len > (max_pac_size - sizeof(struct skw_packet2_header)))) {
			skw_sdio_err("%s skip [%d]len[%d]\n",__func__, header->channel, current_len);
			break;
		}
		skw_sdio->rx_packer_cnt++;
		skw_sdio2_handle_packet(skw_sdio, NULL, header, channel);
		skw_port_log(header->channel, "the -header->len----%d\n", current_len);
		/* pointer to next packet header*/
		p +=  MAX2_PAC_SIZE;
		header = (struct skw_packet2_header *)p;
	}
	if (channel >= max_ch_num)
		skw_sdio_err("line: %d channel number error %d %d\n", __LINE__, channel, max_ch_num);
	if (debug_infos.last_irq_time && (channel > 0 && channel < max_ch_num)) {
		debug_infos.chn_last_irq_time[channel][debug_infos.chn_irq_cnt[channel] % CHN_IRQ_RECORD_NUM] = debug_infos.last_irq_time;
		debug_infos.chn_irq_cnt[channel]++;
	}
	atomic_set(&skw_sdio->suspending, 0);
	return 0;
}
struct scatterlist *skw_sdio_prepare_adma_buffer(struct skw_sdio_data_t *skw_sdio, int *sg_count, int *nsize_offset)
{
	struct scatterlist *sgs;
	void *buffer;
	int	i, j, data_size;
	int	alloc_size = FRAGSZ_SIZE;

	sgs = kzalloc((*sg_count) * sizeof(struct scatterlist), GFP_KERNEL);
	if(sgs == NULL)
		return NULL;

	for(i = 0; i < (*sg_count) - 1; i++) {
		skw_sdio_dbg("skw_sdio_alloc_frag ++ %d\n", i);
		buffer = skw_sdio_alloc_frag(alloc_size,GFP_ATOMIC);
		if(buffer)
			sg_set_buf(&sgs[i], buffer, max_pac_size);
		else{
			*sg_count = i+1;
			break;
		}
	}

	if(i <= 0)
		goto err;
	sg_mark_end(&sgs[*sg_count - 1]);
	data_size = max_pac_size*((*sg_count)-1);
	data_size = data_size%SKW_SDIO_NSIZE_BUF_SIZE;
	*nsize_offset = SKW_SDIO_NSIZE_BUF_SIZE - data_size;
	if(*nsize_offset < 8)
		*nsize_offset = SKW_SDIO_NSIZE_BUF_SIZE + *nsize_offset;
	*nsize_offset = *nsize_offset + SKW_SDIO_NSIZE_BUF_SIZE;
	sg_set_buf(sgs + i, skw_sdio->next_size_buf, *nsize_offset);
	return sgs;
err:
	skw_sdio_err("%s failed\n", __func__);
	if (i > 0) {
		for (j = 0; j < i; j++) {
			skw_page_frag_free(sg_virt(sgs + j));
		}
	}
	kfree(sgs);
	sgs = NULL;
	return NULL;

}

int skw_sdio_rx_thread(void *p)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int read_len, buf_num;
	int ret = 0;
	unsigned int rx_nsize = 0;
	unsigned int valid_len = 0;
	char *rx_buf = NULL;
	struct scatterlist *sgs = NULL;
	char fifo_ind = 0;
	unsigned char reg = 0;

	skw_sdio_sdma_set_nsize(0);
	skw_sdio_adma_set_packet_num(1);
	skw_sdio->cp_fifo_status = 0;
	while (!kthread_should_stop()) {
		/* Wait the semaphore */
		skw_sdio_rx_down(skw_sdio);
		if (!debug_infos.cp_assert_time) {
			debug_infos.last_rx_read_times[debug_infos.rx_read_cnt % CHN_IRQ_RECORD_NUM] = skw_local_clock();
			debug_infos.rx_read_cnt++;
		}
		if (atomic_read(&skw_sdio->threads_exit)) {
			skw_sdio_err("line %d threads exit\n",__LINE__);
			msleep(100);
			continue;
		}
		if (!SKW_CARD_ONLINE(skw_sdio)) {
			skw_sdio_unlock_rx_ws(skw_sdio);
			skw_sdio_err("line %d not have card\n",__LINE__);
			continue;
		}
		skw_resume_check();
		if (skw_sdio->irq_type == SKW_SDIO_EXTERNAL_IRQ) {
			int value = gpio_get_value(skw_sdio->gpio_in);
			if(value == 0) {
				skw_sdio_err("line %d gpio_in:%d\n",__LINE__, value);
				skw_sdio_unlock_rx_ws(skw_sdio);
				continue;
			}
			ret = skw_sdio_readb(SKW_SDIO_CP2AP_FIFO_IND, &fifo_ind);
			if(ret) {
				skw_sdio_err("line %d sdio cmd52 read fail ret:%d\n",__LINE__, ret);
				skw_sdio_unlock_rx_ws(skw_sdio);
				continue;
			}
			skw_sdio_dbg("line:%d cp fifo status(%d,%d) ret=%d\n",
					__LINE__,fifo_ind, skw_sdio->cp_fifo_status, ret);
			if (!ret && !fifo_ind)
				skw_sdio_dbg("cp fifo ret -- %d \n", ret);
			if(fifo_ind == skw_sdio->cp_fifo_status && !is_timeout_kick) {
				skw_sdio_info("line:%d cp fifo status(%d,%d) ret=%d\n",
						__LINE__,fifo_ind, skw_sdio->cp_fifo_status, ret);
				skw_sdio_unlock_rx_ws(skw_sdio);
				continue;
			}
#if defined(SKW_BOOT_MEMPOWERON)
			if(fifo_ind == 0xFF){
				skw_sdio_err("line %d cp assert !! the cp2ap signal=%d\n",__LINE__,fifo_ind);
				skw_sdio->service_index_map = SKW_WIFI;
				//modem_notify_event(DEVICE_ASSERT_EVENT);
				skw_sdio->boot_data->skw_dloader_module(SKW_WIFI);

			}
#endif
		}
		is_timeout_kick = 0;
		skw_sdio->cp_fifo_status = fifo_ind;
receive_again:
		if (skw_sdio->adma_rx_enable) {
			int	nsize_offset;
			buf_num = skw_sdio->remain_packet;
			if (buf_num > MAX_PAC_COUNT)
				buf_num = MAX_PAC_COUNT;

			buf_num = buf_num + 1;
			sgs = skw_sdio_prepare_adma_buffer(skw_sdio, &buf_num, &nsize_offset);
			buf_num = buf_num -1;
			if (!sgs) {
				skw_sdio_err("prepare adma buffer fail\n");
				goto submit_packets;
			}
			if (skw_sdio->power_off) {
				skw_sdio_err("line %d device power off\n", __LINE__);
				rx_nsize = 0;
				ret = -EIO;
			} else {
				ret = skw_sdio_adma_read(skw_sdio, sgs, buf_num + 1,
					buf_num * max_pac_size+nsize_offset);
			}
			if (ret != 0) {
				kfree(sgs);
				sgs = NULL;
				skw_sdio_err("%s adma read fail ret:%d\n", __func__, ret);
				goto submit_packets;
			}
			rx_nsize =  *((uint32_t *)(skw_sdio->next_size_buf + (nsize_offset - 4)));
			if (SKW_SDIO_INBAND_IRQ == skw_sdio->irq_type && rx_nsize == 0) {
				ret = skw_sdio_readb(SDIO_INT_EXT, &reg);
				if (ret < 0) {
					skw_sdio_err("line %d sdio readb error ret=%d\n", __LINE__, ret);
				} else {
					skw_sdio_dbg("line %d SDIO_INT_EXT=0x%x\n", __LINE__, reg);
				}
			}

			valid_len = *((uint32_t *)(skw_sdio->next_size_buf + (nsize_offset - 8)));
			skw_sdio_dbg("line:%d total:%lld next_pac:%d:, valid len:%d cnt %d\n",
					  __LINE__,skw_sdio->rx_packer_cnt, rx_nsize, valid_len, buf_num);

			if(skw_cp_ver == SKW_SDIO_V10){
				skw_sdio_adma_parser(skw_sdio, sgs, buf_num);
			}
			else{
				skw_sdio2_adma_parser(skw_sdio, sgs, buf_num);
			}
			kfree(sgs);
			sgs = NULL;
		} else {
			unsigned int alloc_size;

			buf_num = skw_sdio->remain_packet;
			if (buf_num > MAX_PAC_COUNT)
				buf_num = MAX_PAC_COUNT;
			if(skw_cp_ver == SKW_SDIO_V10)
				read_len = MAX_PAC_SIZE * buf_num + SKW_SDIO_BLK_SIZE;
			else
				read_len = MAX2_PAC_SIZE * buf_num + SKW_SDIO_BLK_SIZE;
			alloc_size = SKW_SDIO_ALIGN_BLK(read_len);
			rx_buf = kzalloc(alloc_size, GFP_KERNEL);
			if (!rx_buf) {
				skw_sdio_err("line %d kzalloc fail\n",__LINE__);
				goto submit_packets;
			}

			ret = skw_sdio_sdma_read(rx_buf, alloc_size);
#if 0
			print_hex_dump(KERN_ERR, "src_sdma_data:", 0, 16, 1,
					rx_buf, alloc_size, 1);
#endif
			if (ret != 0) {
				if(rx_buf){
					kfree(rx_buf);
					rx_buf = NULL;
				}
				skw_sdio_err("line %d sdma read fail ret:%d\n",__LINE__, ret);
				rx_nsize = 0;
				goto submit_packets;
			}
			rx_nsize = *((uint32_t *)(rx_buf + (alloc_size- 4)));
			if (SKW_SDIO_INBAND_IRQ == skw_sdio->irq_type && rx_nsize == 0) {
				ret = skw_sdio_readb(SDIO_INT_EXT, &reg);
				if (ret < 0) {
					skw_sdio_err("line %d sdio readb error ret=%d\n", __LINE__, ret);
				} else {
					skw_sdio_dbg("line %d SDIO_INT_EXT=0x%x\n", __LINE__, reg);
				}
			}
			valid_len = *((uint32_t *)(rx_buf + (alloc_size - 8)));

			skw_sdio_dbg("%s the sdma rx thread alloc_size:%d,read_len:%d,rx_nsize:%d,valid_len:%d\n",
					__func__,alloc_size, read_len, rx_nsize, valid_len);
			if(skw_cp_ver == SKW_SDIO_V10){
				skw_sdio_sdma_parser(rx_buf, buf_num*MAX_PAC_SIZE);
			} else {
				skw_sdio2_sdma_parser(rx_buf, buf_num*MAX2_PAC_SIZE);
			}
		}
submit_packets:
		skw_sdio_dispatch_packets(skw_sdio);
		if(rx_buf)
			kfree(rx_buf);
		skw_sdio_adma_set_packet_num(rx_nsize);
		if (skw_sdio->power_off)
			rx_nsize = 0;
		if (rx_nsize > 0)
			goto receive_again;

		debug_infos.last_irq_time = 0;
		skw_sdio_unlock_rx_ws(skw_sdio);
	}
	skw_sdio_info("%s exit\n", __func__);
	return 0;
}

static int open_sdio_port(int id, void *callback, void *data)
{
	struct sdio_port *port;
	if(id >= max_ch_num)
		return -EINVAL;

	port = &sdio_ports[id];
	if((port->state==PORT_STATE_OPEN) || port->rx_submit)
		return -EBUSY;
	port->rx_submit = callback;
	port->rx_data = data;
	init_completion(&port->rx_done);
	init_completion(&port->tx_done);
	mutex_init(&port->rx_mutex);
	port->state = PORT_STATE_OPEN;
	port->tx_flow_ctrl = 0;
	port->rx_flow_ctrl = 0;
	if(id && id!=skw_log_port()) {
		port->next_seqno = 1; //cp start seqno default no 1
		port->rx_wp = port->rx_rp = 0;
	}
	if(id == skw_log_port()) {
		skw_sdio_cp_log_disable(0);
	}
	skw_sdio_info("%s(%d) %s portno = %d\n", current->comm, current->pid, __func__, id);
	return 0;
}
static int close_sdio_port(int id)
{
	struct sdio_port *port;
	if(id >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[id];
	skw_sdio_info("%s(state=%d) portno = %d\n", current->comm, port->state, id);
	if(!port->state)
		return -ENODEV;
	port->state = PORT_STATE_CLSE;
	port->rx_submit = NULL;
	if(id == skw_log_port()) {
		skw_sdio_cp_log_disable(1);
	}
	complete(&port->rx_done);
	return 0;
}
void kill_BT_rx_transfer(void)
{
	int i;

	for (i=SDIO2_BT_CMD_PORT; i<SDIO2_WIFI_CMD_PORT; i++)
		close_sdio_port(i);
}
void send_host_suspend_indication(struct skw_sdio_data_t *skw_sdio)
{
	uint32_t value = 0;
	uint32_t timeout = 2000, timeout1 = 20;
	if(skw_sdio->gpio_out>=0 && skw_sdio->gpio_in>=0 && skw_sdio->resume_com) {
		skw_sdio_dbg("%s enter gpio=0\n", __func__);
		skw_sdio->host_active = 0;
		if (gpio_get_value(skw_sdio->gpio_in) == 0) {
			udelay(10);
			if (gpio_get_value(skw_sdio->gpio_in) == 0) {
				disable_irq(skw_sdio->irq_num);
				gpio_set_value(skw_sdio->gpio_out, 0);
				do {
					value = gpio_get_value(skw_sdio->gpio_in);
					if (value || timeout1 == 0) {
						skw_sdio_info("%s cp sts:%d in %d ms\n", __func__, value, 20 - timeout1);
						enable_irq(skw_sdio->irq_num);
						goto next;
					}
					mdelay(1);
				} while(timeout1--);
			}
		}
		gpio_set_value(skw_sdio->gpio_out, 0);
next:
		skw_sdio->device_active = 0;
		do {
			value = gpio_get_value(skw_sdio->gpio_in);
			if(value == 0)
				break;
			udelay(10);
		}while(timeout--);
	} else
		skw_sdio_dbg("%s enter\n", __func__);
}

void send_host_resume_indication(struct skw_sdio_data_t *skw_sdio)
{
	if(skw_sdio->gpio_out >= 0) {
		skw_sdio_dbg("%s enter\n", __func__);
		skw_sdio->host_active = 1;
		gpio_set_value(skw_sdio->gpio_out, 1);
		skw_sdio->resume_com = 1;
	}
}

void send_cp_wakeup_signal(struct skw_sdio_data_t *skw_sdio)
{
	if(skw_sdio->gpio_out < 0)
		return;

	gpio_set_value(skw_sdio->gpio_out, 0);
	udelay(5);
	gpio_set_value(skw_sdio->gpio_out, 1);
}

extern int skw_sdio_enable_async_irq(void);
int try_to_wakeup_modem(int portno)
{
	int ret = 0;
	int val;
	unsigned long flags;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if(skw_sdio->gpio_out < 0 || skw_sdio->gpio_in < 0
		|| skw_sdio->gpio_out == skw_sdio->gpio_in)
		return 0;
	skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);

	if(skw_sdio->device_active)
		return 0;
	skw_reinit_completion(skw_sdio->device_wakeup);
	skw_sdio->tx_req_map |= 1<<portno;
	skw_sdio_dbg("%s enter gpio_val=%d : %d\n", __func__, skw_sdio->device_active, skw_sdio->resume_com);
	skw_port_log(portno,"%s enter device_active=%d : %d\n", __func__, skw_sdio->device_active, skw_sdio->resume_com);
	if(skw_sdio->device_active == 0) {
		local_irq_save(flags);
		if(skw_sdio->resume_com==0)
			gpio_set_value(skw_sdio->gpio_out, 1);
		else
			send_cp_wakeup_signal(skw_sdio);
		local_irq_restore(flags);
		ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
		if (ret < 0) {
			skw_sdio->tx_req_map &= ~(1<<portno);
			return -ETIMEDOUT;
		}
	}
	val = gpio_get_value(skw_sdio->gpio_in);
	if(!val) {
		local_irq_save(flags);
		send_cp_wakeup_signal(skw_sdio);
		local_irq_restore(flags);
		ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
		if (ret < 0) {
			skw_sdio->tx_req_map &= ~(1<<portno);
			return -ETIMEDOUT;
		}
		val = gpio_get_value(skw_sdio->gpio_in);
	}
	if ( val && !skw_sdio->sdio_func[FUNC_1]->irq_handler &&
		!skw_sdio->resume_com && skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ) {
		sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
		ret=sdio_claim_irq(skw_sdio->sdio_func[FUNC_1],skw_sdio_inband_irq_handler);
		ret = skw_sdio_enable_async_irq();
		if (ret < 0)
			skw_sdio_err("enable sdio async irq fail ret = %d\n", ret);
		sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		skw_port_log(portno,"%s enable SDIO inband IRQ ret=%d\n", __func__, ret);
	}
	return ret;
}

int wakeup_modem(int portno)
{
	int ret = 0;
	int val;
	unsigned long flags;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if(skw_sdio->gpio_out < 0 || skw_sdio->gpio_in < 0
		|| skw_sdio->gpio_out == skw_sdio->gpio_in)
		return 0;
	skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);

	skw_reinit_completion(skw_sdio->device_wakeup);
	skw_sdio->tx_req_map |= 1<<portno;
	skw_sdio_dbg("%s enter gpio_val=%d : %d\n", __func__, skw_sdio->device_active, skw_sdio->resume_com);
	skw_port_log(portno,"%s enter device_active=%d : %d\n", __func__, skw_sdio->device_active, skw_sdio->resume_com);

	local_irq_save(flags);
	if(skw_sdio->resume_com==0)
		gpio_set_value(skw_sdio->gpio_out, 1);
	else
		send_cp_wakeup_signal(skw_sdio);
	local_irq_restore(flags);
	ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
	if (ret < 0) {
		skw_sdio->tx_req_map &= ~(1<<portno);
		return -ETIMEDOUT;
	}

	val = gpio_get_value(skw_sdio->gpio_in);
	if(!val) {
		local_irq_save(flags);
		send_cp_wakeup_signal(skw_sdio);
		local_irq_restore(flags);
		ret = wait_for_completion_interruptible_timeout(&skw_sdio->device_wakeup, HZ/100);
		if (ret < 0) {
			skw_sdio->tx_req_map &= ~(1<<portno);
			return -ETIMEDOUT;
		}
		val = gpio_get_value(skw_sdio->gpio_in);
	}
	if ( val && !skw_sdio->sdio_func[FUNC_1]->irq_handler &&
		!skw_sdio->resume_com && skw_sdio->irq_type == SKW_SDIO_INBAND_IRQ) {
		sdio_claim_host(skw_sdio->sdio_func[FUNC_1]);
		ret=sdio_claim_irq(skw_sdio->sdio_func[FUNC_1],skw_sdio_inband_irq_handler);
		ret = skw_sdio_enable_async_irq();
		if (ret < 0)
			skw_sdio_err("enable sdio async irq fail ret = %d\n", ret);
		sdio_release_host(skw_sdio->sdio_func[FUNC_1]);
		skw_port_log(portno,"%s enable SDIO inband IRQ ret=%d\n", __func__, ret);
	}
	return ret;
}

void host_gpio_in_routine(int value)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int  device_active = skw_sdio->device_active;
	if(skw_sdio->gpio_out < 0)
		return;

	skw_sdio->device_active = value;
	skw_sdio_dbg("%s enter %d-%d, host tx=0x%x:%d\n", __func__, device_active,
			skw_sdio->device_active, skw_sdio->tx_req_map, skw_sdio->host_active);
	if(device_active  && !skw_sdio->device_active &&
		skw_sdio->tx_req_map && skw_sdio->host_active) {
		send_cp_wakeup_signal(skw_sdio);
	}
	if(skw_sdio->device_active && atomic_read(&skw_sdio->resume_flag))
		complete(&skw_sdio->device_wakeup);
	if(skw_sdio->device_active) {
		if(skw_sdio->host_active == 0)
			skw_sdio->host_active = 1;
		gpio_set_value(skw_sdio->gpio_out, 1);
		skw_sdio->resume_com = 1;
	}
}

static int setup_sdio_packet(void *packet, u8 channel, char *msg, int size)
{
	struct skw_packet_header *header = NULL;
	u32 *data = packet;

	data[0] = 0;
	header = (struct skw_packet_header *)data;
	header->channel = channel;
	header->len = size;
	memcpy(data+1, msg, size);
	data++;
	data[size>>2] = 0;
	header = (struct skw_packet_header *)&data[size>>2];
	header->eof = 1;
	size += 8;
	return size;
}
static int setup_sdio2_packet(void *packet, u8 channel, char *msg, int size)
{
	struct skw_packet2_header *header = NULL;
	u32 *data = packet;

	data[0] = 0;
	header = (struct skw_packet2_header *)data;
	header->channel = channel;
	header->len = size;
	memcpy(data+1, msg, size);
	data++;
	data[size>>2] = 0;
	header = (struct skw_packet2_header *)&data[size>>2];
	header->eof = 1;
	size += 8;
	return size;
}
int loopcheck_send_data(char *buffer, int size)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_port *port;
	int ret, count;
	int loop_port = 0;
	if(skw_cp_ver == SKW_SDIO_V10){
		loop_port = LOOPCHECK_PORT;
	} else {
		loop_port = SDIO2_LOOPCHECK_PORT;
	}
	port = &sdio_ports[loop_port];
	count = (size + 3) & 0xFFFFFFFC;
	if(count + 8 < port->length) {
		if(skw_cp_ver == SKW_SDIO_V10){
			count = setup_sdio_packet(port->write_buffer, port->channel, buffer, count);
		}
		else{
			count = setup_sdio2_packet(port->write_buffer, port->channel, buffer, count);
		}
		try_to_wakeup_modem(loop_port);
		if(!(ret = skw_sdio_sdma_write(port->write_buffer, count))) {
			port->total += count;
			port->sent_packet++;
			ret = size;
		}
		skw_sdio->tx_req_map &= ~(1<<loop_port);
		return ret;
	}
	return -ENOMEM;
}

static int skw_sdio_suspend_send_data(int portno, char *buffer, int size)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_port *port;
	int ret, count, i;
	u32 *data = (u32 *)buffer;
	if(size==0)
		return 0;
	if(portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	if(!port->state || skw_sdio->cp_state)
		return -EIO;

	if(port->state == PORT_STATE_CLSE) {
		port->state = PORT_STATE_IDLE;
		return -EIO;
	}

	count = (size + 3) & 0xFFFFFFFC;
	if(count + 8 < port->length) {
		if(skw_cp_ver == SKW_SDIO_V10){
			count = setup_sdio_packet(port->write_buffer, port->channel, buffer, count);
		}
		else{
			count = setup_sdio2_packet(port->write_buffer, port->channel, buffer, count);
		}
		skw_reinit_completion(port->tx_done);
		try_to_wakeup_modem(portno);

		if(skw_sdio->cp_state)
			return -EIO;

		if(!(ret = skw_sdio_sdma_write(port->write_buffer, count))) {
			port->tx_flow_ctrl++;
			if(sdio_ports[portno].state != PORT_STATE_ASST) {
				ret = wait_for_completion_interruptible_timeout(&port->tx_done,
						msecs_to_jiffies(100));
				if(!ret && port->tx_flow_ctrl) {
					try_to_wakeup_modem(portno);
					port->tx_flow_ctrl--;
				}
			}
			port->total += count;
			port->sent_packet++;
			ret = size;
		} else {
			skw_sdio_info("%s ret=%d\n", __func__, ret);
		}
		skw_sdio->tx_req_map &= ~(1<<portno);
		skw_port_log(portno,"%s port%d size=%d 0x%x 0x%x\n",
			__func__, portno, size, data[0], data[1]);
		return ret;
	} else {
		for(i=0; i<2; i++) {
			try_to_wakeup_modem(portno);
			if(!(ret = skw_sdio_sdma_write(buffer, count))) {
				port->total += count;
				port->sent_packet++;
				ret = size;
				break;
			} else {
				skw_sdio_info("%s ret=%d\n", __func__, ret);
				if(ret == -ETIMEDOUT && !skw_sdio->device_active)
					continue;
			}
		}
		skw_sdio->tx_req_map &= ~(1<<portno);
		skw_port_log(portno,"%s port%d size=%d 0x%x 0x%x\n",
			__func__, portno, size, data[0], data[1]);
		return ret;
	}
	return -ENOMEM;
}
static int send_data(int portno, char *buffer, int size)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	struct sdio_port *port;
	int ret, count, i;
	u32 *data = (u32 *)buffer;
	if(size==0)
		return 0;
	if(portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	if(!port->state || skw_sdio->cp_state)
		return -EIO;

	if(port->state == PORT_STATE_CLSE) {
		port->state = PORT_STATE_IDLE;
		return -EIO;
	}

	count = (size + 3) & 0xFFFFFFFC;
	if(count + 8 < port->length) {
		if(skw_cp_ver == SKW_SDIO_V10){
			count = setup_sdio_packet(port->write_buffer, port->channel, buffer, count);
		}
		else{
			count = setup_sdio2_packet(port->write_buffer, port->channel, buffer, count);
		}
		skw_reinit_completion(port->tx_done);
		for(i=0; i<2; i++) {
		try_to_wakeup_modem(portno);

		if(skw_sdio->cp_state)
			return -EIO;

		if(!(ret = skw_sdio_sdma_write(port->write_buffer, count))) {
			port->tx_flow_ctrl++;
			if(sdio_ports[portno].state != PORT_STATE_ASST) {
				ret = wait_for_completion_interruptible_timeout(&port->tx_done, 
						msecs_to_jiffies(100));
				if(!ret && port->tx_flow_ctrl) {
					skw_sdio_info("%s ret=%d:%d and retry again\n", __func__, ret, port->tx_flow_ctrl);
					port->tx_flow_ctrl--;
					continue;
				}
			}
			port->total += count;
			port->sent_packet++;
			ret = size;
			break;
		} else {
			skw_sdio_info("%s ret=%d\n", __func__, ret);
			if(ret == -ETIMEDOUT && !skw_sdio->device_active)
				continue;
		}
		}
		skw_sdio->tx_req_map &= ~(1<<portno);
		skw_port_log(portno,"%s port%d size=%d 0x%x 0x%x\n",
			__func__, portno, size, data[0], data[1]);
		return ret;
	} else {
		for(i=0; i<2; i++) {
			try_to_wakeup_modem(portno);
			if(!(ret = skw_sdio_sdma_write(buffer, count))) {
				port->total += count;
				port->sent_packet++;
				ret = size;
				break;
			} else {
				skw_sdio_info("%s ret=%d\n", __func__, ret);
				if(ret == -ETIMEDOUT && !skw_sdio->device_active)
					continue;
			}
		}
		skw_sdio->tx_req_map &= ~(1<<portno);
		skw_port_log(portno,"%s port%d size=%d 0x%x 0x%x\n",
			__func__, portno, size, data[0], data[1]);
		return ret;
	}
	return -ENOMEM;
}
static int sdio_read(struct sdio_port *port, char *buffer, int size)
{
	int data_size;
	int	ret = 0;
	int buffer_size;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_dbg("%s buffer size = %d , (wp, rp) = (%d, %d), state %d\n",
			__func__, size, port->rx_wp, port->rx_rp, port->state);
	if(port->state == PORT_STATE_ASST) {
		skw_sdio_err("Line:%d The CP assert  portno =%d error code =%d cp_state=%d !!\n",__LINE__,
				port->channel, ENOTCONN, skw_sdio->cp_state);
		if(skw_sdio->cp_state!=0){
			if(port->channel==skw_log_port())
				port->state = PORT_STATE_OPEN;

			return -ENOTCONN;
		}
	}
try_again0:
	skw_reinit_completion(port->rx_done);
	if(port->rx_wp == port->rx_rp) {

		if ((port->state == PORT_STATE_CLSE) ||((port->channel>0&&port->channel !=skw_log_port())
					&& !(skw_sdio->service_state_map & (1<<BT_SERVICE)))) {
			skw_sdio_err("the log port or at port ---%d --%d\n", port->channel, skw_log_port());
			return -EIO;
		}
		if (port->timeout==0) {
			ret = wait_for_completion_interruptible(&port->rx_done);
			if(ret)
				return ret;
		} else{
			ret = wait_for_completion_interruptible_timeout(&port->rx_done,msecs_to_jiffies(port->timeout));
			if(ret==0) {
                               skw_sdio_info(" read timeout=%d\n", port->timeout);
                               return ret;
			}
		}
		if(port->state == PORT_STATE_CLSE) {
			port->state = PORT_STATE_IDLE;
			return -EAGAIN;
		}else if(port->state == PORT_STATE_ASST) {
			skw_sdio_err("The CP assert  portno =%d error code =%d!!!!\n", port->channel, ENOTCONN);
			if(skw_sdio->cp_state!=0){
				if(port->channel==skw_log_port())
					port->state = PORT_STATE_OPEN;

				return -ENOTCONN;
			}
		}
	}
	mutex_lock(&port->rx_mutex);
	data_size = (port->length + port->rx_wp - port->rx_rp)%port->length;
	if(data_size==0) {
		skw_sdio_info("%s buffer size = %d , (wp, rp) = (%d, %d)\n",
			__func__, size, port->rx_wp, port->rx_rp);
		mutex_unlock(&port->rx_mutex);
		goto try_again0;
	}
	if(size > data_size)
		size = data_size;
	data_size = port->length - port->rx_rp;
	if(size > data_size) {
		memcpy(buffer, &port->read_buffer[port->rx_rp], data_size);
		memcpy(buffer+data_size, &port->read_buffer[0], size - data_size);
		port->rx_rp = size - data_size;
	} else {
		skw_sdio_dbg("size1 = %d , (wp, rp) = (%d, %d) (packet, total)=(%d, %d)\n",
				size, port->rx_wp, port->rx_rp, port->rx_packet, port->rx_count);
		memcpy(buffer, &port->read_buffer[port->rx_rp], size);
		port->rx_rp += size;
	}

	if (port->rx_rp == port->length)
		port->rx_rp = 0;

	if(port->rx_rp == port->rx_wp){
		port->rx_rp = 0;
		port->rx_wp = 0;
	}
	if(port->rx_flow_ctrl) {
		buffer_size = (port->length + port->rx_wp - port->rx_rp)%port->length;
		buffer_size = port->length - 1 - buffer_size;

		if (buffer_size > (port->length*2/3)) {
			port->rx_flow_ctrl = 0;
			skw_sdio_rx_port_follow_ctl(port->channel, port->rx_flow_ctrl);
		}
	}
	mutex_unlock(&port->rx_mutex);
	return size;
}

int recv_data(int portno, char *buffer, int size)
{
	struct sdio_port *port;
	int ret;
	if(size==0)
		return 0;
	if(portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	if(!port->state)
		return -EIO;
	if(port->state == PORT_STATE_CLSE) {
		port->state = PORT_STATE_IDLE;
		return -EIO;
	}
	ret = sdio_read(port, buffer, size);
	return ret;
}

int recv_data_timeout(int portno, char *buffer, int size, int *actual, int timeout)
{
       struct sdio_port *port;
       int ret;
       if(size==0)
               return 0;
       if(portno >= max_ch_num)
               return -EINVAL;
       port = &sdio_ports[portno];
       if(!port->state)
               return -EIO;
       if(port->state == PORT_STATE_CLSE) {
               port->state = PORT_STATE_IDLE;
               return -EIO;
       }
       port->timeout = timeout;
       ret = sdio_read(port, buffer, size);
       port->timeout = 0;
       return ret;
}

int send_data_timeout(int portno, char *buffer, int size, int *actual, int timeout)
{
       struct sdio_port *port;
       int ret;
       if(size==0)
               return 0;
       if(portno >= max_ch_num)
               return -EINVAL;
       port = &sdio_ports[portno];
       if(!port->state)
               return -EIO;
       if(port->state == PORT_STATE_CLSE) {
               port->state = PORT_STATE_IDLE;
               return -EIO;
       }
       ret = send_data(portno, buffer, size);
       return ret;
}

int skw_sdio_suspend_adma_cmd(int portno, struct scatterlist *sg, int sg_num, int total)
{
	struct sdio_port *port;
	int ret, i;
	int irq_state = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	u32 *data;

	if(total==0)
		return 0;
	if(portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	if(!port->state)
		return -EIO;
	data = (u32 *)sg_virt(sg);
	irq_state = skw_sdio_irq_ops(0);
	for(i=0; i<2; i++) {
		try_to_wakeup_modem(portno);
		ret = skw_sdio_adma_write(portno, sg, sg_num, total);
		if(skw_sdio->gpio_in >=0)
			skw_sdio_info("timeout gpioin value=%d \n",gpio_get_value(skw_sdio->gpio_in));
		if(!ret){
			break;
		}
	}
	if(!irq_state){
		skw_sdio_irq_ops(1);
	}
	skw_sdio->tx_req_map &= ~(1<<portno);
	skw_port_log(portno,"%s port%d sg_num=%d total=%d 0x%x 0x%x\n",
			__func__, portno, sg_num, total, data[0], data[1]);
	if(portno == WIFI_CMD_PORT) {
		memcpy(debug_infos.last_sent_wifi_cmd, data, 12);
		debug_infos.last_sent_time = jiffies;
		if (skw_sdio->gpio_in >=0 && !gpio_get_value(skw_sdio->gpio_in)) {
			skw_sdio_info("modem is sleep and wakeup it\n");
			try_to_wakeup_modem(portno);
		}
	}
	port->total += total;
	port->sent_packet += sg_num;
	return ret;
}

static int skw_sdio_irq_ops(int irq_enable)
{
	int ret =-1;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if 0//def CONFIG_SKW_DL_TIME_STATS
	ktime_t cur_time,last_time;
#endif
	//struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	if(skw_sdio->gpio_in < 0 ) {
		skw_sdio_warn("gpio_in < 0 no need the cls the irq ops !!\n");
		return ret;
	}
	skw_sdio_info("gpio_in num %d the value %d !!\n",skw_sdio->gpio_in,gpio_get_value(skw_sdio->gpio_in));
	if(irq_enable){
		skw_sdio_info("enable irq\n");
		skw_sdio->suspend_wake_unlock_enable = 1;
		enable_irq(skw_sdio->irq_num);
		ret=0;
		//enable_irq_wake(skw_sdio->irq_num);
		//last_time = ktime_get();
		//skw_sdio_info("line %d start time %llu and the over time %llu ,the usertime=%llu \n",__LINE__,
		//cur_time, last_time,(last_time-cur_time));
	}else{
		if (gpio_get_value(skw_sdio->gpio_in) == 0) {
			udelay(10);
			if (gpio_get_value(skw_sdio->gpio_in) == 0) {
				disable_irq(skw_sdio->irq_num);
				ret=0;
#if 0//def CONFIG_SKW_DL_TIME_STATS
				cur_time = ktime_get();
#endif
				skw_sdio_info("disable irq\n");
			}else{
				skw_sdio_info("NO disable irq cp wake !the value %d !!\n",gpio_get_value(skw_sdio->gpio_in));
				ret = -2;
			}
		}
		//disable_irq_wake(skw_sdio->irq_num);
		//disable_irq(skw_sdio->irq_num);
	}

	return ret;
};

int wifi_send_cmd(int portno, struct scatterlist *sg, int sg_num, int total)
{
	struct sdio_port *port;
	int ret, i;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	u32 *data;

	if(total==0)
		return 0;
	if(portno >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[portno];
	if(!port->state)
		return -EIO;
	data = (u32 *)sg_virt(sg);
	for(i=0; i<2; i++) {
		try_to_wakeup_modem(portno);
		ret = skw_sdio_adma_write(portno, sg, sg_num, total);
		if(!ret)
			break;
		if (skw_sdio->gpio_in >= 0) {
			skw_sdio_info("timeout gpioin value=%d \n",gpio_get_value(skw_sdio->gpio_in));
		}
	}
	skw_sdio->tx_req_map &= ~(1<<portno);
	if (portno == WIFI_CMD_PORT ||
	   (skw_cp_ver == SKW_SDIO_V20 && portno ==  WIFI_DATA_PORT)) {
		skw_port_log(portno, "%s port%d sg_num=%d total=%d 0x%x 0x%x 0x%x 0x%x\n",
			__func__, portno, sg_num, total, data[0], data[1], data[2], data[3]);
		memcpy(debug_infos.last_sent_wifi_cmd, data, 12);
		debug_infos.last_sent_time = skw_local_clock();
	}
	port->total += total;
	port->sent_packet += sg_num;
	return ret;
}
static int register_rx_callback(int id, void *func, void *para)
{
	struct sdio_port *port;

	if(id >= max_ch_num)
		return -EINVAL;
	port = &sdio_ports[id];
	if(port->state && func)
		return -EBUSY;
	port->rx_submit = func;
	port->rx_data = para;
	if(func) {
		port->sg_rx = kzalloc(MAX_SG_COUNT * sizeof(struct scatterlist), GFP_KERNEL);
		if(port->sg_rx == NULL)
			return -ENOMEM;
		port->state = PORT_STATE_OPEN;
	} else {
		if (port->sg_rx) {
			kfree(port->sg_rx);
			port->sg_rx = NULL;
		}
		port->state = PORT_STATE_IDLE;
	}

	return 0;
}
/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static int bt_service_start(void)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
#if defined(SKW_BOOT_MEMPOWERON)
	ktime_t cur, start_poll;
	skw_sdio_info("the ---debug---line:%d \n",__LINE__);

	cur = ktime_get();
	if(skw_sdio->boot_data==NULL ||(skw_sdio->service_state_map & (1<<BT_SERVICE)))
		return ret;

	skw_sdio_info("the ---debug---line:%d \n",__LINE__);
	mutex_lock(&skw_sdio->service_mutex);
	if(skw_sdio->boot_data->iram_img_data && skw_sdio->service_index_map){
		skw_sdio_info("just download the BT img!!\n");
		skw_sdio->service_index_map = SKW_BT;
		skw_sdio_poweron_mem(SKW_BT);
		skw_sdio->boot_data->skw_dloader_module(SKW_BT);
	}
	ret=skw_sdio->boot_data->bt_start();
	skw_sdio->service_index_map = SKW_BT;
	start_poll = ktime_get();
	skw_sdio_info("the start service time =%lld", ktime_to_us(ktime_sub(start_poll, cur)));
	mutex_unlock(&skw_sdio->service_mutex);
#else
	if(skw_sdio->boot_data==NULL ||(skw_sdio->service_state_map & (1<<BT_SERVICE)))
		return ret;
	ret = skw_sdio->boot_data->bt_start();
#endif
	return ret;
}

/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static int bt_service_stop(void)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("the ---debug---line:%d \n",__LINE__);

	if(skw_sdio->boot_data ==NULL)
		return ret;
	mutex_lock(&skw_sdio->service_mutex);
	ret=skw_sdio->boot_data->bt_stop();
	mutex_unlock(&skw_sdio->service_mutex);
	return ret;
}


/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static int wifi_service_start(void)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info("the ---debug---line:%d \n",__LINE__);

	if (skw_sdio->boot_data==NULL ||(skw_sdio->service_state_map & (1<<WIFI_SERVICE)))
		return 0;
#if defined(SKW_BOOT_MEMPOWERON)
	skw_sdio_info("the ---debug---line:%d \n",__LINE__);
	mutex_lock(&skw_sdio->service_mutex);
#if SKW_WIFIONLY_DEBUG//for the debug wifi only  setvalue 0
	if(skw_sdio->boot_data->iram_img_data && glb_wifiready_done){
#else
	if(skw_sdio->boot_data->iram_img_data && skw_sdio->service_index_map){
#endif
		//skw_sdio->service_index_map &= SKW_WIFI;
		skw_sdio_info("the ---debug---line:%d \n",__LINE__);
#if 0
		if(!skw_sdio->service_index_map){
			skw_sdio_info("the first downkload firmware!!\n");
			//skw_recovery_mode();
			//skw_sdio->service_index_map = SKW_WIFI;
		}else{
			skw_sdio_info("just download the WIFI img!!\n");
			skw_sdio_poweron_mem(SKW_WIFI);
			skw_sdio->boot_data->skw_dloader_module(SKW_WIFI);
		}
#endif
		skw_sdio_info("just download the WIFI img!!\n");
		skw_sdio->service_index_map = SKW_WIFI;
		skw_sdio_poweron_mem(SKW_WIFI);
		skw_sdio->boot_data->skw_dloader_module(SKW_WIFI);
	}
	skw_sdio_info("the ---debug---line:%d \n",__LINE__);
	if (skw_sdio->boot_data->wifi_start)
		ret=skw_sdio->boot_data->wifi_start();
	skw_sdio->service_index_map = SKW_WIFI;
	//skw_sdio->service_index_map = SKW_WIFI;
	glb_wifiready_done=1;
	mutex_unlock(&skw_sdio->service_mutex);
#else
	ret=skw_sdio->boot_data->wifi_start();
#endif
	return ret;
}

/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static int wifi_service_stop(void)
{
	int ret =0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	skw_sdio_info ("the ---debug---line:%d \n",__LINE__);
	//debug code end
	if(skw_sdio->boot_data ==NULL){
		skw_sdio_info("no wifi service start before!!");
		return ret;
	}
	//mutex_lock(&skw_sdio->service_mutex);
	if(skw_sdio->boot_data->wifi_stop)
		ret=skw_sdio->boot_data->wifi_stop();
	//mutex_unlock(&skw_sdio->service_mutex);
	return ret;
}

static int wifi_get_credit(void)
{
	char val;
	int err;

	err = skw_sdio_readb(SDIOHAL_PD_DL_CP2AP_SIG4, &val);
	if(err)
		return err;
	return val;
}
static int wifi_store_credit_to_cp(unsigned char val)
{
	int err;

	err = skw_sdio_writeb(SKW_SDIO_CREDIT_TO_CP, val);

	return err;
}

void kick_rx_thread(void)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	debug_infos.cmd_timeout_cnt++;
	is_timeout_kick = 1;
	if(skw_sdio->gpio_out < 0) {
		skw_sdio_rx_up(skw_sdio);
	} else {
		skw_sdio->device_active = gpio_get_value(skw_sdio->gpio_in);
		if(skw_sdio->device_active) {
			skw_sdio_rx_up(skw_sdio);
		} else {
			try_to_wakeup_modem(LOOPCHECK_PORT);
		}
	}
}
/************************************************************************
 *Decription:bluetooth log enable
 *Author:junwei.jiang
 *Date:2023-02-16
 *Modfiy:
 *
 ********************************************************************* */
static int skw_sdio_bluetooth_log(int disable)
{
	int ret = 0;
	skw_sdio_info("Enter\n");
	if (disable) {
		skw_sdio_info("disable the CP log \n");
		disable = 0x10;
	} else {
		skw_sdio_info(" enable the CP log !!\n");
		disable = 0x18;
	}
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

	return 0;
}

struct sv6160_platform_data wifi_pdata = {
	.data_port =  WIFI_DATA_PORT,
	.cmd_port =  WIFI_CMD_PORT,
	.bus_type = SDIO_LINK|TX_ADMA|RX_ADMA|CP_DBG,
	.max_buffer_size = 84*1536,
	.align_value = 256,
	.hw_adma_tx = wifi_send_cmd,
	.hw_sdma_tx = send_data,
	.callback_register = register_rx_callback,
	.modem_assert = send_modem_assert_command,
	.service_start = wifi_service_start,
	.service_stop = wifi_service_stop,
	.skw_dloader = skw_sdio_dloader,
	.modem_register_notify = modem_register_notify,
	.modem_unregister_notify = modem_unregister_notify,
	.wifi_power_on = skw_sdio_wifi_power_on,
	.at_ops = {
		.port = 0,
		.open = open_sdio_port,
		.close = close_sdio_port,
		.read = recv_data,
		.write = send_data,
		.read_tm = recv_data_timeout,
		.write_tm = send_data_timeout,
	},
	.wifi_get_credit=wifi_get_credit,
	.wifi_store_credit=wifi_store_credit_to_cp,
	.debug_info = assert_context,
	.rx_thread_wakeup = kick_rx_thread,
	.suspend_adma_cmd = skw_sdio_suspend_adma_cmd,
	.suspend_sdma_cmd  = skw_sdio_suspend_send_data,

};
struct sv6160_platform_data ucom_pdata = {
	.data_port = 2,
	.cmd_port  = 3,
	.audio_port = 4,
	.bus_type = SDIO_LINK,
	.max_buffer_size = 0x1000,
	.align_value = 4,
	.hw_sdma_rx = recv_data,
	.hw_sdma_tx = send_data,
	.open_port = open_sdio_port,
	.close_port = close_sdio_port,
	.modem_assert = send_modem_assert_command,
	.service_start = bt_service_start,
	.service_stop = bt_service_stop,
	.modem_register_notify = modem_register_notify,
	.modem_unregister_notify = modem_unregister_notify,
	.skw_dump_mem = skw_sdio_dump,
	.bluetooth_log_disable = skw_sdio_bluetooth_log,
};

int skw_sdio_bind_platform_driver(struct sdio_func *func)
{
	struct platform_device *pdev;
	char	pdev_name[32];
	struct sdio_port *port;
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	memset(sdio_ports, 0, sizeof(struct sdio_port)*max_ch_num);
	sprintf(pdev_name, "skw_ucom");
/*
 *	creaete AT device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "ATC";
	ucom_pdata.data_port = 0;
	memcpy(ucom_pdata.chipid, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	port->next_seqno = 1; //cp start seqno default no 1
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}
	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE >> 2; //4K
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		port->pdev = NULL;
		platform_device_put(pdev);
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		kfree(port->read_buffer);
		port->read_buffer = NULL;
		platform_device_put(pdev);
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
/*
 *	creaete log device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "LOG";
	if(skw_cp_ver == SKW_SDIO_V10)
		ucom_pdata.data_port = 1;
	else
		ucom_pdata.data_port = SDIO2_BSP_LOG_PORT;
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add %s device \n", ucom_pdata.port_name);
		platform_device_put(pdev);
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	port->next_seqno = 1; //cp start seqno default no 1
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}

	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE >> 2; //4K
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		platform_device_put(pdev);
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		kfree(port->read_buffer);
		port->read_buffer = NULL;
		platform_device_put(pdev);
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	/*
	*	creaete LOOPCHECK device
	*/
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "LOOPCHECK";
	if(skw_cp_ver == SKW_SDIO_V10)
		ucom_pdata.data_port = 7;
	else
		ucom_pdata.data_port = SDIO2_LOOPCHECK_PORT;
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}

	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE >> 2;
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		kfree(port->read_buffer);
		port->read_buffer = NULL;
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
#if 0
	if(skw_cp_ver == SKW_SDIO_V20){
	/*
	 *	create BSPUPDATE device
	 */
		pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
		if(!pdev)
			return -ENOMEM;
		pdev->dev.parent = &func->dev;
		pdev->dev.dma_mask = &port_dmamask;
		pdev->dev.coherent_dma_mask = port_dmamask;
		ucom_pdata.port_name = "BSPUPDATE";
		ucom_pdata.data_port = SDIO2_BSP_UPDATE_PORT;
		ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
		if(ret) {
			dev_err(&func->dev, "failed to add platform data \n");
			platform_device_put(pdev);;
			return ret;
		}
		port = &sdio_ports[ucom_pdata.data_port];
		port->state = PORT_STATE_IDLE;
		ret = platform_device_add(pdev);
		if(ret) {
			dev_err(&func->dev, "failt to register platform device\n");
			platform_device_put(pdev);
			return ret;
		}

		port->pdev = pdev;
		port->channel = ucom_pdata.data_port;
		port->length = SDIO_BUFFER_SIZE >> 2;
		port->read_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->read_buffer == NULL) {
			dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}
		port->write_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->write_buffer == NULL) {
			dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}
	}
#endif
	return ret;
}
static int skw_sdio_gpio_check(struct skw_sdio_data_t *skw_sdio)
{
	int ret = -1;
	if(!skw_sdio)
		return ret;

	if ((SKW_SDIO_INBAND_IRQ == skw_sdio->irq_type) && (skw_sdio->gpio_in>=0 && skw_sdio->gpio_out>=0)) {
			skw_sdio_err("Please use SKW_SDIO_EXTERNAL_IRQ! irq_type=%d gpio_in=%d gpio_out=%d\n", skw_sdio->irq_type, skw_sdio->gpio_in,skw_sdio->gpio_out);
			return ret;
	}
	return 0;
}

int skw_sdio_bind_WIFI_driver(struct sdio_func *func)
{
	struct platform_device *pdev;
	char	pdev_name[32];
	struct sdio_port *port;
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	if (sdio_ports[WIFI_DATA_PORT].pdev)
		return 0;

	if(skw_sdio_gpio_check(skw_sdio)<0)
		return -EPERM;

	if (!strncmp((char *)skw_sdio->chip_id, "SV6160LITE", 10)) {
#ifdef SV6621S_WIRELESS
		sprintf(pdev_name, "%s%d", SV6621S_WIRELESS, func->num);
#else
		sprintf(pdev_name, "%s%d", SV6160_WIRELESS, func->num);
#endif
	} else if (!strncmp((char *)skw_sdio->chip_id, "SV6160", 6)) {
		sprintf(pdev_name, "%s%d", SV6160_WIRELESS, func->num);
	} else {
		skw_sdio_err(
			"unknow chip id!!! pls check you porting code!! and the connect the seekwave!!\n");
		sprintf(pdev_name, "%s%d", SV6160_WIRELESS, func->num);
	}
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
	wifi_pdata.bus_type |= CP_RLS;
#else
	if (!strncmp((char *)skw_sdio->chip_id, "SV6160", 6) ||
	    !strncmp((char *)skw_sdio->chip_id, "SV6160LITE", 10)) {
		wifi_pdata.bus_type |= CP_RLS;
	}
#endif
	/*support the sdma type bus*/
	if (!skw_sdio->adma_rx_enable) {
		if(skw_cp_ver == SKW_SDIO_V10)
			wifi_pdata.bus_type = SDIO_LINK|TX_SDMA|RX_SDMA;
		else
			wifi_pdata.bus_type = SDIO2_LINK|TX_SDMA|RX_SDMA;
	} else {
		if(skw_cp_ver == SKW_SDIO_V10)
			wifi_pdata.bus_type = SDIO_LINK|TX_ADMA|RX_ADMA|CP_DBG;
		else
			wifi_pdata.bus_type = SDIO2_LINK|TX_ADMA|RX_ADMA|CP_DBG;
	}
	wifi_pdata.align_value = skw_sdio_blk_size;
	skw_sdio_info(" wifi_pdata bus_type:0x%x \n", wifi_pdata.bus_type);
	if(skw_cp_ver == SKW_SDIO_V20){
		if(!strncmp((char *)skw_sdio->chip_id,"SV6316",6)){
			wifi_pdata.data_port = (SDIO2_WIFI_DATA1_PORT << 4) | SDIO2_WIFI_DATA_PORT;
			wifi_pdata.cmd_port = SDIO2_WIFI_CMD_PORT;
		}else if(!strncmp((char *)skw_sdio->chip_id,"SV6160LITE",10)){
			wifi_pdata.data_port = SDIO2_WIFI_DATA_PORT;
			wifi_pdata.cmd_port = SDIO2_WIFI_CMD_PORT;
		} else {
			wifi_pdata.data_port = (SDIO2_WIFI_DATA1_PORT << 4) | SDIO2_WIFI_DATA_PORT;
			wifi_pdata.cmd_port = SDIO2_WIFI_CMD_PORT;
		}
	}
	memcpy(wifi_pdata.chipid, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	ret = platform_device_add_data(pdev, &wifi_pdata, sizeof(wifi_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		//add kfree wifi_pdata
		platform_device_put(pdev);;
		return ret;
	}
	if(skw_cp_ver == SKW_SDIO_V20){
		if(!strncmp((char *)skw_sdio->chip_id,"SV6316",12)){
			port = &sdio_ports[(wifi_pdata.data_port >> 4) & 0x0F];
			port->pdev = pdev;
			port->channel = (wifi_pdata.data_port >> 4) & 0x0F;
			port->rx_wp = 0;
			port->rx_rp = 0;
			port->sg_index = 0;
			port->state = 0;
		}
	}

	port = &sdio_ports[wifi_pdata.data_port & 0x0F];
	port->pdev = pdev;
	port->channel = wifi_pdata.data_port & 0x0F;
	port->rx_wp = 0;
	port->rx_rp = 0;
	port->sg_index = 0;
	port->state = 0;

	port = &sdio_ports[wifi_pdata.cmd_port];
	port->pdev = pdev;
	port->channel = wifi_pdata.cmd_port;
	port->rx_wp = 0;
	port->rx_rp = 0;
	port->sg_index = 0;
	port->state = 0;

	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
	}
	return ret;
}
int skw_sdio_wifi_status(void)
{
	struct sdio_port *port = &sdio_ports[wifi_pdata.cmd_port];
	if (port->pdev == NULL)
		return 0;
	return 1;
}
int skw_sdio_wifi_power_on(int power_on)
{
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();
	int ret;
	if (power_on) {
		if (skw_sdio->power_off)
			skw_recovery_mode();

		ret = skw_sdio_bind_WIFI_driver(skw_sdio->sdio_func[FUNC_1]);
	} else {
		ret = skw_sdio_unbind_WIFI_driver(skw_sdio->sdio_func[FUNC_1]);
	}
	return ret;
}
#ifdef CONFIG_BT_SEEKWAVE
int skw_sdio_bind_btseekwave_driver(struct sdio_func *func)
{
	struct platform_device *pdev;
	char	pdev_name[32];
	struct sdio_port *port;
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();

	sprintf(pdev_name, "btseekwave");
/*
 *	creaete BT DATA device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;

	if(skw_cp_ver == SKW_SDIO_V20){
		ucom_pdata.data_port = SDIO2_BT_DATA_PORT;
		ucom_pdata.cmd_port = SDIO2_BT_CMD_PORT;
		ucom_pdata.audio_port = SDIO2_BT_AUDIO_PORT;
	} else {
		ucom_pdata.data_port = BT_DATA_PORT;
	}

	memcpy(ucom_pdata.chipid, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}
	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE;
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		platform_device_put(pdev);
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	} else {
		skw_sdio_info("alloc %s TX buffer success\n", ucom_pdata.port_name);
	}

/*
 *	creaete BT COMMAND device
 */
	ucom_pdata.data_port = ucom_pdata.cmd_port;
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	port->pdev = NULL;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE;
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	} else {
		skw_sdio_info("alloc %s TX buffer success\n", ucom_pdata.port_name);
	}

/*
 *	creaete BT audio device
 */
	ucom_pdata.data_port = ucom_pdata.audio_port;
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	port->pdev = NULL;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE;
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	} else {
		skw_sdio_info("alloc %s TX buffer success\n", ucom_pdata.port_name);
	}
	return ret;
}
#else
int skw_sdio_bind_BT_driver(struct sdio_func *func)
{
	struct platform_device *pdev;
	char	pdev_name[32];
	struct sdio_port *port;
	int ret = 0;
	struct skw_sdio_data_t *skw_sdio = skw_sdio_get_data();


	sprintf(pdev_name, "skw_ucom");
/*
 *	creaete BT DATA device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "BTDATA";
	if(skw_cp_ver == SKW_SDIO_V20)
		ucom_pdata.data_port = SDIO2_BT_DATA_PORT;
	else
		ucom_pdata.data_port = BT_DATA_PORT;
	memcpy(ucom_pdata.chipid, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}
	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE;
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		platform_device_put(pdev);
		kfree(port->read_buffer);
		port->read_buffer = NULL;
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}

/*
 *	creaete BT COMMAND device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "BTCMD";
	if(skw_cp_ver == SKW_SDIO_V20)
		ucom_pdata.data_port = SDIO2_BT_CMD_PORT;
	else
		ucom_pdata.data_port = ucom_pdata.cmd_port;

	//memcpy(ucom_pdata.chipid, skw_sdio->chip_id, SKW_CHIP_ID_LENGTH);
	skw_sdio_info("The check chipid ucompdata = %s \n",ucom_pdata.chipid);
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add %s device \n", ucom_pdata.port_name);
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}

	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE >> 2;
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}

/*
 *	creaete BT audio device
 */
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = &func->dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	ucom_pdata.port_name = "BTAUDIO";
	if(skw_cp_ver == SKW_SDIO_V20)
		ucom_pdata.data_port = SDIO2_BT_AUDIO_PORT;
	else
		ucom_pdata.data_port = ucom_pdata.audio_port;
	ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
	if(ret) {
		dev_err(&func->dev, "failed to add platform data \n");
		platform_device_put(pdev);;
		return ret;
	}
	port = &sdio_ports[ucom_pdata.data_port];
	port->state = PORT_STATE_IDLE;
	ret = platform_device_add(pdev);
	if(ret) {
		dev_err(&func->dev, "failt to register platform device\n");
		platform_device_put(pdev);
		return ret;
	}

	port->pdev = pdev;
	port->channel = ucom_pdata.data_port;
	port->length = SDIO_BUFFER_SIZE >> 2;
	port->read_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->read_buffer == NULL) {
		dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}
	port->write_buffer = kzalloc(port->length , GFP_KERNEL);
	if(port->write_buffer == NULL) {
		kfree(port->read_buffer);
		port->read_buffer = NULL;
		dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
		return -ENOMEM;
	}

	if(skw_cp_ver == SKW_SDIO_V20){
		/*
		*	create BTISOC device
		*/
		pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
		if(!pdev)
			return -ENOMEM;
		pdev->dev.parent = &func->dev;
		pdev->dev.dma_mask = &port_dmamask;
		pdev->dev.coherent_dma_mask = port_dmamask;
		ucom_pdata.port_name = "BTISOC";
		ucom_pdata.data_port = SDIO2_BT_ISOC_PORT;
		ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
		if(ret) {
			dev_err(&func->dev, "failed to add platform data \n");
			platform_device_put(pdev);;
			return ret;
		}
		port = &sdio_ports[ucom_pdata.data_port];
		port->state = PORT_STATE_IDLE;
		ret = platform_device_add(pdev);
		if(ret) {
			dev_err(&func->dev, "failt to register platform device\n");
			platform_device_put(pdev);
			return ret;
		}

		port->pdev = pdev;
		port->channel = ucom_pdata.data_port;
		port->length = SDIO_BUFFER_SIZE >> 2;
		port->read_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->read_buffer == NULL) {
			dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}
		port->write_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->write_buffer == NULL) {
			kfree(port->read_buffer);
			port->read_buffer = NULL;
			dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}

		/*
		*	create BTLOG device
		*/
		pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
		if(!pdev)
			return -ENOMEM;
		pdev->dev.parent = &func->dev;
		pdev->dev.dma_mask = &port_dmamask;
		pdev->dev.coherent_dma_mask = port_dmamask;
		ucom_pdata.port_name = "BTLOG";
		ucom_pdata.data_port = SDIO2_BT_LOG_PORT;
		ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
		if(ret) {
			dev_err(&func->dev, "failed to add platform data \n");
			platform_device_put(pdev);;
			return ret;
		}
		port = &sdio_ports[ucom_pdata.data_port];
		port->state = PORT_STATE_IDLE;
		ret = platform_device_add(pdev);
		if(ret) {
			dev_err(&func->dev, "failt to register platform device\n");
			platform_device_put(pdev);
			return ret;
		}

		port->pdev = pdev;
		port->channel = ucom_pdata.data_port;
		port->length = SDIO_BUFFER_SIZE >> 2;
		port->read_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->read_buffer == NULL) {
			dev_err(&func->dev, "failed to allocate %s RX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}
		port->write_buffer = kzalloc(port->length , GFP_KERNEL);
		if(port->write_buffer == NULL) {
			kfree(port->read_buffer);
			port->read_buffer = NULL;
			dev_err(&func->dev, "failed to allocate %s TX buffer\n", ucom_pdata.port_name);
			return -ENOMEM;
		}
	}
	return ret;
}
#endif
static int skw_sdio_unbind_sdio_port_driver(struct sdio_func *func, int portno)
{
	void *pdev;
	struct sdio_port *port;
	struct sv6160_platform_data *pdata = NULL;
	skw_sdio_info("port no %d\n", portno);
	port = &sdio_ports[portno];
	pdev = port->pdev;

	if (pdev) {
		pdata = ((struct sv6160_platform_data
				  *)((struct platform_device *)pdev)
				 ->dev.platform_data);
		if (pdata->port_name != NULL)
			skw_sdio_info("port name %s %d\n", pdata->port_name,
				      portno);
	}
	port->pdev = NULL;
	if (port->read_buffer)
		kfree(port->read_buffer);
	if (port->write_buffer)
		kfree(port->write_buffer);
	if (port->sg_rx)
		kfree(port->sg_rx);
	if (pdev)
		platform_device_unregister(pdev);
	port->sg_rx = NULL;
	port->read_buffer = NULL;
	port->write_buffer = NULL;
	port->rx_wp = 0;
	port->rx_rp = 0;
	port->sg_index = 0;
	port->state = 0;
	return 0;
}

int skw_sdio_unbind_platform_driver(struct sdio_func *func)
{
	int ret;

	ret = skw_sdio_unbind_sdio_port_driver(func, SDIO2_BSP_ATC_PORT);
	if(skw_cp_ver == SKW_SDIO_V20) {
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_BSP_LOG_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_LOOPCHECK_PORT);
	} else {
		ret |= skw_sdio_unbind_sdio_port_driver(func, BSP_LOG_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, LOOPCHECK_PORT);
	}
	return ret;
}

int skw_sdio_unbind_WIFI_driver(struct sdio_func *func)
{
	int ret;

	ret = skw_sdio_unbind_sdio_port_driver(func, SDIO2_WIFI_CMD_PORT);
	return ret;
}

int skw_sdio_unbind_BT_driver(struct sdio_func *func)
{
	int ret = 0;
	if (skw_cp_ver == SKW_SDIO_V20) {
		ret = skw_sdio_unbind_sdio_port_driver(func, SDIO2_BT_DATA_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_BT_CMD_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_BT_AUDIO_PORT);
#ifndef CONFIG_BT_SEEKWAVE
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_BT_LOG_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, SDIO2_BT_ISOC_PORT);
#endif
	} else {
		ret = skw_sdio_unbind_sdio_port_driver(func, BT_DATA_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, BT_CMD_PORT);
		ret |= skw_sdio_unbind_sdio_port_driver(func, BT_AUDIO_PORT);
	}
	return ret;
}
