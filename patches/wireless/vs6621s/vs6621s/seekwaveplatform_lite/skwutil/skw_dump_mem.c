
#include "sv6621s_mem_map.h"
#include "skw_log_to_file.h"

struct memory_segment {
	const char 	*name;
	uint32_t 	address;
	uint32_t 	size;
};

struct memory_segment cp_mem_seg[] = {
	{ "CODE", 	CODE_MEM_BASE_ADDR, CODE_MEM_SIZE},
	{ "DATA", 	DATA_MEM_BASE_ADDR, DATA_MEM_SIZE},
	{ "AHBR", 	AHB_REG_BASE_ADDR, AHB_REG_SIZE},
	{ "PPBM", 	UMEM_MEM_BASE_ADDR, UMEM_MEM_SIZE},
	{ "SMEM", 	SMEM_MEM_BASE_ADDR, SMEM_MEM_SIZE},
	{ "WFRF", 	0x40140000, 0x4000},
	{ "CSCB", 	0xE000E000, 0x1000},
	{ "WREG", 	WREG_MEM_BASE_ADDR, WREG_MEM_SIZE},
	{ "PHYR", 	PHYR_MEM_BASE_ADDR, PHYR_MEM_SIZE},
	{ "SDIO", 	SDIO_MEM_BASE_ADDR, SDIO_MEM_SIZE},
	{ "BTRG", 	BTDM_MEM_BASE_ADDR, BTDM_MEM_SIZE},
	{ "BTEM", 	BTEM_MEM_BASE_ADDR, BTEM_MEM_SIZE},
	{ "BTGB", 	BTGB_MEM_BASE_ADDR, BTGB_MEM_SIZE},
	{ "BTRF", 	BTRF_MEM_BASE_ADDR, BTRF_MEM_SIZE},
	{ "RFTOP", 	RFTOP_MEM_BASE_ADDR, RFTOP_MEM_SIZE},
	{ "RCLK", 	RCLK_MEM_BASE_ADDR, RCLK_MEM_SIZE},
	{ "BBPLL", 	BBPLL_MEM_BASE_ADDR, BBPLL_MEM_SIZE}

};
static uint32_t skw_checksum(void *data, int data_len)
{
	uint32_t *d32 = data;
	uint32_t checksum=0;
	int i;

	data_len = data_len >> 2;
	for (i=0; i<data_len; i++)
		checksum += d32[i];
	return checksum;
}
int skw_dump_memory_into_buffer(struct ucom_dev *ucom, char *buffer, int length)
{
	struct memory_segment *mem_sg = &cp_mem_seg[0];
	int offset=0;
	uint16_t seq, packet_len;
	uint32_t sg_size;
	char *read_buf;
	uint8_t sg_count;
	int ret = 0;

	if (!ucom || !ucom->pdata ||
	    !ucom->pdata->skw_dump_mem)
		return 0;
	sg_count = sizeof(cp_mem_seg)/sizeof(cp_mem_seg[0]);
	if (sg_count==0)
		return 0;
	packet_len = 0x800;
	read_buf = kmalloc(packet_len, GFP_KERNEL);
	if (read_buf==NULL)
		return 0;
	buffer[offset] = sg_count; //save total segment count
	offset++;

	do {
		uint32_t source_addr;
		sg_size = mem_sg->size;

		memcpy(&buffer[offset], mem_sg->name, 5); //save segment name
		offset += 5;
		memcpy(&buffer[offset], &mem_sg->address, 4); //save segment base addrss
		offset += 4;
		memcpy(&buffer[offset], &mem_sg->size, 4); //save segment size
		offset += 4;
		memcpy(&buffer[offset], &packet_len, 2); //save segment size
		offset += 2;
		skwlog_log("%s %s:%d 0x%x 0x%x\n", __func__, mem_sg->name,
			offset, mem_sg->address, mem_sg->size);
		seq = 0;
		source_addr = mem_sg->address;
		do {
			int read_len;
			uint32_t sum;

			memcpy(&buffer[offset], &seq, 2); //save segment size
			seq++;
			offset += 2;

			if (sg_size > packet_len)
				read_len = packet_len;
			else
				read_len = sg_size;
			ret = ucom->pdata->skw_dump_mem(source_addr,(void *)read_buf,read_len);
			if (ret != 0) {
				skwlog_err("%s dump memory fail :%d \n", __func__, ret);
				break;
			}
			source_addr += read_len;
			memcpy(buffer+offset, read_buf, read_len); //save packet payload 
			offset += read_len;

			sum = skw_checksum(read_buf, read_len);
			memcpy (buffer+offset, &sum, 4); //save checksum
			offset += 4;

			sg_size -= read_len;
		} while (sg_size);
		mem_sg++;
		sg_count--;
	} while (sg_count && (!ret));
	kfree(read_buf);
	return offset;
}

static int skw_ucom_dump_from_buffer(char __user *buf, size_t count, loff_t *pos)
{
	int len;
	int ret;

	len = 0;
	ret = 0;
	if (dump_log_size) {
		if (*pos + count < dump_log_size)
			len = count;
		else if (*pos < dump_log_size)
			len = dump_log_size - *pos;
		if (len)
			ret = copy_to_user(buf, &dump_memory_buffer[*pos], len);
		if (ret ==0)
			ret = len;
	} else if (*pos == 0){
		char assert_info[32]={0};
		sprintf(assert_info,"modem_status=%d\n", cp_exception_sts);
		ret = copy_to_user(buf, assert_info, strlen(assert_info));
		len = strlen(assert_info);
	}
	*pos = *pos+len;
	if (len==0)
		skwboot_log("dump_log_size: %d offset %d count %d %p ret=%d\n", dump_log_size, (int)*pos, (int)count, dump_memory_buffer, ret);
	return ret;
}

