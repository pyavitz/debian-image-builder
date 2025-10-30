/************************************************************************
 *Copyright(C) 2020-2021: Seekwave tech LTD 		China
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include "skw_usb.h"
#include "skw_usb_log.h"
#include "skw_usb_debugfs.h"
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

#define MAX_BUFFER_SIZE 20*1024
#define MAX_MSG_SIZE	MAX_BUFFER_SIZE

#define VENDOR_MSG_MODEM_ASSERT 0xA5
#define VENDOR_MSG_SERVICE_CTRL 0xA6
#define VENDOR_MSG_PACKET_COUNT 0xA7
#define VENDOR_MSG_LOG_SWITCH	0xA8
#define VENDOR_MSG_MODEM_RESET  0xA9
#define VENDOR_MSG_MODEM_SUSP   0xAA

#define	WIFI_SERVICE	0
#define BT_SERVICE	  1

#define SERVICE_START	0
#define SERVICE_STOP	1

#define MODEM_OFF		0
#define MODEM_ON		1
#define MODEM_HALT		2
#define MODEM_DOWNLOAD_FAILED   4

#define WIFI_PORT_SHARE_FLAG	0x4000
#define MODEM_WDT_SUPPORT       0x40
#define USB_HOST_RESUME_SUPPORT 0x20

#define MAX_USB_PORT MAX_PORT_COUNT
#define MAX_PACKET_COUNT 20
static struct delayed_work skw_except_work;
static struct work_struct add_device_work;
static struct work_struct dump_memory_worker;
static struct work_struct usb_control_worker;
static struct platform_device *wifi_data_pdev;
static u64 port_dmamask = DMA_BIT_MASK(32);
static u32 service_state_map = 0;
static int cp_log_status = 0;
static char *firmware_data;
static int	firmware_size;
static int	firmware_addr;
static struct seekwave_device *usb_boot_data = NULL;
static struct completion download_done;
static struct completion loop_completion;
static BLOCKING_NOTIFIER_HEAD(modem_notifier_list);
static int chip_en_gpio;
static int host_wake_gpio;
static int modem_status;
static int usb_speed_switching;
static int cls_recovery_mode_en;
static char *skw_chipid = NULL;
static u32 last_sent_wifi_cmd[3];
static u32 last_recv_wifi_evt[3];
static u32 last_recv_wifi_ack[3];
static u64 last_sent_time, last_ack_time;
static struct scatterlist *sgs;
static int nr_sgs;
static int start_service_flag = 0;
static u16 transfer_index;
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */

static const struct usb_device_id skw_usb_io_id_table[] = {
	{ USB_VENDOR_AND_INTERFACE_INFO(0x3607, 0x02, 0x02, 0) },
	{ USB_DEVICE(0x3607, 0x6316) },
	{ USB_DEVICE(0x3607, 0x6621) },
	{ USB_DEVICE(0x3607, 0x6160) },
	{} /* Terminating entry */
};
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:XW
 *
 ********************************************************************* */
static struct recovery_data{
	struct mutex except_mutex;
	int cp_state;
} g_recovery_data;

#ifdef CONFIG_SKW_DL_TIME_STATS
	ktime_t cur_time,last_time;
#endif

#define SKW_USB_GET_RECOVERY_DATA() &g_recovery_data

static struct usb_port_struct {
	struct work_struct work;
	struct platform_device *pdev;
	int	portno;
	struct usb_interface *interface;
	struct usb_device *udev;
	struct urb *read_urb;
	struct usb_endpoint_descriptor *epin;
	struct urb *write_urb;
	struct usb_endpoint_descriptor *epout;
	char *read_buffer;
	char	*write_buffer;
	int		buffer_size;
	struct usb_anchor read_submitted;
	struct usb_anchor write_submitted;
	struct task_struct *thread;
	rx_submit_fn rx_submit;
	adma_callback adma_tx_callback;
	sdma_callback sdma_tx_callback;
	void *rx_data;
	void *tx_data;
	int	state;
	int  ep_mps;
	int  max_packet_count;
	struct semaphore sem;
	int	is_dloader;
	int	sent_packet_count;
	int	req_tx_packet;
	wait_queue_head_t	rx_wait;
	wait_queue_head_t	tx_wait;
	struct tasklet_struct tasklet;
	struct list_head rx_urb_list;
	struct list_head tx_urb_list;
	struct list_head rx_done_urb_list;
	struct list_head suspend_urb_list;
	spinlock_t rx_urb_lock;
	spinlock_t tx_urb_lock;
	int	tx_urb_count;
	int	rx_packet_count;
	int     suspend;
	u64 	tx_done_time;
	u64 	rx_done_time;
} *usb_ports[MAX_USB_PORT];

static int modem_assert(void);
static int skw_recovery_mode(void);
static struct usb_port_struct *log_port;
int skw_reset_bus_dev(void);
extern void kernel_restart(char *cmd);
static int bulkin_read_timeout(int portno, char *buffer, int size, int *actual, int timeout);
static int bulkout_write_timeout(int portno, char *buffer, int size, int *actual, int timeout);
static void bulkout_async_complete(struct urb *urb);
static void bulkin_async_complete(struct urb *urb);
static int assert_info_print;
static int usb_bt_rx_entry(void *para);
char firmware_version[256];
#ifdef CONFIG_BT_SEEKWAVE
static int	bt_audio_port;
#endif
static struct platform_device *bluetooth_pdev = NULL;
static int wifi_port_share;
static int bulk_async_read;
static int dump_memory_done;
static char* dump_memory_buffer=NULL;
static int dump_buffer_size=0;
static int* dump_log_size=NULL;
static int usb_bus_num;
static int usb_port_num;

void skw_get_port_statistic(char *buffer, int size)
{
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	int ret = 0;
	int i;

	if(!buffer)
		return;
	ret += sprintf(&buffer[ret], "%s cp_state=%d\n", firmware_version, recovery->cp_state);
	for(i=0; i<MAX_USB_PORT; i++) {
		if(ret >= size)
			break;

		if (usb_ports[i]) {
			ret += sprintf(&buffer[ret],
				"port%d: req_tx %d tx_done %d, rx %d: tx_time: 0x%x rx_time: 0x%x\n",
				i, usb_ports[i]->req_tx_packet,	usb_ports[i]->sent_packet_count,
		       		usb_ports[i]->rx_packet_count, (u32)usb_ports[i]->tx_done_time,
				(u32)usb_ports[i]->rx_done_time);
		}
	}
	ret += sprintf(&buffer[ret],"CMD: 0x%x 0x%x 0x%x\n",
			last_sent_wifi_cmd[0], last_sent_wifi_cmd[1], last_sent_wifi_cmd[2]);
	ret += sprintf(&buffer[ret],"ACK: 0x%x 0x%x 0x%x\n",
			last_recv_wifi_ack[0], last_recv_wifi_ack[1], last_recv_wifi_ack[2]);
}

#include "usb_boot.c"
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
void skw_usb_exception_work(struct work_struct *work)
{
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	skw_usb_info(" enter cp_state=%d...\n", recovery->cp_state);
	mutex_lock(&recovery->except_mutex);
	if(recovery->cp_state!=1)
	{
		mutex_unlock(&recovery->except_mutex);
		return;
	}
	recovery->cp_state = DEVICE_BLOCKED_EVENT;
	mutex_unlock(&recovery->except_mutex);
	modem_notify_event(DEVICE_BLOCKED_EVENT);
	service_state_map=0;
	skw_recovery_mode();
}

int skw_usb_recovery_debug(int disable)
{
	cls_recovery_mode_en = disable;
	skw_usb_info("the recovery status =%d\n", cls_recovery_mode_en);
	return 0;
}

int skw_usb_recovery_debug_status(void)
{
	skw_usb_info("the recovery val =%d\n", cls_recovery_mode_en);
	return cls_recovery_mode_en;
}


static void usb_setup_service_devices(void)
{
#ifdef CONFIG_BT_SEEKWAVE
	struct usb_port_struct *bt_port;
#endif
	int ret;

	skw_bind_boot_driver(&usb_ports[0]->udev->dev);
	if(usb_ports[1]->pdev){
		if(wifi_data_pdev==NULL) {
			ret = platform_device_add(usb_ports[1]->pdev);
			if(ret) {
				skw_usb_err("the fail to register WIFI device\n");
				wifi_data_pdev = NULL;
				platform_device_put(usb_ports[1]->pdev);
			} else {
				wifi_data_pdev = usb_ports[1]->pdev;
				skw_usb_info("add WIFI devices done\n");
			}
		 }
	} else
		 skw_usb_err("NOT suppport WIFI service\n");
#ifdef CONFIG_BT_SEEKWAVE
	if (bluetooth_pdev) {
		bt_port = usb_ports[bt_audio_port];
		bt_port->pdev = bluetooth_pdev;
		bluetooth_pdev = NULL;
		ret = platform_device_add(bt_port->pdev);
		if(ret) {
			skw_usb_err("failt to register Bluetooth device\n");
			platform_device_put(bt_port->pdev);
			bt_port->pdev = NULL;
		} else
			skw_usb_info("add Bluetooth devices done\n");
	}
#endif

}
void add_devices_work(struct work_struct *work)
{
	if (usb_ports[0])
		usb_setup_service_devices();
}
void skw_set_bt_suspend_flag(void)
{
}
static void usb_port_alloc_recv_urbs(struct usb_port_struct *port, struct usb_endpoint_descriptor *epd, int count, int buffer_size)
{
	int i;
	struct urb *urb;

	for(i=0; i<count; i++) {
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if(!urb)
			break;
		if(!buffer_size) {
			urb->transfer_buffer = NULL;
			urb->transfer_buffer_length = 0;
		} else{
			urb->transfer_buffer = kzalloc(buffer_size, GFP_KERNEL);
			if(!urb->transfer_buffer) {
				usb_free_urb(urb);
				break;
			}
			urb->transfer_buffer_length = buffer_size;
		}
		usb_fill_bulk_urb(urb, port->udev,usb_rcvbulkpipe(port->udev, epd->bEndpointAddress),
			urb->transfer_buffer, buffer_size, bulkin_async_complete, NULL);
		list_add_tail(&urb->urb_list, &port->rx_urb_list);
	}
	skw_usb_dbg(" urb cout %d\n", i);
}

