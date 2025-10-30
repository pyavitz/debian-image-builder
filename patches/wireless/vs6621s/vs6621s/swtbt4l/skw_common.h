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


#ifndef __SKW_COMMON_H__
#define __SKW_COMMON_H__

#include <linux/types.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>


#define BT_HCI_LOG_EN  0
#define BT_CP_LOG_EN   0

//WakeupADVData=gpio No;effactive level;addr offset;ADVData;Mask  ### ADVData size must be equal Mask size
//#define BLE_WAKEUP_ADV_INFO "19;1;0;020106031980010FFF00112233AABBCCDD;0000FF0000FFFFFF000000000000000000"

#define SKWBT_LOG_PORT_EN 1


#ifndef INCLUDE_NEW_VERSION
#define INCLUDE_NEW_VERSION 0
#endif


#define MAX_BT_LOG_SIZE (5*1024*1024) //500M


#define SEEKWAVE_BT_LOG_PATH     "/mnt/skwbt"
#define NV_FILE_NAME             "sv6160.nvbin"
#define NV_FILE_NAME_6316        "sv6316.nvbin"
#define NV_FILE_NAME_6160_LITE   "sv6160lite.nvbin"

//#define BD_ADDR_FILE_PATH        ""


#define BD_ADDR_LEN 6



#define SKWBT_INFO(format, ...)     pr_info("[SKWBT_INFO] "format, ##__VA_ARGS__)
#define SKWBT_ERROR(format, ...)    pr_err("[SKWBT_ERROR] "format, ##__VA_ARGS__)

#define SKW_CHIPID_6316       0x5301
#define SKW_CHIPID_6160       0x0017
#define SKW_CHIPID_6160_LITE  0x5302



#define NV_FILE_RD_BLOCK_SIZE    252

#define HCI_CMD_READ_LOCAL_VERSION_INFO 0x1001

#define HCI_CMD_SKW_BT_NVDS                 0xFC80
#define HCI_CMD_WRITE_BD_ADDR               0xFC82
#define HCI_CMD_WRITE_BT_STATE              0xFE80
#define HCI_CMD_WRITE_WAKEUP_ADV_DATA       0xFC84
#define HCI_CMD_WRITE_WAKEUP_ADV_ENABLE     0xFC85
#define HCI_CMD_WRITE_WAKEUP_ADV_ENABLE_PLT 0xFC86

#define HCI_COMMAND_COMPLETE_EVENT      0x0E
#define HCI_EVT_HARDWARE_ERROR          0x10

#define NV_TAG_DSP_LOG_SETTING  0x05

#ifndef UINT8_TO_STREAM
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#endif


#ifndef UINT16_TO_STREAM
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#endif


#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
#define skw_read   kernel_read
#define skw_write  kernel_write
#else
#define skw_read   vfs_read
#define skw_write  vfs_write
#endif

#ifdef CONFIG_NO_GKI
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

typedef struct
{
    uint8_t  type;
    uint8_t  evt_op;
    uint8_t  len;
    uint8_t  nums;
    uint16_t cmd_op;
    uint8_t  status;
} __packed hci_cmd_cmpl_evt_st;


typedef struct
{
    uint8_t grp_len;
    uint8_t addr_offset;
    uint8_t data[32];
    uint8_t mask[32];
} Wakeup_ADV_Grp_St;

#define BLE_ADV_WAKEUP_GRP_NUMS 3

typedef struct
{
    uint8_t gpio_no;
    uint8_t level;
    uint8_t data_len;//total len
    uint8_t grp_nums;
    Wakeup_ADV_Grp_St adv_group[BLE_ADV_WAKEUP_GRP_NUMS];
} Wakeup_ADV_Info_St;

typedef enum
{
    WAKEUP_OP_DISABLE = 0x00,
    WAKEUP_OP_SCAN_ONLY,
    WAKEUP_OP_SCAN_ADV_HOST,//use host paramater
    WAKEUP_OP_SCAN_ADV_SKW //use skw data: SeekwaveBT
} le_wakeup_op_enum;

ssize_t skw_file_write(struct file *, const void *, size_t);

ssize_t skw_file_read(struct file *fp, void *buf, size_t len);

char skw_file_copy(char *scr_file, char *des_file);

void skw_bd_addr_gen_init(void);

char skw_get_bd_addr(unsigned char *buffer);

int skw_strlen(char *str);

unsigned char skw_char2hex(char ch);

void skwbt_log_port_init(void);

void skwbt_log_port_set_pdata(void *pdata);

void skwbt_log_port_exit(void);

void skwbt_log_port_data_write(uint8_t is_from_cp, uint8_t *data, uint16_t data_len);

void skwbt_log_port_set_bt_open(char is_open);



#endif