static int user_dump_open(struct inode *ip, struct file *fp)
{
	if (dump_buffer_size)
		return 0;
	return 0;
}

static ssize_t user_dump_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret = skw_ucom_dump_from_buffer(buf, count, pos);
	if (ret == 0)
		dump_log_size = 0;
	return ret;

}
static int user_dump_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static ssize_t user_dump_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	return 0;
}
static long user_dump_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	return 0;
}
static const struct file_operations skw_dump_ops = {
	.owner	= THIS_MODULE,
	.open	= user_dump_open,
	.read	= user_dump_read,
	.write	= user_dump_write,
	.unlocked_ioctl = user_dump_ioctl,
	.release= user_dump_release,
};
static int bt_state_event_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct ucom_dev *ucom = container_of(nb, struct ucom_dev, notifier);
	//int status = cp_exception_sts;
	switch(action)
	{
		case DEVICE_ASSERT_EVENT:
		{
			skwboot_log("UCOM BSPASSERT EVENT received!!!!\n");
			cp_exception_sts = 1;
			ucom = ucoms[log_portno];
			if (dump_log_size){
				skwboot_log(" dump done ,mem size: %d\n", dump_log_size);
				return NOTIFY_OK;
			}
			if (!dump_memory_buffer && !atomic_read(&ucom->open) && SKW_DUMP_BUFFER_SIZE!=0)
				dump_memory_buffer = kzalloc(SKW_DUMP_BUFFER_SIZE, GFP_KERNEL);
			if(ucom->pdata && ucom->pdata->dump_modem_memory) {
				dump_log_size = 0;
				if (dump_memory_buffer) {
					dump_buffer_size = SKW_DUMP_BUFFER_SIZE;
					ucom->pdata->dump_modem_memory(dump_memory_buffer,
							dump_buffer_size, &dump_log_size);
				}
			} else if (dump_memory_buffer) {
				dump_buffer_size = SKW_DUMP_BUFFER_SIZE;
				if (!atomic_read(&ucom->open))
					dump_log_size = skw_dump_memory_into_buffer(ucom, dump_memory_buffer,dump_buffer_size);
            }

		}
		break;
		case DEVICE_BSPREADY_EVENT:
		{
			cp_exception_sts = 0;
			skwboot_log("UCOM BSPREADY EVENT Comming in !!!!\n");
		}
		break;
		case DEVICE_DUMPDONE_EVENT:
		{
			cp_exception_sts = 2;
			skwboot_log("UCOM DUMPDONE EVENT Comming in !!!!\n");
		}
		break;
		case DEVICE_BLOCKED_EVENT:
		{
			cp_exception_sts = 3;
			skwboot_log("UCOM BLOCKED EVENT Comming in !!!!\n");
			if (dump_log_size){
				skwboot_log(" dump done ,mem size: %d\n", dump_log_size);
				return NOTIFY_OK;
			}
			if (!dump_memory_buffer)
				dump_memory_buffer = kzalloc(SKW_DUMP_BUFFER_SIZE, GFP_KERNEL);
			if (dump_memory_buffer) {
				dump_buffer_size = SKW_DUMP_BUFFER_SIZE;
				dump_log_size = skw_dump_memory_into_buffer(ucom, dump_memory_buffer,dump_buffer_size);
			}
		}
		break;
		case DEVICE_DISCONNECT_EVENT:
		{
			cp_exception_sts = action;
			skwboot_log("UCOM DISCONNECT EVENT Comming in !!!!\n");
		}
		break;
		default:
		break;

	}
	return NOTIFY_OK;
}

static int skw_bt_state_event_init(struct ucom_dev *ucom)
{
	int ret = 0;
	int devno;
	if (ucom->pdata->modem_register_notify && ucom->notifier.notifier_call == NULL) {
		ucom->notifier.notifier_call = bt_state_event_notifier;
		ucom->pdata->modem_register_notify(&ucom->notifier);
	}
	ret =__register_chrdev(skw_major, UCOM_PORTNO_MAX+1, 1,
                                          "SKWDUMP", &skw_dump_ops);

	devno = MKDEV(skw_major, UCOM_PORTNO_MAX+1);
	if (ret==0)
		device_create(skw_com_class, NULL, devno, NULL, "%s", "SKWDUMP");
	skwlog_log("%s enter  ret=%d\n",__func__, ret);
	return ret;
}

static int skw_bt_state_event_deinit(struct ucom_dev *ucom)
{
	int ret = 0;
	int devno;
	if(ucom) {
		if((ucom->notifier.notifier_call)){
			skwboot_log("%s :%d release the notifier \n", __func__,__LINE__);
			ucom->notifier.notifier_call = NULL;
			ucom->pdata->modem_unregister_notify(&ucom->notifier);
		}
		devno = MKDEV(skw_major, UCOM_PORTNO_MAX+1);
		__unregister_chrdev(skw_major, MINOR(devno), 1,  "SKWDUMP");
		device_destroy(skw_com_class, devno);
	}
	return ret;
}
