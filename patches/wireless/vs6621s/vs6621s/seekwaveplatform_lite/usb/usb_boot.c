/*************************************************************************************
 *Description: usb download
 *Seekwave tech LTD
 *Author: jiayong.yang/junwei.jiang
 *Date:20210527
 *Modify:
 * ***********************************************************************************/
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/ctype.h>
#include "skw_usb_log.h"
#include "usb_boot.h"
/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int dl_mps;
static int dloader_port = 0;

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int check_modem_status_from_connect_message(void)
{
	struct connect_ack *ack = (void *)&connect_ack[12];
	memcpy(skw_chipid,ack->chip_id,16);
	dl_mps = ack->packet_size;
	if(ack->flags.bitmap.boot)
		return NORMAL_BOOT;
	else
		return NORMAL_BOOT;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int dloader_write(char *msg, int msg_len, int *actual, int timeout)
{
	return bulkout_write_timeout(dloader_port, msg, msg_len, actual, timeout);
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int dloader_read(char *msg, int msg_len, int *actual, int timeout)
{
	return bulkin_read_timeout(dloader_port, msg, msg_len, actual, timeout);
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int compare_msg(const char *src, const char *dst, size_t count)
{
	unsigned char c1, c2;

	while (count) {
		c1 = *src++;
		c2 = *dst++;
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
		count--;
	}
	return 0;
}

static int dloader_send_data(const char *command, int command_len, const char *ack, int ack_len)
{
	int actual_len = 0;
	int ret;
	void *data;
	int data_size = 128;

	data = kzalloc(data_size, GFP_KERNEL);

	if (!data)
		return -ENOMEM;
	/* send command */
	ret = dloader_write((char *)command, command_len, &actual_len, 3000);
	if (ret <0 || actual_len != command_len) {
		skw_usb_err(" send cmd error ret %d actual_len %d command_len %d\n",
				ret, actual_len, command_len);
	} else {
		if (ack == NULL)
			goto OUT;

		/* read ack and check it */
		ret = dloader_read(data, data_size, &actual_len, 3000);
		if (ret <0 || ack_len > actual_len || compare_msg(ack, data, ack_len)) {
			skw_usb_err(" ack is NACK:ret == %d\n", ret);
			print_hex_dump(KERN_ERR, "ACK ERR:", 0, 16, 1, data, ack_len, 1);
			ret = -EIO;
		}
	}
OUT:
	kfree(data);
	return ret;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int dloader_send_command(const char *command,  int command_len, const char *ack, int ack_len)
{
	int actual_len = 0;
	int ret;
	void *data;
	int data_size = 128;

	data = kzalloc(data_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	/* send command */
	memcpy(data, (char*)command, command_len);
	ret = dloader_write(data, command_len, &actual_len, 3000);
	if (ret <0 || actual_len != command_len) {
		skw_usb_err(" send cmd error ret %d actual_len %d command_len %d\n",
				ret, actual_len, command_len);
	} else {
		/* read ack */
		ret = dloader_read(data, data_size, &actual_len, 3000);
		if (ret <0) {
			skw_usb_warn(" ack is NACK: acklen ===%d- actual_len ==%d--ret == %d\n",
				ret, ack_len, actual_len);
		}
	}
	if ((command_len > 8) &&(0 == command[8])){
		if(actual_len > sizeof(connect_ack))
			actual_len = sizeof(connect_ack);
		memcpy(connect_ack, data, actual_len);
	}
	kfree(data);
	return ret;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static unsigned short crc16_calculate(unsigned char *buf, int len)
{
	unsigned int i;
	unsigned short crc = 0;

	while (len-- != 0) {
		for (i = 0x80; i != 0; i = i >> 1) {
			if ((crc & 0x8000) != 0) {
				crc = crc << 1;
				crc = crc ^ 0x1021;
			} else {
				crc = crc << 1;
			}
			if ((*buf & i) != 0)
				crc = crc ^ 0x1021;
		}
		buf++;
	}
	return crc;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_command_start_download(unsigned int addr, unsigned int len)
{

	int command_len = 20;
	char command[20] = {0x7E, 0x7E, 0x7E, 0x7E,/* head */
		0x08, 0x00, 0x00, 0x00, /*length */
		0x01, 0x00, /* message type, 01: start command */
		0x00, 0x00, /*crc for data body, excludes message header */
		0x00, 0x00, 0x10, 0x00,/*addr*/
		0x60, 0xb3, 0x06, 0x00 /*image size*/};

	*((u32 *)&command[12]) = addr;
	*((u32 *)&command[16]) = len;

	*((u16 *)&command[10]) = cpu_to_be16(crc16_calculate(&command[12], command_len - 12));
	return dloader_send_command(command, command_len, common_ack, sizeof(common_ack));
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_command_exec(unsigned int addr)
{
	unsigned short command_len = 16;

	char command[16] = {0x7E,0x7E,0x7E,0x7E, /* head */
		0x04, 0x00, 0x00, 0x00,
		0x04, 0x00, /*command type */
		0x43, 0x63, /*command len */
		0x00, 0x00, 0x10, 0x00 /*addr*/
		};
	*((u32 *)&command[12]) = addr;
	*((u16 *)&command[10]) = crc16_calculate(&command[12], 4);
	return dloader_send_command(command, command_len, exec_ack, sizeof(exec_ack));
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_setup_usb_connection(struct usb_port_struct *port)
{
	int ret;

	dloader_command_client_probe();
	if (ret < 0) {
		dev_err(&port->udev->dev, "get version error\n");
		return ret;
	}
	dloader_command_connect();
	if (ret < 0) {
		dev_err(&port->udev->dev, "connection  error\n");
		return ret;
	}
	dev_info(&port->udev->dev,"dloader connect susscess...\n");
	return 0;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_execute_image(struct usb_port_struct *port,unsigned int addr)
{
	int ret;

	ret = dloader_command_exec(addr);
	if (ret < 0) {
		dev_err(&port->udev->dev, "exec command is error\n");
		return ret;
	}
	return 0;
}
/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:junwei.jiang
 *Date:
 *Modify:
 * ************************************************************************/
static unsigned int dloader_send_pdata(char* buf, const void *pdata, unsigned int len)
{
	PACKET_T *packet_ptr = (PACKET_T *)buf;
	int command_len = len + PACKET_HEADER_SIZE;

	packet_ptr->magic = PACKET_MAGIC;
	packet_ptr->type = 0x0002;
	packet_ptr->size = len;
	packet_ptr->crc = 0x0000;
	memset(packet_ptr->content, 0 , len);
	memcpy(packet_ptr->content,pdata, len);

	//crc check sum
	packet_ptr->crc = cpu_to_be16(crc16_calculate((char*)(&(packet_ptr->content)), command_len));

	return command_len;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int usb_download_image(struct usb_port_struct *port, unsigned int addr, unsigned int len)
{
	int ret;
	int size;
	int offset = 0;
	int img_size = 0;
	int temp_size = 0;

	/*the first command connect*/
	ret = dloader_command_start_download(addr, len);
	if (ret < 0) {
		dev_err(&port->udev->dev,"start download command failed\n");
		return ret;
	}
	/*get the data and the sv6160.bin size*/
	img_size = len;

	while (img_size > 0) {
		temp_size = MIN(dl_mps, img_size-offset);
		if(!temp_size)
			return 0;
		size  = dloader_send_pdata(port->read_buffer, (void*)(firmware_data)+offset, temp_size);
		if (size%512==0 && temp_size < dl_mps) {
			temp_size = temp_size>>1;
			size  = dloader_send_pdata(port->read_buffer, (void*)(firmware_data)+offset, temp_size);
		}  
		ret = dloader_send_data(port->read_buffer, size, common_ack, sizeof(common_ack));
		if (ret < 0) {
			dev_err(&port->udev->dev, "donwload img  error\n");
			return ret;
		}
		offset += temp_size;
	}
	return 0;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_get_chip_id(void *buf, unsigned int buf_size)
{
	int len = strlen(usb_ports[0]->udev->product);
	memcpy(buf, usb_ports[0]->udev->product, strlen(usb_ports[0]->udev->product));
	return len;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
int dloader_dump_from_romcode_usb(unsigned int addr, void *buf, int len)
{
	int ret;
	unsigned short command_len = 16;
	char command[20] = {0x7E, 0x7E, 0x7E, 0x7E,/* head */
		0x08, 0x00, 0x00, 0x00, /*for command data len */
		0x00, 0x09, /*command type */
		0x00, 0x00, /*for crc*/
		0x00, 0x00, 0x00, 0x00,/*addr*/
		0x00, 0x00, 0x00, 0x00,/*data len*/
		};
	int actual_len = 0;
	int size;

	//*((u32 *)&command[12]) = cpu_to_be32(addr);
	//*((u32 *)&command[16]) = cpu_to_be32(len);
	*((u32 *)&command[12]) = addr;
	*((u32 *)&command[16]) = len;

	*((u16 *)&command[10]) = cpu_to_be16(crc16_calculate(&command[1], command_len - 4));


	ret = dloader_send_command(command, command_len, NULL, 0);
	if (ret < 0) {
		skw_usb_err(" send command error\n");
		return -EIO;
	}

	size = dl_mps;
	while(len > 0) {
		if (len < size)
			size = len;
		ret = dloader_read(buf, size, &actual_len, 3000);
		if (ret < 0)
			skw_usb_err("dloader_read_ack dump memory error\n");
		else len -= actual_len;
	}
	return ret;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static int dloader_dump_read_usb(struct usb_port_struct *port)
{
	int ret;
	ret = dloader_dump_from_romcode_usb(START_ADDR, port->read_buffer, MAX_IMAGE_SIZE);
	return ret;
}

/***************************************************************************
 * Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 * ************************************************************************/
static void dloader_work(struct work_struct *work)
{
	struct usb_port_struct *port = container_of(work, struct usb_port_struct, work);
	int ret;
	dloader_port = port->portno;
	dloader_setup_usb_connection(port);

	ret = check_modem_status_from_connect_message();
	if (ret == HANG_REBOOT){
		dloader_dump_read_usb(port);
	}
	if(usb_boot_data->dram_dl_size > 0){
		firmware_data = usb_boot_data->dram_img_data;
		ret = usb_download_image(port, usb_boot_data->dram_dl_addr, usb_boot_data->dram_dl_size);
		if(ret <0)
			skw_usb_warn(" dram download img fail !!!!\n");
	}

	if(!ret && usb_boot_data->iram_dl_size > 0){
		firmware_data = usb_boot_data->iram_img_data;
		ret = usb_download_image(port, usb_boot_data->iram_dl_addr, usb_boot_data->iram_dl_size);
	}
	modem_notify_event(DEVICE_BOOTUP_EVENT);
	if (!ret)
		ret = dloader_execute_image(port, START_ADDR);

	if (ret < 0 && chip_en_gpio >= 0) {
		modem_status = MODEM_DOWNLOAD_FAILED;
		skw_usb_info("download failed! power off device\n");
		gpio_set_value(chip_en_gpio, 0);
		msleep(10);
	}
}
