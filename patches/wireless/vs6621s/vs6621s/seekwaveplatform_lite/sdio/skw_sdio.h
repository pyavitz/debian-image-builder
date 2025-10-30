/*
 * Copyright (C) 2022 Seekwave Tech Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SKW_SDIO_H__
#define __SKW_SDIO_H__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include "../skwutil/skw_boot.h"
#include "skw_sdio_log.h"
#define skwsdio_log(fmt, args...) \
	pr_info("[SKWSDIO]:" fmt, ## args)

#define skwsdio_err(fmt, args...) \
	pr_err("[SKWSDIO_ERR]:" fmt, ## args)

#define skwsdio_data_pr(level, prefix_str, prefix_type, rowsize,\
		groupsize, buf, len, asscii)\
		do{if(loglevel) \
			print_hex_dump(level, prefix_str, prefix_type, rowsize,\
					groupsize, buf, len, asscii);\
		}while(0)


#define SKW_AP2CP_IRQ_REG 0x1B0

#define	SKW_BUF_SIZE 	2048

#define SKW_SDIO_SDMA	0
#define SKW_SDIO_ADMA	1

#define SKW_SDIO_INBAND_IRQ	0
#define SKW_SDIO_EXTERNAL_IRQ	1

#define SDIO_RX_TASK_PRIO 	90
#define SDIO_UPDATE_TASK_PRIO 	91

#define SKW_SDIO_BLK_SIZE 	skw_sdio_blk_size
#define MAX_PAC_SIZE 		0x700
#define MAX2_PAC_SIZE 		0x600
#define MAX_PAC_COUNT		72

#define SKW_SDIO_NSIZE_BUF_SIZE SKW_SDIO_BLK_SIZE

#define SKW_SDIO_READ 		0
#define SKW_SDIO_WRITE 		1

#define SKW_SDIO_DATA_FIX 	0
#define SKW_SDIO_DATA_INC 	1

#define MAX_IO_RW_BLK 		511

#define FUNC_0  		0
#define FUNC_1  		1
#define MAX_FUNC_NUM 		2

#define SKW_SDIO_DT_MODE_ADDR	0x0f
#define SKW_SDIO_PK_MODE_ADDR	0x20

#define SKW_SDIO_RESET_MODE_ADDR	0x1C
#define SKW_SDIO_CCCR_ABORT		0x06
#define SDIO_INT_EXT			0x16
#define SDIO_ABORT_TRANS		0x01

#define SKW_SDIO_FBR_REG		0x15C

#define SKW_CHIP_ID0		0x40000000  	//SV6160 chip id0
#define SKW_CHIP_ID1		0x40000004  	//SV6160 chip id1
#define SKW_CHIP_ID2		0x40000008  	//SV6160 chip id2
#define SKW_CHIP_ID3		0x4000000C  	//SV6160 chip id3
#define SKW_SMEM_POWERON1 0x40104000  //SMEM power on [2:3] WIFI [6:7] BT
#define SKW_SMEM_POWERON2 0x40108000  //SMEM power on [2:3] WIFI [6:7] BT
#define SKW_SERVICE_EN 	0x40100000  //service enable [8:9]
#define SKW_REG_RW_LENGTH 4
#define SKW_CHIP_ID_LENGTH	16  		//SV6160 chip id lenght

#define SKW_SDIO_ALIGN_4BYTE(a)  (((a)+3)&(~3))
#define SKW_SDIO_ALIGN_BLK(a) (((a)%SKW_SDIO_BLK_SIZE) ? \
	(((a)/SKW_SDIO_BLK_SIZE + 1)*SKW_SDIO_BLK_SIZE) : (a))

#define SDIO_VER_CCCR	(0)


#define SKW_SDIO_CARD_OFFLINE 0x8000
#define SKW_CARD_ONLINE(skw_sdio) \
	(atomic_read(&skw_sdio->online) < SKW_SDIO_CARD_OFFLINE)

#define SKW_SDIO_RESET_CARD_VAL 0x08
#define SKW_SDIO_RESET_CP 	0x20

#define	WIFI_SERVICE	0
#define	BT_SERVICE	1

#define	SERVICE_START	0
#define	SERVICE_STOP	1

#define	SKW_SDIO_V10 0
#define	SKW_SDIO_V20 1


#define	BSP_ATC_PORT	0
#define	BSP_LOG_PORT	1
#define	BT_DATA_PORT	2
#define	BT_CMD_PORT	3
#define	BT_AUDIO_PORT	4
#define	WIFI_CMD_PORT	5
#define	WIFI_DATA_PORT	6
#define	LOOPCHECK_PORT	7
#define	MAX_CH_NUM	8

#define	SDIO2_BSP_ATC_PORT	0
#define	SDIO2_LOOPCHECK_PORT	1
#define	SDIO2_BT_CMD_PORT	2
#define	SDIO2_BT_AUDIO_PORT	3
#define	SDIO2_BT_ISOC_PORT	4
#define	SDIO2_BT_DATA_PORT      5
#define	SDIO2_WIFI_CMD_PORT	6
#define	SDIO2_WIFI_DATA_PORT	7
#define	SDIO2_WIFI_DATA1_PORT	8
#define	SDIO2_BSP_LOG_PORT	9
#define	SDIO2_BT_LOG_PORT	10
#define	SDIO2_BSP_UPDATE_PORT	11
#define	SDIO2_MAX_CH_NUM	12

struct skw_sdio_data_t {
	struct task_struct *rx_thread;
	struct completion rx_completed;
	struct task_struct *update_thread;
	struct completion update_completed;
#ifdef  CONFIG_WAKELOCK
	struct wake_lock rx_wl;
#else
	struct wakeup_source *rx_ws;
#endif
	atomic_t rx_wakelocked;
	struct mutex transfer_mutex;
	struct mutex except_mutex;
	struct mutex service_mutex;
	atomic_t resume_flag;
	atomic_t online;
	atomic_t threads_exit;
	bool adma_rx_enable;
	bool pwrseq;
	bool blk_size;
	/* EXTERNAL_IRQ 0, INBAND_IRQ 1. */
	unsigned char irq_type;
	atomic_t suspending;
	int gpio_out;
	int gpio_in;
	unsigned int irq_num;
	unsigned int irq_trigger_type;
	struct sdio_func *sdio_func[MAX_FUNC_NUM];
	struct mmc_host *sdio_dev_host;
	unsigned char *eof_buf;

	unsigned int next_size;
	unsigned int remain_packet;
	unsigned long long rx_packer_cnt;
	char *next_size_buf;

	struct completion scan_done;
	struct completion remove_done;
	struct completion download_done;
	int host_active;
	int device_active;
	struct completion device_wakeup;
	char tx_req_map;
	int resume_com;
	int cp_state;
	int chip_en;
	unsigned int chip_id[SKW_CHIP_ID_LENGTH];
	struct seekwave_device *boot_data;
	struct skw_log_data_t *log_data;
	unsigned int service_state_map;
	struct delayed_work skw_except_work;
	int power_off;
	unsigned int sdio_exti_gpio_state;
	int suspend_wake_unlock_enable;
	int service_index_map;
	wait_queue_head_t wq;
	u8  cp_fifo_status;
	int multi_sdio_drivers;
	u8 cp_downloaded_flag;
};

