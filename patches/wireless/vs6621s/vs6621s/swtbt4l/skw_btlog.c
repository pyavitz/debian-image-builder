/*
 *
 *  Seekwave Bluetooth driver
 *
 *  Copyright (C) 2023  Seekwave Tech Ltd.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/poll.h>

#if 1
#include <linux/platform_data/skw_platform_data.h>
#include "skw_common.h"
#else
#include <linux/platform_data/skw_platform_data.h>
#include "skw_btlog.h"
#endif


#ifndef SKWBT_INFO
#define SKWBT_INFO(format, ...)     pr_info("[SKWBTLOG_INFO] "format, ##__VA_ARGS__)
#endif

#ifndef SKWBT_ERROR
#define SKWBT_ERROR(format, ...)    pr_err("[SKWBTLOG_ERROR] "format, ##__VA_ARGS__)
#endif


#if SKWBT_LOG_PORT_EN

typedef struct
{
    uint16_t data_len;
    uint16_t data_len1;
    uint8_t  is_from_cp;
    uint8_t  is_from_cp1;
    uint16_t reserved;
    uint32_t time_h;
    uint32_t time_l;
    uint8_t  data[1];//
} __packed skwbt_log_pkt_st;

typedef struct
{
    struct class *skwbt_class;
    int skwbt_major;
    uint8_t is_open;
    struct sk_buff_head skwbt_rx_q;
    atomic_t rx_recv;
    wait_queue_head_t poll_wait_queue;
    char skwbt_is_open;
} skwbt_log_port_info_st;

#define SKWBT_LOGPORT_NAME "SKWBT_LOG"


static void *skwbt_log_pdata = NULL;
static skwbt_log_port_info_st *skwbt_log_port_info = NULL;

char hex2char(int num)
{
    char ch;
    if(num >= 0 && num <= 9)
    {
        ch = num + 48;
    }
    else if(num > 9 && num <= 15)
    {
        ch = (num - 10) + 65;
    }
    else
    {
        ch = '\0';
    }
    return ch;
}

void hex2String(unsigned char hex[], unsigned char str[], int N)
{
    int i = 0, j;
    for(i = 0, j = 0; i < N; i++, j += 2)
    {
        str[j] = hex2char((hex[i] & 0xF0) >> 4);
        str[j + 1] = hex2char(hex[i] & 0x0F);
    }
    str[N << 1] = 0;
}


void skwbt_log_open(char is_open)
{
    if(skwbt_log_port_info && skwbt_log_pdata)
    {
        struct sv6160_platform_data *pdata = (struct sv6160_platform_data *)skwbt_log_pdata;

        if(pdata->bluetooth_log_disable)
        {
            pdata->bluetooth_log_disable(is_open > 0 ? 0 : 1);
        }
        SKWBT_INFO("skwbt_log_open, is_open:%d", is_open);
    }
}


void skwbt_log_port_set_bt_open(char is_open)
{
    if(skwbt_log_port_info != NULL)
    {
        skwbt_log_port_info->skwbt_is_open = is_open;
        if(skwbt_log_port_info->is_open == 1)
        {
            skwbt_log_open(is_open);
        }
    }
}


void skwbt_log_port_data_write(uint8_t is_from_cp, uint8_t *data, uint16_t data_len)
{
    if((skwbt_log_port_info != NULL) && (skwbt_log_port_info->is_open == 1) && (data_len >= 4))//min len is 4
    {
        struct sk_buff *skb;
        skwbt_log_pkt_st *log_pkt;
        uint16_t total_len = data_len + 16;
        uint64_t timestamp = 0;
        ktime_t c_time;

        if(skb_queue_len(&skwbt_log_port_info->skwbt_rx_q) > 40)//40 packets
        {
            return ;
        }

        skb = alloc_skb(total_len, GFP_ATOMIC);
        if (!skb)
        {
            SKWBT_ERROR("skwbt log alloc skb failed, data_len: %d", (int)data_len);
            return ;
        }
        //timestamp = div_u64(ktime_get(), 1000);
        c_time = ktime_get();
        timestamp = *(uint64_t *)&c_time;
        timestamp = div_u64(timestamp, 1000);

        log_pkt = (skwbt_log_pkt_st *)skb_put(skb, total_len);
        log_pkt->data_len = data_len;
        log_pkt->data_len1 = data_len;
        log_pkt->is_from_cp = is_from_cp;
        log_pkt->is_from_cp1 = is_from_cp;
        log_pkt->reserved = 0xAABB;
        log_pkt->time_h = (uint32_t)(timestamp >> 32ull);
        log_pkt->time_l = (uint32_t)timestamp;
        memcpy(log_pkt->data, data, data_len);
#if 0
        {
            uint8_t str_buffer[130] = {0x00};
            hex2String(data, str_buffer, (data_len > 64) ? 64 : data_len);
            SKWBT_INFO("HCI log, is_from_cp:%d, len:%d, %s", is_from_cp, (int)data_len, str_buffer);
        }
#endif
        //SKWBT_INFO("%s,is_from_cp:%d  data_len: %d\n", __func__, is_from_cp, (int)data_len);

        skb_queue_tail(&skwbt_log_port_info->skwbt_rx_q, skb);
        atomic_inc(&skwbt_log_port_info->rx_recv);
        wake_up(&skwbt_log_port_info->poll_wait_queue);
    }
}

void skwbt_log_port_release_queue(void)
{
    struct sk_buff *skb;
    if(skwbt_log_port_info != NULL)
    {
        while((skb = skb_dequeue(&skwbt_log_port_info->skwbt_rx_q)))
        {
            kfree_skb(skb);
        }
    }
}


static int skwbt_log_port_open(struct inode *inode, struct file *filp)
{
    if((skwbt_log_port_info == NULL) || (skwbt_log_port_info->is_open != 0))
    {
        return -EIO;
    }
    atomic_set(&skwbt_log_port_info->rx_recv, 0);
    skwbt_log_port_info->is_open = 1;
    SKWBT_INFO("%s, skwbt_is_open:%d", __func__, skwbt_log_port_info->skwbt_is_open);
    if(skwbt_log_port_info->skwbt_is_open)
    {
        skwbt_log_open(1);
    }

    return 0;
}

int skwbt_log_port_data_dequeue(int read_len, char __user *buf)
{
    int data_size = 0;
    if(skwbt_log_port_info)
    {
        struct sk_buff *skb;
        if(buf == NULL)
        {
            SKWBT_INFO("%s, invalid read buffer, read_len:%d", __func__, read_len);
            return 0;
        }

        skb = skb_peek(&skwbt_log_port_info->skwbt_rx_q);
        if(skb)
        {
            int ret = 0;

            if(read_len >= skb->len)
            {
                data_size = skb->len;
                skb = skb_dequeue(&skwbt_log_port_info->skwbt_rx_q);
                ret = copy_to_user(buf, skb->data, data_size);

                //SKWBT_INFO("%s, read_len:%d, len:%d, type:%d", __func__, read_len, data_size, skb->data[0]);

                kfree_skb(skb);
            }
            else//read_len < skb->len
            {
                data_size = read_len;
                ret = copy_to_user(buf, skb->data, read_len);
                skb_pull(skb, read_len);

                //SKWBT_INFO("%s, len:%d, type:%d, last:%d", __func__, data_size, skb->data[0], skb->len);
            }
            //SKWBT_INFO("%s enter..., read len:%d, data_size:%d\n", __func__, (int)read_len, data_size);

            if(ret != 0)
            {
                data_size = -EFAULT;
            }
        }
    }
    return data_size;
}


static ssize_t skwbt_log_port_read(struct file *filp, char __user *buf, size_t read_len, loff_t *offt)
{
    if((skwbt_log_port_info != NULL) && (skwbt_log_port_info->is_open == 1))
    {
        if(!skb_queue_empty(&skwbt_log_port_info->skwbt_rx_q))//not empty
        {
            return skwbt_log_port_data_dequeue(read_len, buf);
        }
        else if(filp->f_flags & O_NONBLOCK)
        {
            return 0;
        }
        atomic_set(&skwbt_log_port_info->rx_recv, 0);
        wait_event_interruptible(skwbt_log_port_info->poll_wait_queue, atomic_read(&skwbt_log_port_info->rx_recv));
        //SKWBT_INFO("%s exit...\n", __func__);
        return skwbt_log_port_data_dequeue(read_len, buf);
    }
    return 0;
}

static ssize_t skwbt_log_port_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    SKWBT_INFO("%s, len:%d", __func__, (int)cnt);
    return 0;
}

static int skwbt_log_port_flush(struct file *filp, fl_owner_t id)
{
    skwbt_log_port_release_queue();
    return 0;
}

static int skwbt_log_port_release(struct inode *inode, struct file *filp)
{
    skwbt_log_port_info->is_open = 0;
    atomic_inc(&skwbt_log_port_info->rx_recv);
    wake_up(&skwbt_log_port_info->poll_wait_queue);

    skwbt_log_port_release_queue();

    skwbt_log_open(0);
    return 0;
}

unsigned int skwbt_log_port_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    poll_wait(filp, &skwbt_log_port_info->poll_wait_queue, wait);

    if(!skb_queue_empty(&skwbt_log_port_info->skwbt_rx_q))
    {
        mask = POLLIN | POLLRDNORM;
    }

    //SKWBT_INFO("%s enter..., data size:%d\n", __func__, queue_get_data_size());

    return mask | POLLOUT | POLLWRBAND;
}


long skwbt_log_port_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    return 0;

}


static struct file_operations skwbt_log_port_fops =
{
    .owner = THIS_MODULE,
    .open = skwbt_log_port_open,
    .read = skwbt_log_port_read,
    .write = skwbt_log_port_write,
    .flush = skwbt_log_port_flush,
    .poll  = skwbt_log_port_poll,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    .unlocked_ioctl = skwbt_log_port_ioctl,
#else
    .ioctl = skwbt_log_port_ioctl,
#endif
    .release = skwbt_log_port_release,
};


void skwbt_log_port_init_fail(void)
{
    kfree(skwbt_log_port_info);
    skwbt_log_port_info = NULL;
}

void skwbt_log_port_set_pdata(void *pdata)
{
    skwbt_log_pdata = pdata;

    SKWBT_INFO("%s, pdata:%p, skwbt_log_port_info:%p", __func__, pdata, skwbt_log_port_info);
}


void skwbt_log_port_init(void)
{
    SKWBT_INFO("Seekwave Bluetooth Log init");

    if(skwbt_log_port_info == NULL)
    {
        struct device *skw_device;
        int ret = 0;
        int val_size = sizeof(skwbt_log_port_info_st);
        skwbt_log_port_info = kmalloc(val_size, GFP_KERNEL);
        if(skwbt_log_port_info == NULL)
        {
            return ;
        }
        memset(skwbt_log_port_info, 0, val_size);
        skwbt_log_port_info->is_open = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
        skwbt_log_port_info->skwbt_class = class_create("skwbtlog_port");
#else
        skwbt_log_port_info->skwbt_class = class_create(THIS_MODULE, "skwbtlog_port");
#endif
        if(IS_ERR(skwbt_log_port_info->skwbt_class))
        {
            //int ret =  PTR_ERR(skwbt_proc_log_info->skwbt_class);
            skwbt_log_port_info->skwbt_class = NULL;

            skwbt_log_port_init_fail();
            return ;
        }

        ret = register_chrdev(0, SKWBT_LOGPORT_NAME, &skwbt_log_port_fops);
        if(ret < 0)
        {
            SKWBT_INFO("%s register_chrdev fail %d", __func__, ret);

            class_destroy(skwbt_log_port_info->skwbt_class);
            skwbt_log_port_info->skwbt_class = NULL;
            skwbt_log_port_init_fail();
            return ;
        }
        skw_device = device_create(skwbt_log_port_info->skwbt_class, NULL, MKDEV(ret, 1), NULL, "%s", SKWBT_LOGPORT_NAME);
        if (!skw_device)
        {
            SKWBT_ERROR("%s, create device failed", __func__);

            unregister_chrdev(ret, SKWBT_LOGPORT_NAME);
            class_destroy(skwbt_log_port_info->skwbt_class);

            skwbt_log_port_init_fail();
            return ;
        }
        skwbt_log_port_info->skwbt_major = ret;

        skb_queue_head_init(&skwbt_log_port_info->skwbt_rx_q);
        init_waitqueue_head(&skwbt_log_port_info->poll_wait_queue);
    }
}


void skwbt_log_port_exit(void)
{
    SKWBT_INFO("Seekwave Bluetooth Log Port exit");

    if(skwbt_log_port_info != NULL)
    {
        skwbt_log_port_release_queue();

        unregister_chrdev(skwbt_log_port_info->skwbt_major, SKWBT_LOGPORT_NAME);
        device_destroy(skwbt_log_port_info->skwbt_class, MKDEV(skwbt_log_port_info->skwbt_major, 1));
        class_destroy(skwbt_log_port_info->skwbt_class);
        skwbt_log_port_info->skwbt_class = NULL;

        kfree(skwbt_log_port_info);
        skwbt_log_port_info = NULL;
        skwbt_log_pdata = NULL;
    }
}

#if 0
EXPORT_SYMBOL_GPL(skwbt_log_port_data_write);
EXPORT_SYMBOL_GPL(skwbt_log_port_init);
EXPORT_SYMBOL_GPL(skwbt_log_port_exit);
EXPORT_SYMBOL_GPL(skwbt_log_port_set_bt_open);
EXPORT_SYMBOL_GPL(skwbt_log_port_set_pdata);
#endif



#endif