static void usb_port_alloc_xmit_urbs(struct usb_port_struct *port, struct usb_endpoint_descriptor *epd, int count, int buffer_size)
{
	int i;
	struct urb *urb;

	for(i=0; i<count; i++) {
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if(!urb)
			break;
		if(!buffer_size) {
			urb->transfer_buffer = NULL;
			urb->transfer_buffer_length = 0;
		} else{
			urb->transfer_buffer = kzalloc(buffer_size, GFP_KERNEL);
			if(!urb->transfer_buffer) {
				usb_free_urb(urb);
				break;
			}
			urb->transfer_buffer_length = buffer_size;
		}
		usb_fill_bulk_urb(urb, port->udev,usb_sndbulkpipe(port->udev, epd->bEndpointAddress),
			urb->transfer_buffer, buffer_size, bulkout_async_complete, NULL);
		list_add_tail(&urb->urb_list, &port->tx_urb_list);
	}
	skw_usb_dbg(" urb cout %d\n", i);
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
int open_usb_port(int id, void *callback, void *data)
{
	struct usb_port_struct *port;

	if(id >= MAX_USB_PORT)
		return -EINVAL;

	port = usb_ports[id];
	if(port->state==0)
		return -EIO;
	skw_usb_info("port%d\n", id);
	if (port->state==1) {
		if(port->read_urb && !port->read_urb->context)
			init_usb_anchor(&port->read_submitted);
		if(port->write_urb && !port->write_urb->context)
			init_usb_anchor(&port->write_submitted);
	}
	port->state = 2;
	port->rx_submit = callback;
	port->rx_data = data;
	if(callback && data && !port->thread) {
		sema_init(&port->sem, 0);
		port->thread = kthread_create(usb_bt_rx_entry, port, port->interface->cur_altsetting->string);
		if(port->thread)
			wake_up_process(port->thread);
	}
	if (port->interface && modem_status==MODEM_ON) {
		struct usb_host_interface *iface_desc;
		iface_desc = port->interface->cur_altsetting;
		if (iface_desc && iface_desc->string &&
		    !strncmp(iface_desc->string, "LOG", 3))
			skw_usb_cp_log(0);
	}
	return 0;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int  bulkin_read(struct usb_port_struct *port, void *buffer, int size)
{
	int retval = -1;
	struct urb *urb;
	DECLARE_COMPLETION_ONSTACK(done);
	if(port->state==0)
		return -EIO;

	if(port == log_port){
		memset(buffer, 0 , size);
	}

	if(port->read_urb) {
		urb = port->read_urb;
		urb->transfer_buffer = buffer;
		urb->transfer_buffer_length = size;
		urb->context = &done;
		usb_anchor_urb(urb, &port->read_submitted);
		retval = usb_submit_urb(urb,GFP_KERNEL);
		if(retval==0) {
			retval = wait_for_completion_interruptible(&done);
			if(retval == -ERESTARTSYS)
				usb_kill_urb(urb);
			else if(urb->status) {
				retval = urb->status;
				if (retval == -ENOENT && port->read_urb->actual_length)
					retval = urb->actual_length;
			} else if(retval==0)
				retval = urb->actual_length;
			urb->context = NULL;
		} else {
			 if (retval < 0)
				 usb_unanchor_urb(urb);
			urb->context = NULL;
		}
	}
	if(port == log_port) {
		if(assert_info_print && assert_info_print<28 && retval<100) {
			assert_info_print++;
			if(retval > 4)
				skw_usb_info("%s", (char *)buffer);
		}
		if(retval == 4)
			assert_info_print = 28;
	}
	return retval;
}
int skw_bus_version(void)
{
	skw_usb_info("USB bus Version1.0\n");
	return 0;
}
/*******************************************************
 * bulkin_read_async - async read from bulk in endpoint
 * @port: pointer to struct usb_port_struct
 *
 * Submit a read urb for bulk in endpoint asynchronously.
 *
 * Return: 0 on success, negative value on error
 ******************************************************/
int bulkin_read_async(struct usb_port_struct *port)
{
	int	 retval = -1;
	unsigned long flags;
	struct urb *urb;

	spin_lock_irqsave(&port->rx_urb_lock, flags);
	urb = list_first_entry(&port->rx_urb_list, struct urb, urb_list);
	list_del_init(&urb->urb_list);
	spin_unlock_irqrestore(&port->rx_urb_lock, flags);
	if(urb->context) {
		skw_usb_info("port is busy!!!\n");
		return -EBUSY;
	}

	urb->complete = bulkin_async_complete;
	urb->context = port;
	if (port->suspend)
		list_add_tail(&urb->urb_list, &port->suspend_urb_list);
	else {
		usb_anchor_urb(urb, &port->read_submitted);
		bulk_async_read++;
		retval = usb_submit_urb(urb, GFP_ATOMIC);
		if (retval < 0) {
			bulk_async_read--;
			usb_unanchor_urb(urb);
			urb->context = NULL;
			skw_usb_info(" is error!!! %d\n", retval);
			list_add_tail(&urb->urb_list, &port->suspend_urb_list);
		}
	}
	return retval;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int bulkout_write(struct usb_port_struct *port, void *buffer, int size)
{
	int retval = -1;
	struct urb *urb;
	DECLARE_COMPLETION_ONSTACK(done);


	if (port && port->write_urb && !port->write_urb->context) {
		if (port->suspend) {
		skw_usb_info("port%d is suspended\n", port->portno);
			return -EOPNOTSUPP;
		}
		urb = port->write_urb;
		urb->context = &done;
		urb->transfer_buffer = buffer;
		urb->transfer_buffer_length = size;
		if(size%port->ep_mps == 0)
			urb->transfer_flags |= URB_ZERO_PACKET;
		usb_anchor_urb(urb, &port->write_submitted);
		retval = usb_submit_urb(urb,GFP_KERNEL);
		if(retval==0) {
			retval = wait_for_completion_interruptible(&done);
			if(retval==-ERESTARTSYS)
				usb_kill_urb(urb);
			else if (urb->status)
				retval = urb->status;
			else
				retval = urb->actual_length;
			urb->context = NULL;
		} else {

			if (retval < 0) {
				usb_unanchor_urb(urb);
				skw_usb_info("is error!!! %d\n", retval);
			}
			urb->context = NULL;
		}
	}
	return retval;
}

int bulkout_write_async(struct usb_port_struct *port, void *buffer, int size)
{
	int retval = -1;
	struct urb *urb;
	unsigned long flags;

	if (port->suspend) {
		skw_usb_info("port%d is suspended\n", port->portno);
		return -EOPNOTSUPP;
	}
	spin_lock_irqsave(&port->tx_urb_lock, flags);
	if(list_empty(&port->tx_urb_list)) {
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
		retval = wait_event_interruptible(port->tx_wait, (!list_empty(&port->tx_urb_list)));
		spin_lock_irqsave(&port->tx_urb_lock, flags);
	}
	if (port->state==0) {
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
		return -EIO;
	}
	urb = list_first_entry(&port->tx_urb_list, struct urb, urb_list);
	list_del_init(&urb->urb_list);
	port->tx_urb_count++;
	spin_unlock_irqrestore(&port->tx_urb_lock, flags);

	usb_fill_bulk_urb(urb, port->udev,usb_sndbulkpipe(port->udev, port->epout->bEndpointAddress),
		buffer, size, bulkout_async_complete, port);
	if(size%port->ep_mps == 0)
		urb->transfer_flags |= URB_ZERO_PACKET;
		
	usb_anchor_urb(urb, &port->write_submitted);
	retval = usb_submit_urb(urb,GFP_KERNEL);
	if (retval < 0) {
		usb_unanchor_urb(urb);
		spin_lock_irqsave(&port->tx_urb_lock, flags);
		port->tx_urb_count--;
		list_add_tail(&urb->urb_list, &port->tx_urb_list);
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
		skw_usb_info("is error!!! %d\n", retval);
	}
	skw_usb_dbg(" portno %d wait done %d %d\n", port->portno, retval, port->tx_urb_count);
	return retval;
}
void check_sgs_headers(struct scatterlist *sgs, int sg_num, int total)
{
	int i,size;
	struct skw_packet2_header *header;

	size = 0;
	for (i=0; i<sg_num; i++) {
		header = (struct skw_packet2_header *)sg_virt(sgs + i);
		size += header->len;
		if (header->len > 2048 || size > total)
			skw_usb_err("invalid packet: (%d - %d):( %d-%d-%d)\n", total, sg_num, i, header->len, size);
	}
}

int bulkout_write_sg_async(struct usb_port_struct *port, struct scatterlist *sgs, int sg_num, int total)
{
	struct urb *urb;
	unsigned long flags;
	int retval = -1;

	check_sgs_headers(sgs, sg_num, total);
	spin_lock_irqsave(&port->tx_urb_lock, flags);
	if(list_empty(&port->tx_urb_list)) {
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
		retval = wait_event_interruptible(port->tx_wait, (!list_empty(&port->tx_urb_list)));
		spin_lock_irqsave(&port->tx_urb_lock, flags);
	}
	if (port->state==0) {
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
		return -EIO;
	}
	urb = list_first_entry(&port->tx_urb_list, struct urb, urb_list);
	port->tx_urb_count++;
	list_del_init(&urb->urb_list);
	port->req_tx_packet += sg_num;
	spin_unlock_irqrestore(&port->tx_urb_lock, flags);
	urb->transfer_buffer = NULL;
	urb->transfer_buffer_length = 0;
	usb_fill_bulk_urb(urb, port->udev,usb_sndbulkpipe(port->udev, port->epout->bEndpointAddress),
		NULL, 0, bulkout_async_complete, port);
	urb->sg = sgs;
	urb->num_sgs = sg_num;
	urb->transfer_buffer_length = total;
	if(total%port->ep_mps == 0)
		urb->transfer_flags |= URB_ZERO_PACKET;
	usb_anchor_urb(urb, &port->write_submitted);
	//skw_usb_info("portno %d submit  %d\n", port->portno, port->tx_urb_count);
	retval = usb_submit_urb(urb,GFP_KERNEL);
	if (retval < 0) {
		usb_unanchor_urb(urb);
		spin_lock_irqsave(&port->tx_urb_lock, flags);
		port->tx_urb_count--;
		list_add_tail(&urb->urb_list, &port->tx_urb_list);
		spin_unlock_irqrestore(&port->tx_urb_lock, flags);
	}
	return retval;

}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int bulkout_write_sg(struct usb_port_struct *port, struct scatterlist *sgs, int sg_num, int total)
{
	int	 retval = -1;
	DECLARE_COMPLETION_ONSTACK(done);

	if(!port->write_urb)
		return -ENODEV;
	if(port->write_urb->context) {
		skw_usb_info("port is busy!!!\n");
		return -EBUSY;
	}
	if(port->write_urb) {
		port->write_urb->sg = sgs;
		port->write_urb->num_sgs = sg_num;
		port->write_urb->transfer_buffer_length = total;
		if(total%port->ep_mps == 0)
			port->write_urb->transfer_flags |= URB_ZERO_PACKET;
		port->write_urb->context = &done;
		port->req_tx_packet += port->write_urb->num_sgs;
		usb_anchor_urb(port->write_urb, &port->write_submitted);
		retval = usb_submit_urb(port->write_urb,GFP_KERNEL);
		if(retval==0) {
			retval = wait_for_completion_interruptible(&done);
			if(retval==0)
				retval = port->write_urb->actual_length;
			port->write_urb->context = NULL;
			port->sent_packet_count += sg_num;

		} else {
			skw_port_log(port->portno, "%s retval = %d\n", __func__, retval);
			usb_unanchor_urb(port->write_urb);
			port->write_urb->context = NULL;
		}
	}
	if(retval > 0)
		return 0;
	return retval;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int send_data(int portno, char *buffer, int total)
{
	struct usb_port_struct *port;
	u32 *data;

	if(total==0)
		return 0;
	if (modem_status != MODEM_ON)
		return -EIO;
	port = usb_ports[portno];
	if(!port || !port->state)
		return -EIO;
	if (port->suspend) {
		skw_usb_info("port%d is suspended\n", portno);
		return -EOPNOTSUPP;
	}
	if (portno == 0) {
		data = (u32 *)buffer;
		memcpy(last_sent_wifi_cmd, data, 12);
		last_sent_time = jiffies;
		last_sent_wifi_cmd[0] =  bulk_async_read;
	}
	return bulkout_write(port, buffer, total);
}
static void check_first_header(char *buffer, int total)
{
	struct skw_packet_header *header;

	header = (struct skw_packet_header *)buffer;
	if (header->len > 1536 || header->len > total)
		skw_usb_err("invalid packet: (%d - %d)\n", total, header->len);
}
static int send_data_async(int portno, char *buffer, int total)
{
	struct usb_port_struct *port;
	int ret;

	if(total==0)
		return 0;
	if (modem_status != MODEM_ON)
		return -EIO;
	port = usb_ports[portno];
	if(!port || !port->state)
		return -EIO;
	ret = bulkout_write_async(port, buffer, total);
	check_first_header(buffer, total);
	return ret;
}

int recv_data(int portno, char *buffer, int total)
{
	struct usb_port_struct *port;

	if(total==0)
		return 0;

	port = usb_ports[portno];
	if(!port || !port->state)
		return -EIO;
	return bulkin_read(port, buffer, total);
}

int close_usb_port(int portno)
{
	struct usb_port_struct *port;

	port = usb_ports[portno];

	skw_usb_info("port%d\n", portno);
	if (port) {
		port->state = 1;
		if(port->write_urb && port->write_urb->context)
			usb_kill_urb(port->write_urb);
		if(port->read_urb && port->read_urb->context)
			usb_kill_urb(port->read_urb);
		if(port->thread && down_interruptible(&port->sem))
			skw_usb_info("port%d rx thread exit\n", portno);
		port->thread = NULL;
		if (port->interface) {
			struct usb_host_interface *iface_desc;
			iface_desc = port->interface->cur_altsetting;
			if (iface_desc && iface_desc->string &&
			    !strncmp(iface_desc->string, "LOG", 3))
				skw_usb_cp_log(1);
		}
	}
	return 0;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
int wifi_send_cmd(int portno, struct scatterlist *sg, int sg_num, int total)
{
	struct usb_port_struct *port;
	u32 *data;
	int ret;

	if(total==0)
		return 0;
	if (modem_status != MODEM_ON)
		return -EIO;
	if(portno >= MAX_USB_PORT)
		return -EINVAL;
	port = usb_ports[portno];
	if(!port || !port->state)
		return -EIO;
	if (port->suspend) {
		skw_usb_info("port%d is suspended\n", portno);
		return -EOPNOTSUPP;
	}
	if (portno == 0) {
		data = (u32 *)sg_virt(sg);
		memcpy(last_sent_wifi_cmd, data, 12);
		last_sent_time = jiffies;
		last_sent_wifi_cmd[0] =  bulk_async_read;
	}
	ret = bulkout_write_sg(port, sg, sg_num, total);
	return ret;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
int wifi_send_cmd_async(int portno, struct scatterlist *sg, int sg_num, int total)
{
	struct usb_port_struct *port;
	u32 *data;

	if(total==0)
		return 0;
	if (modem_status != MODEM_ON)
		return -EIO;
	if(portno >= MAX_USB_PORT)
		return -EINVAL;
	port = usb_ports[portno];
	if(!port || !port->state)
		return -EIO;

	if (port->suspend) {
		skw_usb_info("port%d is suspended\n", portno);
		return -EOPNOTSUPP;
	}
	if (portno == 0) {
		data = (u32 *)sg_virt(sg);
		memcpy(last_sent_wifi_cmd, data, 12);
		last_sent_time = jiffies;
		last_sent_wifi_cmd[0] =  bulk_async_read;
	}
	return bulkout_write_sg_async(port, sg, sg_num, total);
}

/************************************************************************
 *Decription: manual assert modem
 *Author:jiayong.yang
 *Date:2021-08-03
 *Modfiy:
 *Notes: this function must not be invoked in IRQ context.
 ************************************************************************/
static int modem_assert_work(void)
{
	struct usb_port_struct *port;
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	int ret = -1;
	 u32 *cmd = last_sent_wifi_cmd;

	if(modem_status == MODEM_HALT){
		skw_usb_info("modem in recovery mode \n");
		return 0;
	}
	port = usb_ports[0];
	if(port && port->state) {
		recovery->cp_state =1;
		ret = usb_control_msg(port->udev, usb_sndctrlpipe(port->udev, 0),
				VENDOR_MSG_MODEM_ASSERT, USB_DIR_OUT| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				0,0,NULL,0,1000);
		skw_usb_err("SND ASSERT CMD ret = %d cmd: 0x%x 0x%x 0x%x: ACK 0x%x-0x%x-0x%x EVT: 0x%x 0x%x 0x%x \n",
				ret, cmd[0], cmd[1], cmd[2],
			       	last_recv_wifi_ack[0],last_recv_wifi_ack[1],last_recv_wifi_ack[2],
			       	last_recv_wifi_evt[0],last_recv_wifi_evt[1],last_recv_wifi_evt[2]);
		modem_status = MODEM_HALT;
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
		schedule_delayed_work(&skw_except_work , msecs_to_jiffies(2000));
#else
		schedule_delayed_work(&skw_except_work , msecs_to_jiffies(6000));
#endif
	}
	return ret;
}
static void usb_control_work(struct work_struct *work)
{
	modem_assert_work();
}
static int modem_assert(void)
{
	struct usb_port_struct *port;

	port = usb_ports[0];
	if (port)
		schedule_work(&usb_control_worker);
	return 0;
}
int wifi_service_start(void)
{
	int ret = 0;

	if(!usb_boot_data)
		return -ENODEV;
	ret=usb_boot_data->wifi_start();

	return ret;
}

int wifi_service_stop(void)
{
	int ret = 0;
	if(!usb_boot_data)
		return -ENODEV;
	ret=usb_boot_data->wifi_stop();
	return ret;
}

int bt_service_start(void)
{
	int ret = 0;

	if(!usb_boot_data)
		return -ENODEV;
	ret=usb_boot_data->bt_start();
	return ret;
}

int bt_service_stop(void)
{
	int ret = 0;

	if(!usb_boot_data)
		return -ENODEV;
	ret=usb_boot_data->bt_stop();
	return ret;
}
static int send_modem_service_command(u16 service, u16 command)
{
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	struct usb_port_struct *port;
	int ret = -1;
	int timeout = 1000;
	port = usb_ports[1];
#ifndef MODEM_WDT_SUPPORT
	if(usb_boot_data->chip_en < 0){
		skw_usb_err("chip_en = %d Invalid Pls check HW !!\n", usb_boot_data->chip_en);
		return ret;
	}
#endif
	if(port)
		skw_usb_info("(%d,%d) cp_state= %d\n",
			service, command, recovery->cp_state);
	if (recovery->cp_state)
		return ret;

	if(port && port->state) {
		skw_reinit_completion(download_done);
		ret = usb_control_msg(port->udev, usb_sndctrlpipe(port->udev, 0),
				VENDOR_MSG_SERVICE_CTRL, USB_DIR_OUT| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				service, command, NULL, 0, 1000);
	}
	if((command & 0x01) == SERVICE_START) {
		skw_usb_info("ret = %d\n", ret);
		complete(&loop_completion);
		start_service_flag = 1;
		wait_for_completion_interruptible_timeout(&download_done, msecs_to_jiffies(timeout + 1000*service));
		service_state_map |= (1<<service);
	} else {
		if(service==BT_SERVICE && modem_status==MODEM_ON)
			wait_for_completion_interruptible_timeout(&download_done, msecs_to_jiffies(1000));
		service_state_map &= ~(1<<service);
	}
	return ret;
}

static int skw_get_packet_count(u8 portno)
{
	struct usb_port_struct *port;
	int ret = -1;
	u16 *packet_count, size=2;

	port = usb_ports[portno];
	if(port && port->state) {
		ret = usb_control_msg(port->udev, usb_rcvctrlpipe(port->udev, 0),
				VENDOR_MSG_PACKET_COUNT, USB_DIR_IN| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				portno, 0, port->read_buffer, size, 1000);

		packet_count = (u16 *)port->read_buffer;
		if(ret < 0)
			skw_port_log(portno,"%s (%d,%d) ret = %d\n", __func__, portno, *packet_count, ret);
		if(ret==size)
			port->max_packet_count = *packet_count;
		else
			port->max_packet_count = MAX_PACKET_COUNT;
	}
	return ret;
}

void skw_usb_cp_log(int disable)
{
	struct usb_port_struct *port;
	int ret = -1;
	port = usb_ports[0];
	if(port && port->state) {
		ret = usb_control_msg(port->udev, usb_sndctrlpipe(port->udev, 0),
				VENDOR_MSG_LOG_SWITCH, USB_DIR_OUT| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				disable, 0, NULL, 0, 10);

		skw_usb_info("(disable=%d) ret = %d\n", disable, ret);
	}
	cp_log_status = disable;
}
static int skw_usb_bluetooth_log(int disable)
{
	struct usb_port_struct *port;
	int ret = -1;

	if (disable) {
		skw_usb_info("disable the BLUETOOTH log \n");
		disable = 0x2;
	} else {
		skw_usb_info(" enable the BLUETOOTH log !!\n");
		disable = 0x6;
	}
	port = usb_ports[0];
	if(port && port->state) {
		ret = usb_control_msg(port->udev, usb_sndctrlpipe(port->udev, 0),
				VENDOR_MSG_LOG_SWITCH, USB_DIR_OUT| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				disable, 0, NULL, 0, 10);
		skw_usb_info("(disable=%d) ret = %d\n", disable, ret);
	}
	return ret;
}
/************************************************************************
 *Decription:send BT start command to modem.
 *Author:jiayong.yang
 *Date:2021-08-30
 *Modfiy:
 *
 ********************************************************************* */
static int skw_BT_service_start(void)
{
	u16 cmd = SERVICE_START;

	if (!wifi_data_pdev)
		return -ENODEV;

	skw_usb_info("Enter modem_status=%d\n", modem_status);
	if (service_state_map & (1<<BT_SERVICE))
		return 0;

#if defined(CONFIG_SEEKWAVE_PLD_RELEASE) && defined(MODEM_WDT_SUPPORT)
	if (chip_en_gpio < 0){
		cmd |= MODEM_WDT_SUPPORT;
	}
#endif
	return send_modem_service_command(BT_SERVICE, cmd);
}

/************************************************************************
 *Decription:send BT stop command to modem.
 *Author:jiayong.yang
 *Date:2021-08-30
 *Modfiy:
 *
 ********************************************************************* */
static int skw_BT_service_stop(void)
{
	skw_usb_info("Enter modem_status=%d\n", modem_status);
	if (!wifi_data_pdev)
		return -ENODEV;

	if (service_state_map & (1<<BT_SERVICE)){
		return send_modem_service_command(BT_SERVICE, SERVICE_STOP);
	}
	return 0;
}
/************************************************************************
 *Decription:send WIFI start command to modem.
 *Author:jiayong.yang
 *Date:2021-08-30
 *Modfiy:
 *
 ********************************************************************* */
static int skw_WIFI_service_start(void)
{
	int count=90;
	u16 cmd = SERVICE_START;
	skw_usb_info("Enter STARTWIFI---modem_status=%d, 0x%x\n",
			modem_status, service_state_map);
	if (modem_status == MODEM_HALT) {
		while(!usb_ports[1] && count--)
			msleep(10);
	}
	if (service_state_map & (1<<WIFI_SERVICE))
		return 0;
	cmd |= WIFI_PORT_SHARE_FLAG;
#if defined(CONFIG_SEEKWAVE_PLD_RELEASE) && defined(MODEM_WDT_SUPPORT)
	if (chip_en_gpio < 0){
		cmd |= MODEM_WDT_SUPPORT;
	}
#endif
	return send_modem_service_command(WIFI_SERVICE, cmd);
}

/************************************************************************
 *Decription: send WIFI stop command to modem.
 *Author:jiayong.yang
 *Date:2021-08-30
 *Modfiy:
 *
 ********************************************************************* */
static int skw_WIFI_service_stop(void)
{
	int count=10;
	int portno;
	struct usb_port_struct *port;

	skw_usb_info("Enter,STOPWIFI--- modem status %d, 0x%x\n",
			modem_status, service_state_map);
	for (portno=0; portno<2; portno++) {
		port = usb_ports[portno];
		if (port && port->write_urb && port->write_urb->context) {
			usb_kill_anchored_urbs(&port->write_submitted);
		} else if (port && port->tx_urb_count) {
			usb_kill_anchored_urbs(&port->write_submitted);
		}
	}
	if (modem_status == MODEM_HALT) {
		service_state_map &= ~(1<<WIFI_SERVICE);
		while(!usb_ports[1] && count--)
			msleep(10);
		return 0;
	}
	if (service_state_map & (1<<WIFI_SERVICE))
		return send_modem_service_command(WIFI_SERVICE, SERVICE_STOP);
	return 0;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-08-30
 *Modfiy:
 *
 ********************************************************************* */
static void bulkin_complete(struct urb *urb)
{
	struct usb_port_struct *port;
	int portno;

	if(!urb)
		return;

	portno = usb_pipeendpoint(urb->pipe) - 1;
	port = usb_ports[portno];
	port->rx_done_time = jiffies;
	port->rx_packet_count++;
	if(urb) {
		if(urb->status) {
			skw_usb_info("endpoint%d actual = %d status %d\n",
				usb_pipeendpoint(urb->pipe), urb->actual_length, urb->status);
		}
		if(urb->status == -ENOENT && port && port!=log_port  && port->suspend)
			list_add_tail(&urb->urb_list, &port->suspend_urb_list);
		else if (urb->context)
			complete(urb->context);
	}
}
static void bulkin_async_complete(struct urb *urb)
{
	struct usb_port_struct *port;

	if(!urb)
		return;
	port = urb->context;
	if(!port)
		return;
	port->rx_done_time = jiffies;
	bulk_async_read--;
	if(urb->status) {
		skw_usb_info("endpoint%d actual = %d status %d\n",
			usb_pipeendpoint(urb->pipe), urb->actual_length, urb->status);
	}
	if(urb->status == -ENOENT && port && port->suspend)
		list_add_tail(&urb->urb_list, &port->suspend_urb_list);
	else if (port) {
		urb->context = NULL;
		spin_lock(&port->rx_urb_lock);
		if (port->state)
			list_add_tail(&urb->urb_list, &port->rx_done_urb_list);
		else
			list_add_tail(&urb->urb_list, &port->rx_urb_list);
		spin_unlock(&port->rx_urb_lock);
		if (port->state)
			tasklet_hi_schedule(&port->tasklet);
	}
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static void bulkout_complete(struct urb *urb)
{
	struct usb_port_struct *port;
	int portno;

	portno = usb_pipeendpoint(urb->pipe) - 1;
	port = usb_ports[portno];

	if(urb->status)
		skw_usb_info("endpoint%d actual = %d status %d\n",
			usb_pipeendpoint(urb->pipe),  urb->actual_length, urb->status);

	if (port) {
		port->tx_done_time = jiffies;
		port->sent_packet_count++;
	}
	if (urb->context)
		complete(urb->context);
}

static void bulkout_async_complete(struct urb *urb)
{
	struct usb_port_struct *port = urb->context;

	if(urb->status) {
		port->sent_packet_count += urb->num_sgs;
		if(urb->sg && port->adma_tx_callback)
			port->adma_tx_callback(port->portno, urb->sg, urb->num_sgs, port->tx_data, urb->status);
		else if(urb->transfer_buffer && port->sdma_tx_callback)
			port->sdma_tx_callback(port->portno, urb->transfer_buffer, urb->transfer_buffer_length, port->tx_data, urb->status);
		skw_usb_info("port%d endpoint%d actual = %d status %d\n",
			port->portno, usb_pipeendpoint(urb->pipe), urb->actual_length, urb->status);
	} else if(urb->sg && port->adma_tx_callback) {
		port->adma_tx_callback(port->portno, urb->sg, urb->num_sgs, port->tx_data, 0);
		port->sent_packet_count += urb->num_sgs;
	} else if(urb->transfer_buffer && port->sdma_tx_callback)
		port->sdma_tx_callback(port->portno, urb->transfer_buffer, urb->transfer_buffer_length, port->tx_data, 0);
	urb->context = NULL;
	port->tx_done_time = jiffies;
	spin_lock(&port->tx_urb_lock);
	list_add_tail(&urb->urb_list, &port->tx_urb_list);
	port->tx_urb_count--;
	if(port->tx_urb_count==0 && port->sent_packet_count!=port->req_tx_packet)
		skw_usb_info(" port[%d]= %d %d\n", port->portno, port->sent_packet_count, port->req_tx_packet);
	spin_unlock(&port->tx_urb_lock);
	wake_up_interruptible(&port->tx_wait);
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
int bulkin_read_timeout(int portno, char *buffer, int size, int *actual, int timeout)
{
	struct usb_port_struct *port;
	unsigned int pipe;
	int	ret;

	if(portno >= MAX_USB_PORT || !buffer || !size)
		return -EINVAL;
	port = usb_ports[portno];
	if(!port->state)
		return -EIO;
	if(actual)
		*actual = 0;
	pipe = usb_rcvbulkpipe(port->udev, port->epin->bEndpointAddress);
	ret = usb_bulk_msg(port->udev, pipe, buffer, size, actual,timeout);

	if(port == log_port && actual) {
		if(assert_info_print && assert_info_print<28 && *actual<100) {
			assert_info_print++;
			if(*actual > 4)
				skw_usb_info("%s", (char *)buffer);
		}
		if(*actual == 4)
			assert_info_print = 28;
	}
	if(ret)
		return ret;

	if(actual)
		return *actual;
	return ret;
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
int bulkout_write_timeout(int portno, char *buffer, int size, int *actual, int timeout)
{
	struct usb_port_struct *port;
	unsigned int pipe;
	int	ret;

	if(portno >= MAX_USB_PORT || !buffer || !size)
		return -EINVAL;
	port = usb_ports[portno];
	
	if(!port->state)
		return -EIO;
	if(actual)
		*actual = 0;
	pipe = usb_sndbulkpipe(port->udev, port->epout->bEndpointAddress);
	ret = usb_bulk_msg(port->udev, pipe, buffer, size, actual,timeout);
	if(ret)
		return ret;
	if(actual)
		return *actual;
	return ret;
}

static void kick_rx_thread(void)
{
	struct usb_port_struct *port;

	port = usb_ports[0];

	if (!port || !port->state)
		return;

	skw_usb_info("submitted urb %d\n", bulk_async_read);
	port = usb_ports[1];
	if ((bulk_async_read == 0) && port &&
		(!list_empty(&port->rx_urb_list)))
		bulkin_read_async(port);
	else if (port && list_empty(&port->rx_urb_list))
		skw_usb_info("urb list is empty \n");
}
/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int register_rx_callback(int id, void *func, void *para);
static int register_tx_callback(int id, void *func, void *para);
static struct sv6160_platform_data wifi_pdata = {
	.data_port = 0,
	.cmd_port = 1,
#ifdef CONFIG_SEEKWAVE_PLD_RELEASE
	.bus_type = USB_LINK|TX_SDMA|RX_SDMA|TX_ASYN|CP_RLS,
#else
	.bus_type = USB_LINK|TX_SDMA|RX_SDMA|TX_ASYN|CP_DBG,
#endif
	.max_buffer_size = MAX_BUFFER_SIZE,
	.align_value = 512,
	.hw_adma_tx = wifi_send_cmd,
	.hw_sdma_tx = send_data,
	.hw_adma_tx_async = wifi_send_cmd_async,
	.hw_sdma_tx_async = send_data_async,
	.callback_register = register_rx_callback,
	.modem_assert = modem_assert,
	.service_start = wifi_service_start,
	.service_stop = wifi_service_stop,
	.modem_register_notify = modem_register_notify,
	.modem_unregister_notify = modem_unregister_notify,
	.at_ops = {
		.port = 2,
		.open = open_usb_port,
		.close = close_usb_port,
		.read = recv_data,
		.write = send_data,
		.read_tm = bulkin_read_timeout,
		.write_tm = bulkout_write_timeout,
	},
	.tx_callback_register = register_tx_callback,
	.rx_thread_wakeup = kick_rx_thread,
};

void usb_handle(unsigned long tsk_data)
{
	int	size, read, ret;
	int  transfer_count = 0, sg_count, offset;
	u16  data_flag = 0x8000;
	unsigned long flags;
	char *buffer;
	int  *data;
	struct usb_port_struct *port = (struct usb_port_struct *) tsk_data;
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	struct scatterlist *sg;
	struct urb *urb;
	u16  pcount, sequence;

	if (!strncmp(skw_chipid, "SV6316", 6) || !strlen(skw_chipid)
            || !strncmp(skw_chipid, "SV6160LITE", 10))
		data_flag = 2;

	while(!list_empty(&port->rx_done_urb_list)) {

		if (!port->state || port!=usb_ports[1])
			break;
		spin_lock_irqsave(&port->rx_urb_lock, flags);
		urb = list_first_entry(&port->rx_done_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		list_add_tail(&urb->urb_list, &port->rx_urb_list);
		spin_unlock_irqrestore(&port->rx_urb_lock, flags);

		sg_init_table(sgs, nr_sgs);
		read = urb->actual_length;
		buffer = urb->transfer_buffer;
		transfer_count++;
		if(urb->status < 0 || !port->state) {
			skw_usb_err(" bulkin read status=%d state=%d\n", urb->status, port->state);
			return ;
		}
		if(port->rx_submit) {
			int is_cmd;
			u32 d32;

			data = (int *)buffer;
			d32 = data[0];
			sequence = d32 & 0xffff;
			pcount = d32 >> 16;
			pcount &= 0x7fff;
			offset = 0;
			sg_count = 0;
			sg = sgs;
			is_cmd = 0;
			if (d32 & 0x80000000) {
				if (transfer_index != sequence)
					skw_usb_info("data lost: recved %d, expect:%d\n", transfer_index,sequence);
				transfer_index = sequence + 1;
			}
			while (offset+12 < read) {
				sg_count++;
				if(sg_count > nr_sgs) {
					skw_usb_warn("packet count is overflow %d : %d : %d : %d!!!\n",
							offset, read, sg_count, nr_sgs);
					sg_count--;
					break;
				}
				size = data[2] >> 16;
				size += 3;
				size = size & 0xfffffffc;
				if(data[2] & data_flag) {
					if (sg_count > 1 && !is_cmd)
						size = -1;
					else
						is_cmd = 1;
				}
				if (size + offset > read || size > 2048 || size <= 12) {
					skw_usb_warn("Invalid packet size=%d: %d : %d :%d  0x%x:0x%x!!!\n",
							size, offset, read, sg_count, d32, data[2]);
					if (cls_recovery_mode_en) {
						print_hex_dump(KERN_ERR, "PACKET1::", 0, 16, 1,
								urb->transfer_buffer, offset+12, 1);
						modem_assert();
					}
					if (sg_count > 0)
						sg_count--;
					break;
				}
				sg_set_buf(sg,  &buffer[offset], size);
				sg++;
				offset  += size;
				if (is_cmd && !recovery->cp_state) {
					skw_usb_dbg("rx_submit(0x%x): command: 0x%x 0x%x: 0x%x 0x%x readlen=%d\n", (u32)jiffies,
							       	data[2], data[3], last_recv_wifi_ack[1], last_recv_wifi_ack[2], read);
					if ((data[3] & 0xff) == 0x10) {
						last_ack_time = jiffies;
						memcpy(last_recv_wifi_ack, &data[1], 12);
					} else
						memcpy(last_recv_wifi_evt, &data[1], 12);
				}
				data = (int *)&buffer[offset];
			}
			if (d32 & 0x80000000) {
				if (sg_count != pcount)
					skw_usb_info("packet lost:%d %d\n", sg_count, pcount);
			}
			if(sg_count >15)
				skw_usb_info("rx_submit: port%d packet count %d\n",
					port->portno, sg_count);
			if(is_cmd)
				port = usb_ports[wifi_pdata.cmd_port];
			else
				port = usb_ports[wifi_pdata.data_port];
			if (port->rx_submit)
				port->rx_submit(port->portno, sgs, sg_count, port->rx_data);
			port->rx_packet_count += sg_count;
			if (modem_status != MODEM_ON)
				return ;
			port = usb_ports[wifi_pdata.data_port];
		}
	}

	while(!list_empty(&port->rx_urb_list)) {
		if (port->state==0)
			break;
		ret = bulkin_read_async(port);
	}
}

/**********************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 **********************************************************************/
int usb_port_async_entry(void *para)
{
	struct usb_port_struct *port = para;
	struct sched_param param;
	unsigned long flags;
	//int	size, read, ret;
	int ret;
	u16	mpc;
	struct urb *urb;
	u16  data_flag = 0x8000;

	if(port->portno == 0) {
		param.sched_priority = USB_RX_TASK_PRIO;
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
		sched_set_fifo_low(current);
#else
		sched_setscheduler(current, SCHED_FIFO, &param);
#endif
	}
	if(port->max_packet_count)
		mpc = port->max_packet_count;
	else
		mpc = 2;

	if (!strncmp(skw_chipid, "SV6316", 6) || !strlen(skw_chipid)
            || !strncmp(skw_chipid, "SV6160LITE", 10))
		data_flag = 2;

	nr_sgs = mpc+1;
	sgs = kzalloc((nr_sgs)*sizeof(struct scatterlist), GFP_KERNEL);
	if (!sgs)
		return -ENOMEM;
	bulk_async_read = 0;
	if (mpc<=13)
		usb_port_alloc_recv_urbs(port, port->epin, 3, 20*1024);
	else
		usb_port_alloc_recv_urbs(port, port->epin, 3, 24*1024);
	usb_port_alloc_xmit_urbs(port, port->epout,10,0);
	msleep(300);
	transfer_index = 0;
	skw_usb_info(" port %d running packet %d %s 0x%x...\n",port->portno, mpc, skw_chipid, data_flag);
	if (!list_empty(&port->rx_urb_list)) {
		ret = bulkin_read_async(port);
	}

	wait_event_interruptible(port->rx_wait, (!port->state));
	skw_usb_info(" port %d stoped\n", port->portno);
	msleep(50);
	kfree(sgs);

	if(port->write_urb) {
		usb_kill_anchored_urbs(&port->write_submitted);
	}
	if(port->read_urb) {
		usb_kill_anchored_urbs(&port->read_submitted);
	}

	if(port->write_urb && port->write_urb->context)
		wait_for_completion_interruptible(port->write_urb->context);

	spin_lock_irqsave(&port->rx_urb_lock, flags);
	while(!list_empty(&port->rx_urb_list)) {
		urb = list_first_entry(&port->rx_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		if(urb->transfer_buffer)
			kfree(urb->transfer_buffer);
		usb_free_urb(urb);
	}
	spin_unlock_irqrestore(&port->rx_urb_lock, flags);
	while(!list_empty(&port->tx_urb_list)) {
		urb = list_first_entry(&port->tx_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		usb_free_urb(urb);
	}
	up(&port->sem);
	return 0;
}

static void skw_usb_kill_wifi_threads(struct usb_port_struct *p)
{
	int i;
	struct usb_port_struct *port;
	for(i=0; i<3; i++) {
		port = usb_ports[i];
		if(port==NULL)
			break;
		if(port && port->thread) {
			port->state = 0;
		}
	}
}
static void skw_usb_dump_memory(char *buffer, int size, int *log_size)
{
	if (log_port->state==2)
		return;
	if (size && buffer && log_size) {
		dump_memory_buffer = buffer;
		dump_buffer_size = size;
		dump_log_size = log_size;
		skw_usb_info("dump_memory : %p-%d\n", buffer, size);
		schedule_work(&dump_memory_worker);
	}
}

static void show_assert_context(void)
{
	int read;
	int error_count;
	int total_size;
	int dump_memory_size = 0;

	if(log_port && log_port->state!=2) {
		char *buffer;
		buffer = kzalloc(1024, GFP_KERNEL);
		if (!buffer)
			return;
		open_usb_port(log_port->portno, 0, 0);
		dump_memory_done = 0;
		error_count=0;
		total_size = 0;
		do {
			if (modem_status == MODEM_ON)
				break;
			if (!dump_memory_done)
				read = bulkin_read_timeout(log_port->portno, buffer, 1024, &read, 10);
			if (read > 0) {
				if (total_size + read < dump_buffer_size) {
					memcpy(&dump_memory_buffer[total_size], buffer, read);
					dump_memory_size = total_size + read;
				}
				total_size += read;
				memset(buffer, 0, read);
			}
			if(read == 4 || read < 0) {
				break;
			}
		} while (assert_info_print<100 && !dump_memory_done);
		while (!dump_memory_done) {
			if (modem_status == MODEM_ON)
				break;
			read = bulkin_read_timeout(log_port->portno, buffer, 1024, &read, 10);
			if (read <= 0) {
				error_count++;
				skw_usb_info("%s read = %d : total %d done=%d\n", current->comm, read, total_size, dump_memory_done);
				if(error_count >1)
					break;
			} else {
				if (total_size + read < dump_buffer_size) {
					memcpy(&dump_memory_buffer[total_size], buffer, read);
					dump_memory_size = total_size + read;
				}
				total_size += read;
			}
		}
		close_usb_port(log_port->portno);
		skw_usb_info("dump memory size: %d buffer_size: %d\n", dump_memory_size, dump_buffer_size);
		if (dump_log_size)
			*dump_log_size = dump_memory_size;
		kfree(buffer);
	}
}
static void dump_memory_work(struct work_struct *work)
{
	if(dump_log_size && *dump_log_size==0) {
		skw_usb_info(" running...\n");
		show_assert_context();
	}
}

int skw_pin_config(struct usb_port_struct *port, char *buffer)
{
	int i;
	u32 val;
	int func_index = 0;
	int g_offset = 0;
	u32 pin_group_index = 0;
	u32 func_group_index = 0;
	u32 func_sel_val[2] = {0};
	u32 func_sel_off[] = {PN_FUNC_SEL0_OFFSET, PN_FUNC_SEL1_OFFSET};
	u32 *pin_val;
	u8 pin_off[PN_CNT] = {0};
	char value_str[9];
	char result_str[15]; //e.g. PN:10:12345678
	char off_str[3];

	pin_val = (u32 *)kzalloc(PN_CNT * sizeof(u32), GFP_KERNEL);

	while (g_offset < usb_boot_data->nv_mem_pnfg_size) {
		//1. pin offset
		pin_off[pin_group_index] = usb_boot_data->nv_mem_pnfg_data[g_offset];

		//2. func_sel value
		val = usb_boot_data->nv_mem_pnfg_data[g_offset + 1];
		func_sel_val[func_group_index] |= (val << (3 * func_index)) & (7 << (3 * func_index));

		//3. pin config value
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 2] << BIT_PN_DSLP_EN_START) & \
				GENMASK(BIT_PN_DSLP_EN_END, BIT_PN_DSLP_EN_START);//dslp_en
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 3] << BIT_DRV_STREN_START) & \
				GENMASK(BIT_DRV_STREN_END, BIT_DRV_STREN_START);//drv_strength
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 4] << BIT_NORMAL_WP_START) & \
				GENMASK(BIT_NORMAL_WP_END, BIT_NORMAL_WP_START);//normal:wpu/wpd
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 5] << BIT_SCHMITT_START) & \
				GENMASK(BIT_SCHMITT_END, BIT_SCHMITT_START);//schmitt trigger enable
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 6] << BIT_SLP_WP_START) & \
				GENMASK(BIT_SLP_WP_END, BIT_SLP_WP_START);//sleep:wpu/wpd
		pin_val[pin_group_index] |=
				((u32)usb_boot_data->nv_mem_pnfg_data[g_offset + 7]) & \
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
		sprintf(value_str, "%08x", func_sel_val[i]);
		sprintf(off_str, "%02x", func_sel_off[i]);
		sprintf(result_str, "PN:%s:%s", off_str, value_str);
		skw_usb_dbg("func_sel[%d]:%s\n", i, result_str);
		memcpy(buffer+256, result_str, 14);
		bulkout_write(port, buffer+256, 14);
	}

	//pin config
	for (i = 0;i < PN_CNT;i++) {
		sprintf(value_str, "%08x", pin_val[i]);
		sprintf(off_str, "%02x", pin_off[i]);
		sprintf(result_str, "PN:%s:%s", off_str, value_str);
		skw_usb_dbg("pin_conf[%d]:%s\n", i, result_str);
		memcpy(buffer+256, result_str, 14);
		bulkout_write(port, buffer+256, 14);
	}
	kfree(pin_val);

	return 0;
}

static int usb_loopcheck_entry(void *para)
{
	struct usb_port_struct *port = para;
	char *buffer;
	int read, size;
	int count= 0, timeout=300;
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	size = 512;
	buffer = kzalloc(size, GFP_KERNEL);
	schedule_delayed_work(&skw_except_work , msecs_to_jiffies(6000));
	while(port->state && buffer){
		read = 0;
		memset(buffer,0,512);
		do{
			if(port->state==0)
				break;
			read = bulkin_read(port, buffer, 256);
		}while(!read);

		if (port->suspend) {
			msleep(500);
			continue;
		}
		if(read < 0 || !port->state) {
			skw_usb_err("bulkin read_len=%d\n",read);
			break;
		}
		if(strncmp(buffer, "BSPREADY", read))
			skw_usb_info("recv(%d):%s %s\n", read, skw_chipid, buffer);
		memcpy(buffer+256, "LOOPCHECK", 9);
		if (read==8 && !strncmp(buffer, "BSPREADY", read)) {
			if (start_service_flag)
				continue;
			bulkout_write(port, buffer+256, 9);
			//bulkout_write_timeout(port->portno, buffer+256,9, &size, 300);
		} else if (read==9 && !strncmp(buffer, "WIFIREADY", read)) {
			start_service_flag = 0;
			service_state_map |= (1<<WIFI_SERVICE);
			complete(&download_done);
			bulkout_write(port, buffer+256, 9);
			//bulkout_write_timeout(port->portno, buffer+256,9, &size, 300);
		} else if (read==6 && !strncmp(buffer, "BTEXIT", read)) {
			complete(&download_done);
			//bulkout_write(port, buffer+256, 9);
			bulkout_write_timeout(port->portno, buffer+256,9, &size, 300);
		} else if (read==7 && !strncmp(buffer, "BTREADY", read)) {
			start_service_flag = 0;
			service_state_map |= (1<<BT_SERVICE);
			complete(&download_done);
			bulkout_write(port, buffer+256, 9);
			//bulkout_write_timeout(port->portno, buffer+256,9, &size, 300);
		} else if (!strncmp(buffer, "BSPASSERT", 9)) {
			sprintf(firmware_version, "%s\n%s\n", firmware_version, buffer);
			skw_usb_err("cmd:0x%x 0x%x 0x%x ack:%x %x:%x event:0x%x:0x%x:0x%x time:0x%x:0x%x:0x%x\n",
			       last_sent_wifi_cmd[0],last_sent_wifi_cmd[1],last_sent_wifi_cmd[2],
			       last_recv_wifi_ack[0],last_recv_wifi_ack[1],last_recv_wifi_ack[2],
			       last_recv_wifi_evt[0],last_recv_wifi_evt[1],last_recv_wifi_evt[2],
			       (u32)jiffies,(u32)last_sent_time, (u32)last_ack_time);
			if(recovery->cp_state==1)
				cancel_delayed_work_sync(&skw_except_work);

			mutex_lock(&recovery->except_mutex);
			if(recovery->cp_state==DEVICE_BLOCKED_EVENT){
				mutex_unlock(&recovery->except_mutex);
				break;
			}
			recovery->cp_state = 1;
			mutex_unlock(&recovery->except_mutex);

			assert_info_print = 1;
			memset(buffer, 0, read);
			skw_usb_kill_wifi_threads(port);
			modem_status = MODEM_HALT;
			modem_notify_event(DEVICE_ASSERT_EVENT);
			if (log_port->state!=2)
				schedule_work(&dump_memory_worker);
			memset(buffer, 0, 256);
			read = bulkin_read_timeout(port->portno, buffer, 256, &read, 1000);
			if (read > 0)
				skw_usb_info("bspassert after recv(%d): %s\n", read, buffer);
			dump_memory_done = 1;
			modem_notify_event(DEVICE_DUMPDONE_EVENT);
			msleep(100);
			skw_recovery_mode();
			service_state_map =0;

			break;
		} else if (!strncmp("trunk_W", buffer, 7)) {
#ifdef CONFIG_SKW_DL_TIME_STATS
			last_time = ktime_get();
			skw_usb_info(",the download time start time %llu and lasttime %llu ,lose_time=%llu\n",
				cur_time, last_time,(last_time-cur_time));
#endif
			cancel_delayed_work_sync(&skw_except_work);
			recovery->cp_state = 0;
			assert_info_print = 0;
			modem_status = MODEM_ON;
			memset(firmware_version, 0 , sizeof(firmware_version));
			strncpy(firmware_version, buffer, read);

			modem_notify_event(DEVICE_BSPREADY_EVENT);
			count = 0;
			schedule_work(&add_device_work);
			bulkout_write_timeout(port->portno, buffer+256,9, &size, 300);
			if (usb_boot_data->nv_mem_pnfg_data != NULL && usb_boot_data->nv_mem_pnfg_size != 0) {
				skw_usb_info("UPDATE '%s' PINCFG from %s\n", skw_chipid, usb_boot_data->skw_nv_name);
				skw_pin_config(port, buffer);
			}
		}
		wait_for_completion_interruptible_timeout(&loop_completion, msecs_to_jiffies(timeout));
		skw_reinit_completion(loop_completion);
	}
	skw_usb_info(" -port%d is stopped\n", port->portno);
	if(port->read_urb && port->read_urb->context) {
		usb_kill_anchored_urbs(&port->read_submitted);
	}

	if(port->write_urb && port->write_urb->context) {
		usb_kill_anchored_urbs(&port->write_submitted);
		if(port->write_urb->context)
			wait_for_completion_interruptible(port->write_urb->context);
	}
	kfree(buffer);
	up(&port->sem);
	return 0;
}

static int usb_bt_rx_entry(void *para)
{
	struct usb_port_struct *port = para;
	char *buffer;
	int read, size;

	size = 2048;
	buffer = kzalloc(size, GFP_KERNEL);
	while(port->state==2 && buffer){
		read = 0;
		memset(buffer,0,size);
		do{
			if(port->state != 2)
				break;
			read = bulkin_read(port, buffer, size);
		}while(!read);

		if(read < 0) {
			skw_usb_err("bulkin read_len=%d\n",read);
			break;
		}
		if(port->rx_submit)
			port->rx_submit(port->portno, port->rx_data, read, buffer);
	}
	skw_usb_info("-port%d is stopped\n", port->portno);
	if(port->write_urb && port->write_urb->context) {
		usb_kill_anchored_urbs(&port->write_submitted);
	}
	if(port->read_urb && port->read_urb->context) {
		usb_kill_anchored_urbs(&port->read_submitted);
	}

	if(buffer)
		kfree(buffer);
	up(&port->sem);
	return 0;
}


/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static struct sv6160_platform_data ucom_pdata = {
	.max_buffer_size =0x800,
	.bus_type = USB_LINK,
	.hw_sdma_tx = send_data,
	.hw_sdma_rx = recv_data,
	.open_port = open_usb_port,
	.close_port = close_usb_port,
	.modem_assert = modem_assert,
	.service_start = bt_service_start,
	.service_stop = bt_service_stop,
	.modem_register_notify = modem_register_notify,
	.modem_unregister_notify = modem_unregister_notify,
	.dump_modem_memory = skw_usb_dump_memory,
	.bluetooth_log_disable = skw_usb_bluetooth_log,
};

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */

static int register_rx_callback(int id, void *func, void *para)
{
	if(id >= MAX_USB_PORT)
		return -EINVAL;

	if(usb_ports[id] == NULL)
		return -EIO;
	if(func && !usb_ports[id]->rx_submit) {
		usb_ports[id]->rx_submit = func;
		usb_ports[id]->rx_data = para;
		if(id==1)
			skw_WIFI_service_start();
		return 0;
	} else if(!func && usb_ports[id]->rx_submit) {
		if(id==1)
			skw_WIFI_service_stop();

		usb_ports[id]->rx_submit = func;
		usb_ports[id]->rx_data = para;
		return 0;
	}
	if(wifi_pdata.bus_type & TX_ASYN) {
		if(wifi_pdata.bus_type & TX_SDMA)
			usb_ports[id]->sdma_tx_callback = func;
		else
			usb_ports[id]->adma_tx_callback = func;
	}
	usb_ports[id]->tx_data = para;
	return 0;
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int register_tx_callback(int id, void *func, void *para)
{
	if(id >= MAX_USB_PORT)
		return -EINVAL;

	if(usb_ports[id] == NULL)
		return -EIO;
	if(wifi_pdata.bus_type & TX_ASYN) {
		if(wifi_pdata.bus_type & TX_SDMA)
			usb_ports[id]->sdma_tx_callback = func;
		else
			usb_ports[id]->adma_tx_callback = func;
	}
	usb_ports[id]->tx_data = para;
	return 0;
}


/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_usb_io_probe(struct usb_interface *interface,
				const struct usb_device_id *id)
{
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	struct usb_port_struct *port;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *epd;
	struct platform_device *pdev;
	struct usb_device *udev = interface_to_usbdev(interface);
	char	pdev_name[32], names[32];
	int	i, ret, dloader=0;

	memset(names, 0 ,sizeof(names));
	iface_desc = interface->cur_altsetting;
	if (iface_desc->string == NULL)
		return -EINVAL;
	sprintf(names, "%s", iface_desc->string);

	if (!strncmp(names, "Boot", 4))
		dloader = 1;
	if (!strlen(skw_chipid)) {
		memcpy(skw_chipid, udev->product, SKW_CHIP_ID_LENGTH);
	}
	skw_usb_info(" descriptor.idProduct = 0x%x CHIP_ID= %.100s\n",
		     udev->descriptor.idProduct, udev->product);
#ifndef CONFIG_SV6160_LITE_FPGA
	if ((udev->descriptor.idProduct != 0x6316) && !dloader
		&& chip_en_gpio < 0 && modem_status == MODEM_OFF) {
#ifndef MODEM_WDT_SUPPORT
		return -EINVAL;
#endif
	}
#endif
	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	mutex_lock(&recovery->except_mutex);
	if (usb_bus_num == 0xff && usb_port_num == 0xff) {
		skw_usb_info("bus[%x].port[%d]\n", udev->bus->busnum, udev->portnum);
		usb_bus_num = udev->bus->busnum;
		usb_port_num = udev->portnum;
	} else if (usb_bus_num != udev->bus->busnum || usb_port_num != udev->portnum) {
		mutex_unlock(&recovery->except_mutex);
		if (port)
			kfree(port);
		return -EBUSY;
	}
	mutex_unlock(&recovery->except_mutex);

	pdev = NULL;
	if (!strncmp(names, "WIFITCMD", 8))
		wifi_port_share = 1;
	usb_ports[iface_desc->desc.bInterfaceNumber] = port;
	INIT_LIST_HEAD(&port->rx_urb_list);
	INIT_LIST_HEAD(&port->tx_urb_list);
	INIT_LIST_HEAD(&port->rx_done_urb_list);
	INIT_LIST_HEAD(&port->suspend_urb_list);
	spin_lock_init(&port->rx_urb_lock);
	spin_lock_init(&port->tx_urb_lock);
	port->tx_urb_count = 0;
	init_waitqueue_head(&port->rx_wait);
	init_waitqueue_head(&port->tx_wait);
	if(dloader)
		dloader = 1;
	else if (iface_desc->desc.bInterfaceNumber == 1) {
		if (!strncmp(skw_chipid, "SV6160LITE", 10)) {
#ifdef SV6621S_WIRELESS
			sprintf(pdev_name, "%s%d", SV6621S_WIRELESS,
				iface_desc->desc.bInterfaceNumber);
#else
			sprintf(pdev_name, "%s%d", SV6160_WIRELESS,
				iface_desc->desc.bInterfaceNumber);
#endif
		} else if (!strncmp(skw_chipid, "SV6160", 6)) {
			sprintf(pdev_name, "%s%d", SV6160_WIRELESS,
				iface_desc->desc.bInterfaceNumber);
		} else if (!strncmp(skw_chipid, "SV6316", 6)) {
			sprintf(pdev_name, "%s%d", SV6316_WIRELESS,
				iface_desc->desc.bInterfaceNumber);
		} else {
			skw_usb_err(
				"unknow chip id!!! pls check you porting code!! and the connect the seekwave!!\n");
			if (skw_chipid)
				skw_usb_err("unknow chip id: %s\n", skw_chipid);

			sprintf(pdev_name, "%s%d", SV6316_WIRELESS,
				iface_desc->desc.bInterfaceNumber);
		}
		if(!wifi_data_pdev)
			pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
		else
			pdev = wifi_data_pdev;
		if(!pdev)
			return -ENOMEM;
	} else {
#ifdef CONFIG_BT_SEEKWAVE
		if (!strncmp(names, "DATA", 4)) {
			ucom_pdata.data_port = 0;
		} else if(!strncmp(names, "BTDATA", 6))
			ucom_pdata.data_port = iface_desc->desc.bInterfaceNumber;
		else if(!strncmp(names, "BTCMD", 5))
			ucom_pdata.cmd_port = iface_desc->desc.bInterfaceNumber;
		else if(!strncmp(names, "BTISOC", 6)) {
			ucom_pdata.audio_port = iface_desc->desc.bInterfaceNumber;
			sprintf(pdev_name, "%s", "btseekwave");
			ucom_pdata.port_name = "BTHCI";
			bluetooth_pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
			if (!bluetooth_pdev)
				return -ENOMEM;
			bluetooth_pdev->dev.parent = &udev->dev;
			bluetooth_pdev->dev.dma_mask = &port_dmamask;
			bluetooth_pdev->dev.coherent_dma_mask = port_dmamask;
			bt_audio_port = iface_desc->desc.bInterfaceNumber;
			memcpy(ucom_pdata.chipid, skw_chipid, SKW_CHIP_ID_LENGTH);
			ret = platform_device_add_data(bluetooth_pdev, &ucom_pdata, sizeof(ucom_pdata));
			if(ret) {
				skw_usb_err("failed to add platform data \n");
				platform_device_put(pdev);
				kfree(port);
				return ret;
			}
			skw_usb_info("add the bt devices \n");
		}else if(!strncmp(names, "AUDIO", 5)) {
			ucom_pdata.audio_port = 0;
			sprintf(pdev_name, "%s", "btseekwave");
			ucom_pdata.port_name = "BTHCI";
			bluetooth_pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
			if (!bluetooth_pdev)
				return -ENOMEM;
			bluetooth_pdev->dev.parent = &udev->dev;
			bluetooth_pdev->dev.dma_mask = &port_dmamask;
			bluetooth_pdev->dev.coherent_dma_mask = port_dmamask;
			bt_audio_port = iface_desc->desc.bInterfaceNumber;
			memcpy(ucom_pdata.chipid, skw_chipid, SKW_CHIP_ID_LENGTH);
			ret = platform_device_add_data(bluetooth_pdev, &ucom_pdata, sizeof(ucom_pdata));
			if(ret) {
				skw_usb_err("failed to add platform data \n");
				platform_device_put(pdev);
				kfree(port);
				return ret;
			}
		} else
#endif
		if (iface_desc->desc.bInterfaceNumber && strncmp(names, "LOOP", 4)) {
			sprintf(pdev_name, "%s", "skw_ucom");
			ucom_pdata.port_name = iface_desc->string;
			ucom_pdata.data_port = iface_desc->desc.bInterfaceNumber;
			memcpy(ucom_pdata.chipid, skw_chipid, SKW_CHIP_ID_LENGTH);
			pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
			if(!pdev)
				return -ENOMEM;
		}
	}
	if(!dloader) {
		if (1==iface_desc->desc.bInterfaceNumber && wifi_data_pdev) {
			struct sv6160_platform_data *pdata;
			pdev = wifi_data_pdev;
			//pdev->dev.parent = NULL;
			pdata = pdev->dev.platform_data;
			if (pdata) {
				pdata->align_value = iface_desc->endpoint[0].desc.wMaxPacketSize;
				wifi_pdata.align_value = iface_desc->endpoint[0].desc.wMaxPacketSize;
			}
			port->pdev = pdev;
		} else if (iface_desc->desc.bInterfaceNumber && pdev) {
			if (1==iface_desc->desc.bInterfaceNumber &&
			    usb_boot_data && usb_boot_data->pdev) {
				pdev->dev.parent = &usb_boot_data->pdev->dev;
			} else
				pdev->dev.parent = &udev->dev;
			pdev->dev.dma_mask = &port_dmamask;
			pdev->dev.coherent_dma_mask = port_dmamask;

			if(iface_desc->desc.bInterfaceNumber == 1) {
				wifi_pdata.align_value = iface_desc->endpoint[0].desc.wMaxPacketSize;
				if(usb_boot_data && usb_boot_data->iram_dl_size >0x50000)
					wifi_pdata.at_ops.port = 4;
				else
					wifi_pdata.at_ops.port = 2;
				if(udev->config->string && !strncmp(udev->config->string, "ECOM", 4)) {
					wifi_pdata.bus_type &= ~TYPE_MASK;
					wifi_pdata.bus_type |= USB2_LINK;
				}
				ret = platform_device_add_data(pdev, &wifi_pdata, sizeof(wifi_pdata));
				modem_status = MODEM_ON;
			} else{
				memcpy(ucom_pdata.chipid, skw_chipid, SKW_CHIP_ID_LENGTH);
				ret = platform_device_add_data(pdev, &ucom_pdata, sizeof(ucom_pdata));
			}
			if(ret) {
				skw_usb_err("failed to add platform data \n");
				platform_device_put(pdev);
				kfree(port);
				return ret;
			}
			if(iface_desc->desc.bInterfaceNumber>1){
				ret = platform_device_add(pdev);
				if(ret) {
					skw_usb_err("failt to register platform device\n");
					platform_device_put(pdev);
					kfree(port);
					return ret;
				}
			}
			port->pdev = pdev;
		}
	}
	usb_set_intfdata(interface, port);

	port->interface = usb_get_intf(interface);
	port->udev = usb_get_dev(udev);
	/* register struct wcn_usb_intf */
	skw_usb_dbg("intf[%x] is registerred: ep count %d %s\n",
			iface_desc->desc.bInterfaceNumber,
			iface_desc->desc.bNumEndpoints,
			iface_desc->string);
	ret = -ENOMEM;
	for(i=0; i<iface_desc->desc.bNumEndpoints; i++) {

		epd = &iface_desc->endpoint[i].desc;
		port->buffer_size = 5120;
		port->ep_mps = epd->wMaxPacketSize;
		if(usb_endpoint_is_bulk_in(epd)) {
			port->epin = epd;
			port->read_urb = usb_alloc_urb(0, GFP_KERNEL);
			if(!port->read_urb)
				goto err0;
			if(iface_desc->desc.bInterfaceNumber > 1) {
				port->read_buffer = NULL;
				port->buffer_size = 0;
			} else {
				port->read_buffer = kzalloc(port->buffer_size , GFP_KERNEL);
				if(!port->read_buffer)
					goto err0;
			}
			usb_fill_bulk_urb(port->read_urb, udev,
				usb_rcvbulkpipe(udev, epd->bEndpointAddress),
				port->read_buffer, port->buffer_size,
				bulkin_complete, port);
			port->read_urb->context = NULL;
			init_usb_anchor(&port->read_submitted);
			skw_usb_dbg("BulkinEP = 0x%x rp=%p\n",
					epd->bEndpointAddress, port->read_buffer);
		} else if(usb_endpoint_is_bulk_out(epd)) {
			port->epout = epd;
			port->write_urb = usb_alloc_urb(0, GFP_KERNEL);
			if(!port->write_urb)
				goto err0;
			if(iface_desc->desc.bInterfaceNumber > 1) {
				port->write_buffer = NULL;
				port->buffer_size = 0;
			} else{
				port->write_buffer = kzalloc(port->buffer_size, GFP_KERNEL);
				if(!port->write_buffer)
					goto err0;
			}
			usb_fill_bulk_urb(port->write_urb, udev,
				usb_sndbulkpipe(udev, epd->bEndpointAddress),
				port->write_buffer, port->buffer_size, bulkout_complete,port);
			port->write_urb->context = NULL;
			init_usb_anchor(&port->write_submitted);
			skw_usb_dbg("BulkoutEP = 0x%x wp =%p context %p\n",
					epd->bEndpointAddress, port->write_buffer, port->write_urb->context);
		}
	}
	if(!dloader) {
		port->portno = iface_desc->desc.bInterfaceNumber;
		port->state = 1;
		if (port->portno<=1) {
			if (!strncmp(names, "WIFIDATA", 8)) {
				skw_get_packet_count(port->portno);
				wifi_pdata.cmd_port = 1 - port->portno;
				wifi_pdata.data_port = port->portno;
				port->thread = kthread_create(usb_port_async_entry, port, iface_desc->string);

				tasklet_init(&port->tasklet, usb_handle, (unsigned long) port);
			} else {
				wifi_pdata.cmd_port = port->portno;
				wifi_pdata.data_port = 1 - port->portno;
			}
			if(port->thread) {
				sema_init(&port->sem, 0);
				wake_up_process(port->thread);
			} else
				sema_init(&port->sem, 1);
		} else if(!strncmp(names, "LOOP", 4)) {
			sema_init(&port->sem, 0);
			port->thread = kthread_create(usb_loopcheck_entry, port, iface_desc->string);
			if (port->thread)
				wake_up_process(port->thread);
		} else	sema_init(&port->sem, 1);
	} else {
		port->state = 1;
		assert_info_print = 0;
		INIT_WORK(&port->work, dloader_work);
		if (usb_boot_data &&
		    usb_boot_data->iram_dl_size &&
		    usb_boot_data->dram_dl_size) {
			skw_usb_info("schedule boot-work: 0x%x:0x%x\n",
				usb_boot_data->dram_dl_size,usb_boot_data->iram_dl_size);
			schedule_work(&port->work);
			modem_status = MODEM_ON;
		}
		port->is_dloader = 1;
		if (!usb_boot_data) {
			seekwave_boot_init(skw_chipid);
		}
	}
	if (!strncmp(names, "LOG", 3))
		log_port = port;
	return 0;
err0:
	skw_usb_err("no memory  to register device\n");
	if(port->write_buffer)
		kfree(port->write_buffer);
	if(port->read_buffer)
		kfree(port->read_buffer);
	if(port->write_urb)
		usb_free_urb(port->write_urb);
	if(port->read_urb)
		usb_free_urb(port->read_urb);
	if(port->pdev)
		platform_device_unregister(port->pdev);
	usb_ports[iface_desc->desc.bInterfaceNumber] = NULL;
	kfree(port);
	return ret;
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int launch_download_work(char *data, int size,int addr)
{
	int chk_ports =0;
	firmware_size = size;//link to usb_download size
	firmware_data	= data;//link to usb_download dl_data
	firmware_addr = addr;
	do{
		if((usb_ports[0] !=NULL)&&(usb_ports[0]->state)) {
			chk_ports = 1;
			break;
		}
		msleep(10);
	}while(!chk_ports);
	schedule_work(&usb_ports[0]->work);
	return 0;
}


static int skw_recovery_mode(void)
{
	int ret=0;
	skw_usb_info("[+]\n");
#ifndef MODEM_WDT_SUPPORT
	if(!cls_recovery_mode_en){
		ret = skw_reset_bus_dev();
	} else {
	    skw_usb_info("No Need To Recovery!\n");
	}
#endif
	skw_usb_info("[-]\n");
	return ret;
}

void get_bt_antenna_mode(char *mode)
{
}

void reboot_to_change_bt_antenna_mode(char *mode)
{
}

void get_USB_speed_mode(char *mode)
{
}

void reboot_to_change_USB_speed_mode(char *mode)
{
}

void reboot_to_change_bt_uart1(char *mode)
{
}
static irqreturn_t skw_gpio_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}
/************************************************************************
 *Decription:
 *Author:JUNWEI.JIANG
 *Date:2021-12-20
 *Modfiy:
 *
 ********************************************************************* */
int skw_boot_loader(struct seekwave_device *boot_data)
{
	int ret = 1;

	if (usb_ports[0] && usb_ports[0]->suspend)
		return -EOPNOTSUPP;
	usb_boot_data= boot_data;
	skw_usb_info("status:%d , chip_en_gpio=%d, gpio_in=%d", modem_status,
		       	usb_boot_data->chip_en, usb_boot_data->gpio_in);
	chip_en_gpio = usb_boot_data->chip_en;
#ifdef CONFIG_SKW_DL_TIME_STATS
	cur_time = ktime_get();
#endif
	if (host_wake_gpio < 0 && usb_boot_data->gpio_in>=0) {
		int irq_num;

		host_wake_gpio = usb_boot_data->gpio_in;
		irq_num = gpio_to_irq(host_wake_gpio);		
		ret = request_irq(irq_num, skw_gpio_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "skw-gpio-irq", NULL);
		skw_usb_info("request_gpio_irq ret=%d\n", ret);
		if (ret == 0)
			enable_irq_wake(irq_num);
	}
	if(!boot_data->first_dl_flag ){
		if (usb_ports[0] && !usb_ports[0]->is_dloader) {
			//usb_setup_service_devices();
			schedule_work(&add_device_work);
		} else if(boot_data->iram_img_data !=NULL && boot_data->dram_img_data!=NULL){
			skw_usb_info("USB FIRST BOOT... \n");
			ret=launch_download_work(boot_data->iram_img_data,boot_data->iram_dl_size,boot_data->iram_dl_addr);
		}else{
			skw_usb_info("The CPBOOT not download from AP!!!!\n");
		}
	}
	if(boot_data->dl_module==RECOVERY_BOOT){
		skw_recovery_mode();
		return 0;
	}
	if(boot_data->service_ops==SKW_WIFI_START){
		//skw_WIFI_service_start();
		//skw_usb_info("----WIFI-SERVICE-----START!!!\n");
	}else if(boot_data->service_ops== SKW_WIFI_STOP &&
			(service_state_map & (1<<WIFI_SERVICE))){
		skw_WIFI_service_stop();
		//skw_usb_info("----WIFI-SERVICE-----STOP!!!\n");
	}else if(boot_data->service_ops == SKW_BT_START){
		skw_usb_info("----BT-SERVICE-----START!!!\n");
		ret=skw_BT_service_start();
	}else if(boot_data->service_ops==SKW_BT_STOP &&
			(service_state_map & (1<<BT_SERVICE))){
		skw_usb_info("----BT-SERVICE-----STOP!!!\n");
		ret=skw_BT_service_stop();
	}
	if(ret < 0)
		return -1;
	else
		return 0;
}
void *skw_get_bus_dev(void)
{
	int time_count=0;
	if(modem_status == MODEM_OFF && !usb_ports[0]) {
		//skw_reset_bus_dev();//force reset usb bus no chipen or chipen can`t ops
		skw_usb_err(" power on USB\n");
		do{
			msleep(10);
			time_count++;
		}while(!usb_ports[0] && time_count < 50);
	}
	if(!usb_ports[0] || !usb_ports[0]->state || !usb_ports[0]->udev){
		skw_usb_err(" the port open device fail !!!\n");
		return NULL;
	}
	return &usb_ports[0]->udev->dev;
}

/************************************************************************
 *Decription:check dev ready for boot
 *Author:junwei.jiang
 *Date:2022-06-07
 *Modfiy:
 *
 ********************************************************************* */
int skw_reset_bus_dev(void)
{
	struct usb_port_struct *port;
	int ret = -1;

#ifndef MODEM_WDT_SUPPORT 
	if(chip_en_gpio >= 0) {
		skw_chip_power_reset();
		return 0;
	}
#endif
	port = usb_ports[0];
	if (port == NULL)
		return 0;

	ret = usb_control_msg(port->udev, usb_sndctrlpipe(port->udev, 0),
			VENDOR_MSG_MODEM_RESET, USB_DIR_OUT| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
			0,0,NULL,0,100);
	skw_usb_info("ret = %d\n", ret);
	if (ret == -ETIMEDOUT) {
		usb_reset_device(port->udev);
	}
	return ret;
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_usb_io_free_suspend_urbs(struct usb_interface *interface)
{
	struct usb_port_struct *port;
	struct urb *urb;

	port = usb_get_intfdata(interface);

	port->suspend = 0;
	while(!list_empty(&port->suspend_urb_list)) {
		urb = list_first_entry(&port->suspend_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		if (!list_empty(&port->suspend_urb_list))
			list_add_tail(&urb->urb_list, &port->rx_urb_list);
		else {
			urb->status = -EIO;
			urb->complete(urb);
		}
	}
	return 0;
}

static void skw_usb_io_disconnect(struct usb_interface *interface)
{
	int infno = interface->cur_altsetting->desc.bInterfaceNumber;
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();
	struct usb_port_struct *port;
	unsigned long flags;
	struct urb *urb;

	port = usb_get_intfdata(interface);
	if(!port)
		return;
	log_port = NULL;
	port->state = 0;
	skw_usb_info("interface[%x] disconnected %d\n", infno, modem_status);
	if(!port->is_dloader) {
		if(!recovery->cp_state && !infno)
			modem_notify_event(DEVICE_DISCONNECT_EVENT);
		if(port->read_urb && port->read_urb->context)
			usb_kill_anchored_urbs(&port->read_submitted);
		if(port->write_urb && port->write_urb->context)
			usb_kill_anchored_urbs(&port->write_submitted);
		if (infno > 1)
			platform_device_unregister(port->pdev);
		if (infno == 1) {
			wake_up_interruptible(&port->rx_wait);
			wake_up_interruptible(&port->tx_wait);
			if (modem_status==MODEM_ON) {
				if(wifi_data_pdev && &port->udev->dev == wifi_data_pdev->dev.parent) {
					platform_device_unregister(wifi_data_pdev);
					wifi_data_pdev = NULL;
					skw_usb_info("WIFI device disconnected1!!!\n");
				}
			}
		}
		if (port->pdev == wifi_data_pdev && port->suspend) {
			tasklet_kill(&port->tasklet);
		}
		skw_usb_io_free_suspend_urbs(interface);
		if(port->thread && !port->suspend&& down_timeout(&port->sem, 1000))
			skw_usb_info("start  to unregister interface[%x]\n", infno);
	} else {
		flush_work(&port->work);
	}

	if(port->read_urb && !port->read_urb->context) {
		kfree(port->read_urb);
		port->read_urb = NULL;
	} else skw_usb_err(" memory leak port.r%d!!!!!!!!\n", infno);
	if(port->write_urb && !port->write_urb->context) {
		kfree(port->write_urb);
		port->write_urb = NULL;
	} else skw_usb_err(" memory leak port.w%d!!!!!!!!\n", infno);
	if(port->read_buffer)
		kfree(port->read_buffer);
	if(port->write_buffer)
		kfree(port->write_buffer);
	spin_lock_irqsave(&port->rx_urb_lock, flags);
	while(!list_empty(&port->rx_done_urb_list)) {
		urb = list_first_entry(&port->rx_done_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		if(urb->transfer_buffer)
			kfree(urb->transfer_buffer);
		usb_free_urb(urb);
	}
	spin_unlock_irqrestore(&port->rx_urb_lock, flags);
	usb_ports[infno] = NULL;
	usb_set_intfdata(interface, NULL);
	usb_put_dev(port->udev);
	usb_put_intf(interface);
	kfree(port);
	if (chip_en_gpio >= 0 && MODEM_DOWNLOAD_FAILED == modem_status) {
		modem_status = MODEM_HALT;
		msleep(50);
		gpio_set_value(chip_en_gpio, 1);
		skw_usb_info("retry to boot device\n");
	}
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_usb_io_pre_reset(struct usb_interface *interface)
{
	/* there is a lock to prevent we reset a interface when
	 * urb submit
	 */
	struct usb_port_struct *port;

	port = usb_get_intfdata(interface);

	return 0;
}

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static int skw_usb_io_post_reset(struct usb_interface *interface)
{
	struct usb_port_struct *port;

	port = usb_get_intfdata(interface);
	return 0;
}
#ifdef CONFIG_PM
static int skw_usb_io_suspend(struct usb_interface *interface, pm_message_t message)
{
	struct usb_port_struct *port;
	struct recovery_data *recovery = SKW_USB_GET_RECOVERY_DATA();

	port = usb_get_intfdata(interface);

	if(usb_ports[1] == port) {
		int ret;
		u16 *count = (u16 *)port->read_buffer;
		
		modem_notify_event(DEVICE_SUSPEND_EVENT);
		ret = usb_control_msg(port->udev, usb_rcvctrlpipe(port->udev, 0),
				VENDOR_MSG_MODEM_SUSP, USB_DIR_IN| USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				1, 0, port->read_buffer, 2, 10);
		skw_usb_info("RET = %d  packet suspended = %d\n", ret, *count);
		if (*count)
			msleep(10);
	}
	if (port->tx_urb_count)
		usb_kill_anchored_urbs(&port->write_submitted);
	port->suspend = 1;

	if(port->portno == 1 || port->read_urb->context)
		usb_kill_anchored_urbs(&port->read_submitted);
	if(port->write_urb->context)
		usb_kill_anchored_urbs(&port->write_submitted);
	if (port->portno==0 && recovery->cp_state) {
		recovery->cp_state = 0;
		cancel_delayed_work_sync(&skw_except_work);
	}
#if KERNEL_VERSION(3, 2, 0) <= LINUX_VERSION_CODE
	skw_usb_info("port%d %s MSG\n", port->portno, PMSG_IS_AUTO(message)? "Auto":"None-auto");
#else
	skw_usb_info("port%d MSG not supported print!!\n", port->portno);
#endif
	return 0;
}
static int skw_usb_io_resume(struct usb_interface *interface)
{
	int	 retval = -1;
	struct usb_port_struct *port;
	struct urb *urb;
	port = usb_get_intfdata(interface);

	skw_usb_info("port%d enter...\n", port->portno);
	while(!list_empty(&port->suspend_urb_list)) {
		urb = list_first_entry(&port->suspend_urb_list, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		if(port->portno == wifi_pdata.data_port)
			urb->context = port;
		usb_anchor_urb(urb, &port->read_submitted);
		retval = usb_submit_urb(urb, GFP_KERNEL);
		if (retval < 0) {
			usb_unanchor_urb(urb);
			skw_usb_info("is error!!! %d\n", retval);
			return retval;
		}
	}
	if (usb_ports[1]==port && port->suspend) {
		port->suspend = 0;
		modem_notify_event(DEVICE_RESUME_EVENT);
	}
	port->suspend = 0;
	return 0;
}
static int skw_usb_io_reset_resume(struct usb_interface *interface)
{
	struct usb_port_struct *port;

	skw_usb_info("enter...\n");
	port = usb_get_intfdata(interface);
	if (port)
		port->suspend++;
	skw_usb_io_resume(interface);
	return 0;
}
#endif

/************************************************************************
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
struct usb_driver skw_usb_io_driver = {
	.name = "skw_usb_io",
	.probe = skw_usb_io_probe,
	.disconnect = skw_usb_io_disconnect,
#ifdef CONFIG_PM
        .suspend   = skw_usb_io_suspend,
        .resume    = skw_usb_io_resume,
        .reset_resume = skw_usb_io_reset_resume,
#endif	
	.pre_reset = skw_usb_io_pre_reset,
	.post_reset = skw_usb_io_post_reset,
	.id_table = skw_usb_io_id_table,
	.supports_autosuspend = 1,
};

/**
 * wcn_usb_io_init() - init wcn_usb_io's memory and register this driver.
 * @void: void.
 */
static int __init skw_usb_io_init(void)
{
	usb_bus_num = 0xff;
	usb_port_num = 0xff;
	wifi_data_pdev = NULL;
	bluetooth_pdev = NULL;
	log_port = NULL;
	usb_boot_data = NULL;
#ifndef CONFIG_SEEKWAVE_PLD_RELEASE
	cls_recovery_mode_en = 1;
#else
	cls_recovery_mode_en = 0;
#endif
	wifi_port_share = 0;
	usb_speed_switching = 0;
	memset(usb_ports, 0, sizeof(usb_ports));
	init_completion(&download_done);
	init_completion(&loop_completion);
	skw_usb_debugfs_init();
	skw_usb_log_level_init();
	chip_en_gpio = -1;
	modem_status = MODEM_OFF;
	skw_chipid = wifi_pdata.chipid;
	mutex_init(&g_recovery_data.except_mutex);
	INIT_DELAYED_WORK(&skw_except_work, skw_usb_exception_work);
	INIT_WORK(&add_device_work, add_devices_work);
	INIT_WORK(&dump_memory_worker, dump_memory_work);
	INIT_WORK(&usb_control_worker, usb_control_work);
	dump_memory_buffer = NULL;
	dump_log_size=NULL;
	dump_buffer_size = 0;
	usb_register(&skw_usb_io_driver);
	return 0;
}

/************************************************************************
 *Copyright(C) 2020-2021: Seekwave tech LTD 		China
 *Decription:
 *Author:jiayong.yang
 *Date:2021-05-27
 *Modfiy:
 *
 ********************************************************************* */
static void __exit skw_usb_io_exit(void)
{
	int ret;

	if (chip_en_gpio >=0) {
		skw_chip_set_power(0);
		msleep(200);
		skw_chip_set_power(1);
	}
	if (usb_ports[0] && usb_ports[0]->udev) {
		skw_usb_info("reset SKWUSB device");
		skw_reset_bus_dev();
	}
	if (usb_boot_data && usb_boot_data->pdev && wifi_data_pdev &&
	    wifi_data_pdev->dev.parent == &usb_boot_data->pdev->dev) {
		skw_usb_info("unregister WIFI device\n");
		platform_device_unregister(wifi_data_pdev);
		wifi_data_pdev = NULL;
		ret = 0;
	}
	seekwave_boot_exit();
	skw_usb_debugfs_deinit();
	cancel_delayed_work_sync(&skw_except_work);
	cancel_work_sync(&add_device_work);
	cancel_work_sync(&dump_memory_worker);
	cancel_work_sync(&usb_control_worker);
	dump_memory_buffer = NULL;
	dump_log_size=NULL;
	mutex_destroy(&g_recovery_data.except_mutex);
	if(bluetooth_pdev)
		platform_device_put(bluetooth_pdev);
	usb_deregister(&skw_usb_io_driver);
	if(wifi_data_pdev)
		platform_device_put(wifi_data_pdev);

}
module_init(skw_usb_io_init)
module_exit(skw_usb_io_exit)
MODULE_LICENSE("GPL v2");
