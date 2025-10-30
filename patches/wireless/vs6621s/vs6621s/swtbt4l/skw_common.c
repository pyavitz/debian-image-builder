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

#include "skw_common.h"
#include "skw_btsnoop.h"

#if ((BT_HCI_LOG_EN == 1) || (BT_CP_LOG_EN == 1))

#define FILE_RW_ENABLE

#endif

#ifndef BD_ADDR_FILE_PATH
//#define BD_ADDR_FILE_PATH SEEKWAVE_BT_LOG_PATH
#else

#endif

#define BD_ADDR_FILE_PATH "/devinfo/skwbt"


static unsigned char bdaddr_lap[4] = {0x12, 0x24, 0x56};
static char bdaddr_valid = 0;
static unsigned int randseed;


#ifdef FILE_RW_ENABLE


mm_segment_t skwbt_get_fs(void)
{
    mm_segment_t oldfs;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
    oldfs = force_uaccess_begin();
#else
    oldfs = get_fs();
    set_fs(KERNEL_DS);
#endif

    return oldfs;
}

void skwbt_set_fs(mm_segment_t fs)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
    force_uaccess_end(fs);
#else
    set_fs(fs);
#endif

}

#endif

ssize_t skw_file_write(struct file *fp, const void *buf, size_t len)
{
#ifdef FILE_RW_ENABLE
    ssize_t res_len = 0;
    loff_t pos = fp->f_pos;
    mm_segment_t fs = skwbt_get_fs();
    res_len = skw_write(fp, buf, len, &pos);
    fp->f_pos = pos;
    skwbt_set_fs(fs);

    return res_len;
#else
    return 0;
#endif

}
EXPORT_SYMBOL_GPL(skw_file_write);


ssize_t skw_file_read(struct file *fp, void *buf, size_t len)
{
#ifdef FILE_RW_ENABLE
    ssize_t res_len = 0;
    loff_t pos = fp->f_pos;
    mm_segment_t fs = skwbt_get_fs();
    res_len = skw_read(fp, buf, len, &pos);
    fp->f_pos = pos;
    skwbt_set_fs(fs);
    return res_len;
#else
    return 0;
#endif
}
EXPORT_SYMBOL_GPL(skw_file_read);



/*
file copy
return 1:success
*/
char skw_file_copy(char *scr_file, char *des_file)
{
#ifdef FILE_RW_ENABLE
    struct file *src_fp = filp_open(scr_file, O_RDONLY, 0644);
    struct file *des_fp = filp_open(des_file, O_RDWR | O_CREAT, 0644);
    char *pld_buf;
    int len;

    if(IS_ERR(src_fp) || (IS_ERR(des_fp)))
    {
        return -1;
    }
    pld_buf = (char *)kzalloc(1025, GFP_KERNEL);

    while(1)
    {
        len = skw_file_read(src_fp, pld_buf, 1024);
        if(len <= 0)
        {
            break;
        }
        skw_file_write(des_fp, pld_buf, len);
    }

    kfree(pld_buf);
    filp_close(src_fp, NULL);
    filp_close(des_fp, NULL);
#endif
    return 1;
}
EXPORT_SYMBOL_GPL(skw_file_copy);



unsigned int skw_rand(void)
{
    unsigned int r;// = randseed = randseed * 1103515245 + 12345;

    do
    {
        r = randseed = randseed * 1103515245 + 12345;
        r = (r << 16) | ((r >> 16) & 0xFFFF);
    } while(r == 0);

    return r;
}

void skw_srand(void)
{
    randseed = (unsigned int) ktime_get_ns();
    skw_rand();
    skw_rand();
    skw_rand();
}


void skw_bd_addr_gen_init(void)
{
#ifdef BD_ADDR_FILE_PATH

#ifdef FILE_RW_ENABLE
    struct file *fp = NULL;
    char file_path[256] = {0};
    if(bdaddr_valid)
    {
        return ;
    }
    skw_srand();

    snprintf(file_path, 256, "%s/skwbdaddr", BD_ADDR_FILE_PATH);

    SKWBT_INFO("skwbdaddr init path:%s\n", file_path);

    fp = filp_open(file_path, O_RDWR, 0666);
    if((fp == NULL) || IS_ERR(fp))
    {
        fp = filp_open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if((fp == NULL) || IS_ERR(fp))
        {
            SKWBT_INFO("skwbdaddr open err:%ld\n", PTR_ERR(fp));
        }
        else
        {
            bdaddr_lap[0] = (unsigned char)(skw_rand() & 0xFF);
            bdaddr_lap[1] = (unsigned char)(skw_rand() & 0xFF);
            bdaddr_lap[2] = (unsigned char)(skw_rand() & 0xFF);
            SKWBT_INFO("skwbd addr:%x\n", *((u32 *)bdaddr_lap));
            if(skw_file_write(fp, bdaddr_lap, 3) != 3)
            {
                SKWBT_INFO("skwbd addr write err:%ld\n", PTR_ERR(fp));
            }
            bdaddr_valid = 1;
            filp_close(fp, NULL);

        }
    }
    else
    {
        if(skw_file_read(fp, bdaddr_lap, 3) > 0)
        {
            bdaddr_valid = 1;
        }

        filp_close(fp, NULL);
    }
#endif
#endif
}
EXPORT_SYMBOL_GPL(skw_bd_addr_gen_init);


char skw_get_bd_addr(unsigned char *buffer)
{
    if(bdaddr_valid > 0)
    {
        buffer[0] = bdaddr_lap[0];
        buffer[1] = bdaddr_lap[1];
        buffer[2] = bdaddr_lap[2];
        return 1;
    }
    return 0;
}
EXPORT_SYMBOL_GPL(skw_get_bd_addr);