struct debug_vars {
	u16 cmd_timeout_cnt;
	u32 rx_inband_irq_cnt;
	u32 rx_gpio_irq_cnt;
	u32 rx_irq_statistics_cnt;
	u32 rx_read_cnt;
	u32 last_sent_wifi_cmd[3];
	u64 last_sent_time;
	u64 last_rx_submit_time;
	u64 host_assert_cp_time;
	u64 cp_assert_time;
	u64 last_irq_time;
	u64 rx_irq_statistics_time;
	u32 chn_irq_cnt[SDIO2_MAX_CH_NUM];
#define CHN_IRQ_RECORD_NUM 3
	u64 chn_last_irq_time[SDIO2_MAX_CH_NUM][CHN_IRQ_RECORD_NUM];
	u64 last_irq_times[CHN_IRQ_RECORD_NUM];
	u64 last_clear_irq_times[CHN_IRQ_RECORD_NUM];
	u64 last_rx_read_times[CHN_IRQ_RECORD_NUM];
};
struct skw_log_data_t {
	u16 cp_log_en;
	u16 log_level;
	u16 log_port;
	uint32_t reg_val;
	u32 smem_poweron;
	u32 service_en;
	u32 service_eb_val;
};


struct sdio_port {
	struct platform_device *pdev;
	struct scatterlist *sg_rx;
	int	 sg_index;
	int	 total;
	int	sent_packet;
	unsigned int type;
	unsigned int channel;
	rx_submit_fn rx_submit;
	void *rx_data;
	int	state;
	char *read_buffer;
	int rx_rp;
	int rx_wp;
	char *write_buffer;
	int  length;
	struct completion rx_done;
	struct completion tx_done;
	struct mutex rx_mutex;
	int	rx_packet;
	int	rx_count;
	int 	tx_flow_ctrl;
	int	 rx_flow_ctrl;
	u16	next_seqno;
	int     timeout;
};


