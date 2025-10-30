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


#define LOG_TAG "skw_btsnoop"

#include "skw_btsnoop.h"
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
//#include <linux/timekeeping.h>
#include "skw_common.h"

static const uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;

uint64_t skw_btsnoop_timestamp(void)
{
    //uint64_t timestamp = ktime_get_ns() / 1000LL + BTSNOOP_EPOCH_DELTA;
    //uint64_t timestamp = do_div(ktime_get_ns(), 1000) + BTSNOOP_EPOCH_DELTA;
    uint64_t timestamp = div_u64(ktime_get_ns(), 1000) + BTSNOOP_EPOCH_DELTA;

    return timestamp;
}


#if BT_HCI_LOG_EN

static struct mutex btsnoop_log_lock;


extern int skwbt_log_disable;


struct file *skw_btsnoop_open(void)
{
    struct file *hci_btsnoop_fd = NULL;
    char log_path[256] = {0}, is_new_file = 1;
    int file_size = 0;
    int file_mode = O_WRONLY | O_CREAT | O_APPEND;
    struct file *fp = NULL;

    snprintf(log_path, 256, "%s/btsnoop-hci.cfa", SEEKWAVE_BT_LOG_PATH);

    fp = filp_open(log_path, O_RDONLY, 0644);
    if(!IS_ERR(fp))
    {
        file_size = (int)vfs_llseek(fp, 0, SEEK_END);
        //pr_info("btsnoop file size:%d", file_size);
        filp_close(fp, NULL);
    }
    if(file_size >= MAX_BT_LOG_SIZE)
    {
        char tmp_path[256] = {0};
        snprintf(tmp_path, 256, "%s/btsnoop-hci.cfa.last", SEEKWAVE_BT_LOG_PATH);
        skw_file_copy(log_path, tmp_path);
        file_mode = O_CREAT | O_WRONLY | O_TRUNC;
    }
    else if(file_size > 0)
    {
        is_new_file = 0;
    }

   //pr_info("btsnoop_rev_length:%d", btsnoop_rev_length);

    hci_btsnoop_fd = filp_open(log_path, file_mode, 0644);

    if ((hci_btsnoop_fd == NULL) || IS_ERR(hci_btsnoop_fd))
    {
        //pr_info("btsnoop open fail, err:%lld", PTR_ERR(fp));
        hci_btsnoop_fd = NULL;
        return NULL;
    }

    if(is_new_file)
    {
        skw_file_write(hci_btsnoop_fd, "btsnoop\0\0\0\0\1\0\0\x3\xea", 16);
    }
    return hci_btsnoop_fd;
}

void skw_btsnoop_init(void)
{
    mutex_init(&btsnoop_log_lock);
}

void skw_btsnoop_close(void)
{
    mutex_unlock(&btsnoop_log_lock);
}

static void skw_btsnoop_write(struct file *fp, const void *data, size_t length)
{
    if (fp != NULL)
    {
        skw_file_write(fp, data, length);
    }
}

void skw_btsnoop_capture(const unsigned char *packet, unsigned char is_received)
{
    int length_he = 0;
    int length    = 0;
    int flags     = 0;
    int drops     = 0;
    unsigned char type = packet[0];

    uint64_t timestamp = skw_btsnoop_timestamp();
    unsigned int time_hi = timestamp >> 32;
    unsigned int time_lo = timestamp & 0xFFFFFFFF;
    struct file *fp = NULL;
	if(skwbt_log_disable)
	{
		return ;
	}

    mutex_lock(&btsnoop_log_lock);

    fp = skw_btsnoop_open();
    if((fp == NULL) || IS_ERR(fp))
    {
        mutex_unlock(&btsnoop_log_lock);
        return ;
    }

    switch (type)
    {
        case HCI_COMMAND_PKT:
            length_he = packet[3] + 4;
            flags = 2;
            break;
        case HCI_ACLDATA_PKT:
            length_he = (packet[4] << 8) + packet[3] + 5;
            flags = is_received;
            break;
        case HCI_SCODATA_PKT:
            length_he = packet[3] + 4;
            flags = is_received;
            break;
        case HCI_EVENT_PKT:
            length_he = packet[2] + 3;
            flags = 3;
            break;
        default:
            mutex_unlock(&btsnoop_log_lock);
            return;
    }


    length = htonl(length_he);
    flags = htonl(flags);
    drops = htonl(drops);
    time_hi = htonl(time_hi);
    time_lo = htonl(time_lo);

    skw_btsnoop_write(fp, &length, 4);
    skw_btsnoop_write(fp, &length, 4);
    skw_btsnoop_write(fp, &flags, 4);
    skw_btsnoop_write(fp, &drops, 4);
    skw_btsnoop_write(fp, &time_hi, 4);
    skw_btsnoop_write(fp, &time_lo, 4);

    skw_btsnoop_write(fp, packet, length_he);

    filp_close(fp, NULL);

    mutex_unlock(&btsnoop_log_lock);
}

EXPORT_SYMBOL_GPL(skw_btsnoop_init);
EXPORT_SYMBOL_GPL(skw_btsnoop_capture);
EXPORT_SYMBOL_GPL(skw_btsnoop_close);
EXPORT_SYMBOL_GPL(skw_btsnoop_timestamp);

#endif