char *skw_strchr(char *str, const char ch)
{
    char *ptr = str;

    while((ptr != NULL) && ((*ptr) != '\r') && ((*ptr) != '\n') && ((*ptr) != 0))
    {
        if((*ptr) == ch)
        {
            return ptr;
        }
        ptr ++;
    }
    return NULL;
}

int skw_strlen(char *str)
{
    char *ptr = str;
    int str_len = 0;
    while((ptr != NULL) && ((*ptr) != '\r') && ((*ptr) != '\n') && ((*ptr) != 0))
    {
        ptr ++;
        str_len ++;
    }
    return str_len;
}
EXPORT_SYMBOL_GPL(skw_strlen);

unsigned char skw_char2hex(char ch)
{
    unsigned char num = 0;
    if(ch >= '0' && ch <= '9')
    {
        num = ch - 48;//0:48
    }
    else if(ch >= 'a' && ch <= 'f')
    {
        num = ch + 10 - 97;//a:97
    }
    else if(ch >= 'A' && ch <= 'F')
    {
        num = ch + 10 - 65;//A:65
    }
    return num;
}
EXPORT_SYMBOL_GPL(skw_char2hex);


/*
    data_str = "xxxx;...."
*/
char *skwbt_config_get_uint8(char *data_str, uint8_t *value)
{
    char *split0 = skw_strchr(data_str, ';');
    uint8_t len = 0;
    char buffer[8] = {0};
    if((split0 == NULL) || (split0 == data_str))
    {
        return NULL;
    }
    len = split0 - data_str;
    if(len > 4)//invalid
    {
        SKWBT_INFO("%s, invalid str , %s", __func__, data_str);
        return NULL;
    }
    memcpy(buffer, data_str, len);
    *value = (int)simple_strtol(buffer, NULL, 10);
    return split0 + 1;//skip ;
}

void skw_parse_wakeup_adv_conf(char *data_str, Wakeup_ADV_Info_St *wakeup_adv_info)
{
    //WakeupADVData=GPIO_No(decimal);Level(decimal);addr offset(decimal);ADVData(Hex);Mask(Hex)
    int str_len = strlen(data_str);
    char *base_ptr = data_str;
    char *split0, *split1;
    uint8_t adv_grp_nums = 0, adv_len = 0, mask_len;
    uint8_t gpio_no = 0, level = 0, addr_offset;
    uint8_t i = 0, j = 0, k;
    Wakeup_ADV_Grp_St *adv_grp;
    int total_len = 0;

    wakeup_adv_info->data_len = 0;
    if(str_len > 512)
    {
        SKWBT_INFO("%s, invalid config str, %s", __func__, data_str);
        return ;
    }
    if((base_ptr = skwbt_config_get_uint8(base_ptr, &gpio_no)) == NULL)
    {
        return ;
    }
    if((base_ptr = skwbt_config_get_uint8(base_ptr, &level)) == NULL)
    {
        return ;
    }
    for(k = 0; k < BLE_ADV_WAKEUP_GRP_NUMS; k++)
    {
        //addr offset(decimal);ADVData(Hex);Mask(Hex)
        if((base_ptr = skwbt_config_get_uint8(base_ptr, &addr_offset)) == NULL)
        {
            break;
        }
        if((addr_offset == 1) || (addr_offset > 26))
        {
            SKWBT_INFO("%s, invalid addr_offset , %s", __func__, data_str);
            return ;
        }
        adv_grp = &wakeup_adv_info->adv_group[k];
        split0 = strchr(base_ptr, ';');
        if(split0 == NULL)
        {
            SKWBT_INFO("%s, invalid config , %s", __func__, data_str);
            return ;
        }
        split1 = strchr(split0 + 1, ';');

        adv_len = split0 - base_ptr;
        adv_grp->addr_offset = addr_offset;
        adv_grp->grp_len = adv_len + 2;//add addr_offset & self length

        split0 ++;//skip ;
        if(split1 == NULL)
        {
            mask_len = data_str + str_len - split0;
        }
        else
        {
            mask_len = split1 - split0;
        }
        if(mask_len != adv_len)
        {
            SKWBT_INFO("%s, mask_len != adv_len , %s", __func__, data_str);
            return ;
        }
        SKWBT_INFO("grp len:%d, adv_len:%d", adv_grp->grp_len, adv_len);
        for(i = 0, j = 0; i < adv_len; j ++, i += 2)
        {
            adv_grp->data[j] = (skw_char2hex(base_ptr[i]) << 4) | skw_char2hex(base_ptr[i + 1]);
            adv_grp->mask[j] = (skw_char2hex(split0[i]) << 4) | skw_char2hex(split0[i + 1]);
        }
        total_len += adv_grp->grp_len;
        adv_grp_nums ++;
        if(split1 == NULL)
        {
            break;
        }
        base_ptr = split1 + 1;
    }
    wakeup_adv_info->data_len = total_len;//not contain gpio, level, grp_nums, gpio_no
    wakeup_adv_info->grp_nums = adv_grp_nums;
    wakeup_adv_info->gpio_no = gpio_no;
    wakeup_adv_info->level = level;

    SKWBT_INFO("ADV str len:%d, gpio:%d, level:%d, adv_grp_nums:%d, total_len:%d, Data:%s", str_len, gpio_no, level, adv_grp_nums, total_len, data_str);
}

EXPORT_SYMBOL_GPL(skw_parse_wakeup_adv_conf);