//=======================================================
//debug sdio macro and Variable
//int glb_wifiready_done;
#define SKW_WIFIONLY_DEBUG 1
//=======================================================
void skw_resume_check(void);
struct skw_sdio_data_t *skw_sdio_get_data(void);

void skw_sdio_rx_up(struct skw_sdio_data_t *skw_sdio);
int skw_sdio_rx_thread(void *p);

void skw_sdio_unlock_rx_ws(struct skw_sdio_data_t *skw_sdio);
int skw_recovery_mode(void);
int skw_sdio_sdma_write(unsigned char *src, unsigned int len);
int skw_sdio_sdma_read(unsigned char *src, unsigned int len);
int skw_sdio_adma_write(int portno, struct scatterlist *sgs, int sg_count, int total);
int skw_sdio_adma_read(struct skw_sdio_data_t *skw_sdio, struct scatterlist *sgs, int sg_count, int total);
int skw_sdio_dt_read(unsigned int address, void *buf, unsigned int len);
int skw_sdio_dt_write(unsigned int address, void *buf, unsigned int len);
int skw_sdio_readb(unsigned int address, unsigned char *data);
int skw_sdio_writeb(unsigned int address, unsigned char data);
int skw_sdio_writel(unsigned int address, void *data);
int skw_sdio_readl(unsigned int address, void *data);
int send_modem_service_command(u16 service, u16 command);
int send_modem_assert_command(void);
int skw_sdio_bind_platform_driver(struct sdio_func * func);
int skw_sdio_bind_WIFI_driver(struct sdio_func * func);
#ifndef CONFIG_BT_SEEKWAVE
int skw_sdio_bind_BT_driver(struct sdio_func * func);
#endif
int skw_sdio_bind_btseekwave_driver(struct sdio_func * func);
int skw_sdio_unbind_platform_driver(struct sdio_func *func);
int skw_sdio_unbind_WIFI_driver(struct sdio_func * func);
int skw_sdio_unbind_BT_driver(struct sdio_func * func);
int skw_boot_loader(struct seekwave_device *boot_data);
void send_host_suspend_indication(struct skw_sdio_data_t *skw_sdio);
void send_host_resume_indication(struct skw_sdio_data_t *skw_sdio);
int try_to_wakeup_modem(int portno);
int wakeup_modem(int portno);
void host_gpio_in_routine(int value);
void skw_sdio_inband_irq_handler(struct sdio_func *func);
void modem_notify_event(int event);
int loopcheck_send_data(char *buffer, int size);
void skw_get_port_statistic(char *buffer, int size);
void skw_get_sdio_config(char *buffer, int size);
int skw_sdio_cp_log_disable(int disable);
int skw_sdio_recovery_debug(int disable);
int skw_sdio_recovery_debug_status(void);
int skw_sdio_wifi_serv_debug(int enable);
int skw_sdio_wifi_serv_debug_status(void);
int skw_sdio_bt_serv_debug(int enable);
int skw_sdio_bt_serv_debug_status(void);
void reboot_to_change_bt_antenna_mode(char *mode);
void reboot_to_change_bt_uart1(char *mode);
void get_bt_antenna_mode(char *mode);
int skw_sdio_wifi_power_on(int power_on);
int skw_sdio_wifi_status(void);
int skw_sdio_dloader(int index);
int skw_sdio_poweron_mem(int index);
void skw_get_sdio_debug_info(char *buffer, int size);
void skw_get_assert_print_info(char *buffer, int size);
int skw_sdio_debug_log_open(void);
int skw_sdio_debug_log_close(void);
int skw_sdio_gpio_irq_pre_ops(void);
int skw_sdio_chk_cp_gpio_cfg(void);
void send_cp_wakeup_signal(struct skw_sdio_data_t *skw_sdio);
int skw_sdio_smem_poweron(void); // for smem
void kill_BT_rx_transfer(void);
void skw_sdio_cp_dumpmem(void);
#endif /* SKW_SDIO_H */
