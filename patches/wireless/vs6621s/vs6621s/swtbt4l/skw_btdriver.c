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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/platform_data/skw_platform_data.h>
// #include <skw_platform_data.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/moduleparam.h>

#include "skw_btsnoop.h"
#include "skw_log.h"
#include "skw_common.h"

#define VERSION "0.1"




enum
{
    BT_STATE_DEFAULT = 0x00,
    BT_STATE_CLOSE,
    BT_STATE_REMOVE
};

int skwbt_log_disable = 0;
int is_init_mode = 0;
uint16_t chip_version = 0;
static wait_queue_head_t nv_wait_queue;
static wait_queue_head_t recovery_wait_queue;
static wait_queue_head_t close_wait_queue;
Wakeup_ADV_Info_St wakeup_adv_info = {0};
char *bd_addr = NULL;

static atomic_t evt_recv;
static atomic_t cmd_reject;
static atomic_t atomic_close_sync;//make sure running close func before remove func

module_param(bd_addr, charp, S_IRUSR);


static int btseekwave_send_frame(struct hci_dev *hdev, struct sk_buff *skb);
int btseekwave_plt_event_notifier(struct notifier_block *nb, unsigned long action, void *param);

int btseekwave_send_hci_command(struct hci_dev *hdev, u16 opcode, int len, char *cmd_pld);

extern int skw_start_bt_service(void);
extern int skw_stop_bt_service(void);
struct btseekwave_data
{
    struct hci_dev   *hdev;
    struct sv6160_platform_data *pdata;

    struct work_struct work;

    struct notifier_block plt_notifier;
    uint8_t plt_notifier_set;
    uint8_t bt_is_open;

    struct sk_buff_head cmd_txq;
    struct sk_buff_head data_txq;
    struct sk_buff_head audio_txq;
};

struct btseekwave_data *skw_data = NULL;


void btseekwave_hci_hardware_error(struct hci_dev *hdev)
{
    struct sk_buff *skb = NULL;
    int len = 3;
    uint8_t hw_err_pkt[4] = {HCI_EVENT_PKT, HCI_EVT_HARDWARE_ERROR, 0x01, 0x00};
    uint8_t *base_ptr = NULL;
    skb = alloc_skb(len, GFP_ATOMIC);
    if (!skb)
    {
        SKWBT_ERROR("%s: failed to allocate mem", __func__);
        return;
    }
    base_ptr = (uint8_t *)skb_put(skb, len);
    if(base_ptr)//for Coverity scan
    {
        memcpy(base_ptr, hw_err_pkt + 1, len);
    }
    bt_cb(skb)->pkt_type = HCI_EVENT_PKT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    hci_recv_frame(hdev, skb);
#else
    hci_recv_frame(skb);
#endif

    SKWBT_INFO("%s enter", __func__);
}


static int btseekwave_tx_packet(int portno, struct btseekwave_data *data, struct sk_buff *skb)
{
    int err = 0;
    u32 *d;
    uint32_t pkt_len = skb->len;

    d = (u32 *)skb->data;

    //SKWBT_INFO("%s enter size %d: 0x%x 0x%x\n", __func__, skb->len, d[0], d[1]);

    if(data->pdata && data->pdata->hw_sdma_tx)
    {
        err = data->pdata->hw_sdma_tx(portno, skb->data, skb->len);
    }
    if(err < 0)
    {
        SKWBT_ERROR("btseekwave_tx_packet tx failed err:%d, pkt_len:%d", err, pkt_len);
        return err;
    }
    kfree_skb(skb);

    data->hdev->stat.byte_tx += pkt_len;

    //SKWBT_INFO("%s, pkt:%d, users:%d \n", __func__, bt_cb((skb))->pkt_type, skb->users.refs.counter);

    return 0;
}

static void btseekwave_work(struct work_struct *work)
{
    struct btseekwave_data *data = container_of(work, struct btseekwave_data, work);
    struct sk_buff *skb;
    int err = 0;

    //SKWBT_INFO("%s %s", __func__, data->hdev->name);

    if(atomic_read(&cmd_reject))
    {
        return ;
    }

    while ((skb = skb_dequeue(&data->cmd_txq)))
    {
        err = btseekwave_tx_packet(data->pdata->cmd_port, data, skb);
        if (err < 0)
        {
            data->hdev->stat.err_tx++;
            skb_queue_head(&data->cmd_txq, skb);
            SKWBT_ERROR("btseekwave_tx_packet command failed len: %d\n", err);
            break;
        }
    }

    while (err >= 0 && (skb = skb_dequeue(&data->data_txq)))
    {
        err = btseekwave_tx_packet(data->pdata->data_port, data, skb);
        if (err < 0)
        {
            data->hdev->stat.err_tx++;
            skb_queue_head(&data->data_txq, skb);
            SKWBT_ERROR("btseekwave_tx_packet data failed len: %d\n", err);
            break;
        }
    }
    while (err >= 0 && (skb = skb_dequeue(&data->audio_txq)))
    {
        err = btseekwave_tx_packet(data->pdata->audio_port, data, skb);
        if (err < 0)
        {
            data->hdev->stat.err_tx++;
            skb_queue_head(&data->audio_txq, skb);
            SKWBT_ERROR("btseekwave_tx_packet audio failed len: %d\n", err);
            break;
        }
    }
//  SKWBT_INFO("btseekwave_work done\n");
}


static int btseekwave_rx_packet(struct btseekwave_data *data, u8 pkt_type, void *buf, int c_len)
{
    struct sk_buff *skb;
    //SKWBT_INFO("rx hci pkt len = %d, pkt_type:%d", c_len, pkt_type);

    skb = bt_skb_alloc(c_len, GFP_ATOMIC);
    if (!skb)
    {
        SKWBT_ERROR("skwbt alloc skb failed, len: %d\n", c_len);
        return 0;
    }
    bt_cb((skb))->expect = 0;
    skb->dev = (void *) data->hdev;
    bt_cb(skb)->pkt_type = pkt_type;
    memcpy(skb_put(skb, c_len), buf, c_len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    hci_recv_frame(data->hdev, skb);
#else
    hci_recv_frame(skb);
#endif

    return 0;
}

int btseekwave_rx_complete(int portno, struct scatterlist *priv, int size, void *buf)
{
    int ret = 0;
    struct btseekwave_data *data = (struct btseekwave_data *)priv;
    u8 pkt_type = 0;

    //SKWBT_INFO("btseekwave_rx_complete size=%d, is_init_mode:%d, data:0x%04X", size, is_init_mode, buf ? (*(uint32_t *)buf) : 0xFFFFFFFF);
    if(size == 0)
    {
        return 0;
    }
    else if(size < 0)//CP assert/exception
    {
        SKWBT_ERROR("cp exception\n");
        return 0;
    }
#if SKWBT_LOG_PORT_EN
    skwbt_log_port_data_write(1, (uint8_t *)buf, (uint16_t)size);
#endif

    pkt_type = *((u8 *)buf);
    if(HCI_EVENT_SKWLOG == pkt_type)
    {
#if BT_CP_LOG_EN
        skwlog_write(buf, size);
#endif
        return 0;
    }


    if((HCI_EVENT_PKT == pkt_type) || (HCI_ACLDATA_PKT == pkt_type) || (HCI_SCODATA_PKT == pkt_type))
    {
#if BT_HCI_LOG_EN
        skw_btsnoop_capture(buf, 1);
#endif

        if(is_init_mode)//command complete event
        {
            hci_cmd_cmpl_evt_st *hci_evt = (hci_cmd_cmpl_evt_st *)buf;
            if((HCI_EVENT_PKT == pkt_type) && (HCI_COMMAND_COMPLETE_EVENT == hci_evt->evt_op) && (HCI_CMD_READ_LOCAL_VERSION_INFO == hci_evt->cmd_op))
            {
                struct hci_rp_read_local_version *ver;
                ver = (struct hci_rp_read_local_version *)(buf + 6);
                chip_version = le16_to_cpu(ver->hci_rev);
                SKWBT_INFO("%s, chip version:0x%X", __func__, chip_version);
            }

            atomic_inc(&evt_recv);
            wake_up(&nv_wait_queue);
            SKWBT_INFO("init cmd response: 0x%x \n", *((u32 *)(buf + 3)));
            return 0;
        }

        ret = btseekwave_rx_packet(data, pkt_type, buf + 1, size - 1);
    }
    else
    {
        SKWBT_ERROR("err hci packet: %x, len:%d\n", pkt_type, size);
    }

    return ret;
}

struct sk_buff *btseekwave_prepare_cmd(struct hci_dev *hdev, u16 opcode, u32 plen,
                                       const void *param)
{
    int len = HCI_COMMAND_HDR_SIZE + plen;
    struct hci_command_hdr *hdr;
    struct sk_buff *skb;

    skb = bt_skb_alloc(len, GFP_ATOMIC);
    if (!skb)
    {
        return NULL;
    }

    hdr = (struct hci_command_hdr *) skb_put(skb, HCI_COMMAND_HDR_SIZE);
    hdr->opcode = cpu_to_le16(opcode);
    hdr->plen   = plen;

    if (plen)
    {
        uint8_t *base_ptr = (uint8_t *)skb_put(skb, plen);
        if(base_ptr)//for Coverity scan
        {
            memcpy(base_ptr, param, plen);
        }
    }

    bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;

    return skb;
}


void btseekwave_write_bd_addr(struct hci_dev *hdev)
{
    u8 cmd_pld[32] = {0x00};
    //struct sk_buff *skb;
    int ret;
    if(bd_addr)
    {
        uint8_t i = 0, j, size = skw_strlen(bd_addr);
        SKWBT_INFO("%s bd addr:%s", __func__, bd_addr);
        //BC:9A:98:86:74:62
        if(size != 17)
        {
            return ;
        }
        for (i = 16, j = 0; j < 6; i -= 3, j++)
        {
            cmd_pld[j] = (skw_char2hex(bd_addr[i - 1]) << 4) | skw_char2hex(bd_addr[i]);
        }
    }
    else
    {
        if(!skw_get_bd_addr(cmd_pld))//bd addr is invalid
        {
            return ;
        }
    }

    ret = btseekwave_send_hci_command(hdev, HCI_CMD_WRITE_BD_ADDR, BD_ADDR_LEN, cmd_pld);
    if(ret < 0)
    {
        SKWBT_ERROR("%s write bd_addr timeout", __func__);
    }
}


/*
0: success
other:fail
*/
int btseekwave_send_hci_command(struct hci_dev *hdev, u16 opcode, int len, char *cmd_pld)
{
    struct sk_buff *skb;
    int ret = 0, i;

    skb = btseekwave_prepare_cmd(hdev, opcode, len, cmd_pld);
    if(!skb)
    {
        SKWBT_ERROR("%s no memory for command", __func__);
        return -1;
    }
    //waiting controller response
    atomic_set(&evt_recv, 0);

    ret = btseekwave_send_frame(hdev, skb);
    if(ret != 0)
    {
        SKWBT_ERROR("%s cmd send fail, ret:%d", __func__, ret);
        return -1;
    }

    for(i = 0; i < 3; i++)
    {
        ret = wait_event_timeout(nv_wait_queue, (atomic_read(&evt_recv)), msecs_to_jiffies(1000));
        if((ret > 0) || (atomic_read(&evt_recv)))
        {
            return 0;
        }
        SKWBT_INFO("%s cp response timeout, ret:%d", __func__, ret);
        if(ret == 0)//timeout
        {
            break;
        }
    }

    return -1;
}

void btseekwave_write_ble_wakeup_adv_info(struct hci_dev *hdev)
{
    uint8_t adv_data_len = wakeup_adv_info.data_len;
    if(adv_data_len > 0)
    {
        uint8_t p_buf[256] = {0x00};
        uint8_t *ptr = p_buf;
        uint8_t i, adv_len;
        uint8_t pld_len = adv_data_len + 4;//add the length of gpio & level & grp nums & total len
        Wakeup_ADV_Grp_St *adv_grp;

        UINT8_TO_STREAM(ptr, wakeup_adv_info.gpio_no);
        UINT8_TO_STREAM(ptr, wakeup_adv_info.level);
        UINT8_TO_STREAM(ptr, wakeup_adv_info.grp_nums);
        UINT8_TO_STREAM(ptr, adv_data_len);
        for(i = 0; i < wakeup_adv_info.grp_nums; i++)
        {
            adv_grp = &wakeup_adv_info.adv_group[i];
            UINT8_TO_STREAM(ptr, adv_grp->grp_len);
            UINT8_TO_STREAM(ptr, adv_grp->addr_offset);
            adv_len = (adv_grp->grp_len - 2) >> 1;

            SKWBT_INFO("grp len:%d, adv_len:%d", adv_grp->grp_len, adv_len);

            memcpy(ptr, adv_grp->data, adv_len);
            ptr += adv_len;
            memcpy(ptr, adv_grp->mask, adv_len);
            ptr += adv_len;
        }
        btseekwave_send_hci_command(hdev, HCI_CMD_WRITE_WAKEUP_ADV_DATA, pld_len, p_buf);
    }
}

void btseekwave_write_ble_wakeup_adv_enable(le_wakeup_op_enum enable_op)
{
    if((wakeup_adv_info.data_len > 0) && skw_data && (skw_data->bt_is_open))
    {
        char buffer[2] = {0x00};
        struct sk_buff *skb;
        struct hci_dev *hdev = skw_data->hdev;

        buffer[0] = enable_op;

        skb = btseekwave_prepare_cmd(hdev, HCI_CMD_WRITE_WAKEUP_ADV_ENABLE_PLT, 1, buffer);
        if(!skb)
        {
            SKWBT_ERROR("%s no memory for nv command", __func__);
            return ;
        }
        SKWBT_INFO("%s", __func__);
        btseekwave_send_frame(hdev, skb);

        msleep(5);
    }
}
EXPORT_SYMBOL_GPL(btseekwave_write_ble_wakeup_adv_enable);

static void btseekwave_port_close(struct btseekwave_data *data)
{
    if(data && data->pdata)
    {
        data->bt_is_open = 0;

        if(data->pdata->modem_unregister_notify && data->plt_notifier_set)
        {
            data->plt_notifier_set = 0;
            data->pdata->modem_unregister_notify(&data->plt_notifier);
        }
#if INCLUDE_NEW_VERSION
        if(data->pdata->service_stop)
        {
            data->pdata->service_stop();
        }
        else
        {
            SKWBT_ERROR("func %s service_stop not exist", __func__);
        }
#else
        skw_stop_bt_service();
#endif
        if(data->pdata->close_port)
        {
            data->pdata->close_port(data->pdata->cmd_port);
            if(data->pdata->data_port != 0)
            {
                data->pdata->close_port(data->pdata->data_port);
            }
            if(data->pdata->audio_port != 0)
            {
                data->pdata->close_port(data->pdata->audio_port);
            }
        }
    }

}


int btseekwave_download_nv(struct hci_dev *hdev)
{
    int page_offset = 0, ret = 0, len = 0;
    u8 *cmd_pld = NULL;
    const struct firmware *fw;
    int err = 0, count = 0;
    uint8_t log_disable = 1, cp_log_disable = 1;
    SKWBT_INFO("%s", __func__);

    is_init_mode = 1;
    chip_version = SKW_CHIPID_6160;

    ret = btseekwave_send_hci_command(hdev, HCI_CMD_READ_LOCAL_VERSION_INFO, 0, NULL);
    if(ret < 0)
    {
        SKWBT_ERROR("%s, read local version err", __func__);
        if(skw_data)
        {
            skw_data->pdata->modem_assert();
        }
        return -1;
    }

    if(SKW_CHIPID_6316 == chip_version)
    {
        err = request_firmware(&fw, NV_FILE_NAME_6316, &hdev->dev);
    }
    else if(SKW_CHIPID_6160_LITE == chip_version)
    {
        err = request_firmware(&fw, NV_FILE_NAME_6160_LITE, &hdev->dev);
    }
    else
    {
        err = request_firmware(&fw, NV_FILE_NAME, &hdev->dev);
    }
    if (err < 0)
    {
        SKWBT_ERROR("nv file load fail, BT Controller Version:0x%04X", chip_version);
        return err;
    }
    cmd_pld = (u8 *)kzalloc(512, GFP_KERNEL);
    if(cmd_pld == NULL)
    {
        SKWBT_ERROR("%s malloc fail", __func__);
        release_firmware(fw);
        return -1;
    }
#if ((BT_CP_LOG_EN == 1) || (BT_HCI_LOG_EN == 1))
    skwbt_log_disable = 0;
#endif

#if BT_CP_LOG_EN
    cp_log_disable = 0;
#endif


    if((SKW_CHIPID_6316 == chip_version) || (SKW_CHIPID_6160_LITE == chip_version))
    {
        int total_len = 0;
        int nv_pkt_len = 0;
        uint8_t nv_tag = 0;
        uint8_t *base_ptr = NULL;
        count = 4;//skip header
        while(count < fw->size)
        {
            nv_tag = fw->data[count];
            nv_pkt_len = fw->data[count + 2] + 3;
            if((nv_pkt_len + total_len) >= NV_FILE_RD_BLOCK_SIZE)
            {
                cmd_pld[0] = (char)page_offset;
                cmd_pld[1] = (char)total_len;//para len
                ret = btseekwave_send_hci_command(hdev, HCI_CMD_SKW_BT_NVDS, total_len + 2, cmd_pld);
                if(ret < 0)
                {
                    //return -1;
                    total_len = 0;
                    err = -1;
                    break;
                }
                page_offset ++;
                total_len = 0;
                continue;
            }
            base_ptr = cmd_pld + 2 + total_len;
            if(base_ptr)
            {
                memcpy(base_ptr, fw->data + count, nv_pkt_len);
            }
            if(nv_tag == NV_TAG_DSP_LOG_SETTING)
            {
                log_disable = fw->data[count + 3];
                if(cp_log_disable)
                {
                    log_disable = 1;
                }
                if(total_len < NV_FILE_RD_BLOCK_SIZE)//for Coverity scan
                {
                    *(cmd_pld + 2 + total_len + 3) = log_disable;
                }
                SKWBT_INFO("%s log_disable from NV:%d, skwbt_log_disable:%d", __func__, log_disable, cp_log_disable);
            }
            count += nv_pkt_len;
            total_len += nv_pkt_len;
        }
        if(total_len > 0)
        {
            cmd_pld[0] = (char)page_offset;
            cmd_pld[1] = (char)total_len;//para len
            ret = btseekwave_send_hci_command(hdev, HCI_CMD_SKW_BT_NVDS, total_len + 2, cmd_pld);
            if(ret < 0)
            {
                SKWBT_ERROR("%s, line:%d, cp response timeout", __func__, __LINE__);
            }
        }
    }
    else
    {
        log_disable = fw->data[0x131];
        SKWBT_INFO("%s log_disable from NV:%d, skwbt_log_disable:%d", __func__, log_disable, cp_log_disable);
        while(count < fw->size)
        {
            len = NV_FILE_RD_BLOCK_SIZE;
            if((fw->size - count) < NV_FILE_RD_BLOCK_SIZE)
            {
                len = fw->size - count;
            }
            cmd_pld[0] = (char)page_offset;
            cmd_pld[1] = (char)len;//para len
            memcpy(cmd_pld + 2, fw->data + count, len);
            count += len;

            if(1 == page_offset)
            {
                if(cp_log_disable)
                {
                    log_disable = 1;
                }
                *(cmd_pld + 2 + 53) = log_disable;
            }

            ret = btseekwave_send_hci_command(hdev, HCI_CMD_SKW_BT_NVDS, len + 2, cmd_pld);
            if(ret < 0)
            {
                SKWBT_ERROR("%s, line:%d, cp response timeout", __func__, __LINE__);
                break;
            }
            page_offset ++;
        }
    }

    if(err == 0)
    {
        btseekwave_write_bd_addr(hdev);
        btseekwave_write_ble_wakeup_adv_info(hdev);
    }

    kfree(cmd_pld);
    release_firmware(fw);
    is_init_mode = 0;

    return err;
}


static int btseekwave_open(struct hci_dev *hdev)
{
    struct btseekwave_data *data = hci_get_drvdata(hdev);
    int err = -1;

    SKWBT_INFO("%s enter...", __func__);

    if(atomic_read(&cmd_reject))
    {
        int ret = wait_event_timeout(recovery_wait_queue,
                                     (!atomic_read(&cmd_reject)),
                                     msecs_to_jiffies(2000));
        if(!ret)
        {
            SKWBT_ERROR("%s timeout", __func__);
            return ret;
        }
    }

    if(data && data->pdata && data->pdata->open_port)
    {
        SKWBT_INFO("%s, cmd_port:%d, mode data_port:%d, audio_port:%d\n", __func__, data->pdata->cmd_port, data->pdata->data_port, data->pdata->audio_port);

        err = data->pdata->open_port(data->pdata->cmd_port, btseekwave_rx_complete,  data);
        if(err < 0)
        {
            SKWBT_ERROR("command port open fail, ret:%d", err);
            return err;
        }

        if(data->pdata->data_port != 0)
        {
            err = data->pdata->open_port(data->pdata->data_port, btseekwave_rx_complete, data);
            if(err < 0)
            {
                SKWBT_ERROR("data port open fail, ret:%d", err);
                return err;
            }
        }
        if(data->pdata->audio_port != 0)
        {
            err = data->pdata->open_port(data->pdata->audio_port, btseekwave_rx_complete, data);
            if(err < 0)
            {
                SKWBT_ERROR("audio port open fail, ret:%d", err);
                return err;
            }
        }
#if INCLUDE_NEW_VERSION
        if(data->pdata->service_start)
        {
            err = data->pdata->service_start();
            if(err != 0)
            {
                SKWBT_ERROR("func %s service_start err:%d", __func__, err);
                return err;
            }
        }
        else
        {
            SKWBT_ERROR("func %s service_start not exist", __func__);
            return -1;
        }
#else
        err = skw_start_bt_service();
        if(err != 0)
        {
            SKWBT_ERROR("%s service_start err:%d", __func__, err);
            return err;
        }
#endif
        err = btseekwave_download_nv(hdev);
        if(err == 0)
        {
            data->bt_is_open = 1;
            if(data->plt_notifier_set == 0)
            {
                data->plt_notifier.notifier_call = btseekwave_plt_event_notifier;
                data->pdata->modem_register_notify(&data->plt_notifier);
                data->plt_notifier_set = 1;
            }
        }
        else
        {
            btseekwave_port_close(data);
        }

    }
    atomic_set(&atomic_close_sync, 0);
    return err;
}

void btseekwave_write_bt_state(struct hci_dev *hdev)
{
    //char buffer[10] = {0x01, 0x80, 0xFE, 0x01, 0x00};
    u8 cmd_pld[5] = {0x00};
    struct sk_buff *skb = btseekwave_prepare_cmd(hdev, HCI_CMD_WRITE_BT_STATE, 1, cmd_pld);
    if(skb)
    {
        btseekwave_send_frame(hdev, skb);
        msleep(15);
    }
}


static int btseekwave_close(struct hci_dev *hdev)
{
    struct btseekwave_data *data = hci_get_drvdata(hdev);
    int state = 0;

    SKWBT_INFO("%s enter...\n", __func__);

    if(data && (data->pdata->data_port == 0))
    {
#if INCLUDE_NEW_VERSION

#else
        btseekwave_write_bt_state(hdev);
#endif
    }

    if(atomic_read(&cmd_reject))
    {
        int ret = wait_event_timeout(recovery_wait_queue,
                                     (!atomic_read(&cmd_reject)),
                                     msecs_to_jiffies(2000));
        if(!ret)
        {
            SKWBT_ERROR("%s timeout, ret:%d", __func__, ret);
        }
    }
    btseekwave_port_close(data);

    state = atomic_read(&atomic_close_sync);
    SKWBT_INFO("func %s, atomic_read:%d", __func__, state);

    if(state == BT_STATE_DEFAULT)
    {
        atomic_set(&atomic_close_sync, BT_STATE_CLOSE);
    }
    else
    {
        atomic_set(&atomic_close_sync, BT_STATE_CLOSE);
        wake_up(&close_wait_queue);
    }
    return 0;
}

static int btseekwave_flush(struct hci_dev *hdev)
{
    struct btseekwave_data *data = hci_get_drvdata(hdev);

    SKWBT_INFO("%s", hdev->name);

    if (work_pending(&data->work))
    {
        cancel_work_sync(&data->work);
    }

    skb_queue_purge(&data->cmd_txq);
    skb_queue_purge(&data->data_txq);
    skb_queue_purge(&data->audio_txq);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
//
#else
/*for low version*/
static int btseekwave_send_frame_lv(struct sk_buff *skb)
{
    struct hci_dev *hdev = (struct hci_dev *) skb->dev;
    return btseekwave_send_frame(hdev, skb);
}
#endif

static int btseekwave_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
    struct btseekwave_data *data = hci_get_drvdata(hdev);
    u8 pkt_type = bt_cb(skb)->pkt_type;
    u8 *d = skb_push(skb, 1);
    *d = pkt_type;

    if(data->pdata == NULL)
    {
        SKWBT_ERROR("%s pointer is null", __func__);
        return -EILSEQ;
    }
    if((pkt_type == HCI_COMMAND_PKT) || ((pkt_type == HCI_ACLDATA_PKT) && (data->pdata->data_port == 0))
            || ((pkt_type == HCI_SCODATA_PKT) && (data->pdata->audio_port == 0)))
    {
        hdev->stat.cmd_tx++;
        skb_queue_tail(&data->cmd_txq, skb);
    }
    else if(pkt_type == HCI_ACLDATA_PKT)
    {
        hdev->stat.acl_tx++;
        skb_queue_tail(&data->data_txq, skb);
    }
    else if(pkt_type == HCI_SCODATA_PKT)
    {
        skb_queue_tail(&data->audio_txq, skb);
        hdev->stat.sco_tx++;
    }
    else
    {
        return -EILSEQ;
    }

#if BT_HCI_LOG_EN
    skw_btsnoop_capture(skb->data, 0);
#endif

#if SKWBT_LOG_PORT_EN
    skwbt_log_port_data_write(0, skb->data, (uint16_t)skb->len);
#endif

    schedule_work(&data->work);

    return 0;
}


static int btseekwave_setup(struct hci_dev *hdev)
{
    SKWBT_INFO("%s", __func__);
    return 0;
}


/*
must be in DEVICE_ASSERT_EVENT to DEVICE_DUMPDONE_EVENT closing USB
*/
int btseekwave_plt_event_notifier(struct notifier_block *nb, unsigned long action, void *param)
{
    SKWBT_INFO("%s, action:%d", __func__, (int)action);
    if(skw_data == NULL)
    {
        return 0;
    }
#if 1
    switch(action)
    {
        case DEVICE_ASSERT_EVENT:
        {
            //make surce host data cann't send to plt driver before close usb
            atomic_set(&cmd_reject, 1);
#if INCLUDE_NEW_VERSION
            if((skw_data) && (skw_data->pdata) && (skw_data->pdata->service_stop))
            {
                skw_data->pdata->service_stop();
            }
            else
            {
                SKWBT_ERROR("func %s service_stop not exist", __func__);
            }
#else
            skw_stop_bt_service();
#endif
        }
        break;
        case DEVICE_BSPREADY_EVENT://
        {
            if(atomic_read(&cmd_reject))
            {
                struct btseekwave_data *data = skw_data;//container_of(nb, struct btseekwave_data, plt_notifier);
                atomic_set(&cmd_reject, 0);
                wake_up(&recovery_wait_queue);

                if(data)
                {
                    btseekwave_flush(data->hdev);
                    btseekwave_hci_hardware_error(data->hdev);//report to host
                }
            }
        }
        break;
        case DEVICE_DUMPDONE_EVENT:
        {

        }
        break;
        case DEVICE_BLOCKED_EVENT:
        {

        }
        break;
        default:
        {

        }
        break;

    }
#endif
    return NOTIFY_OK;
}

static int btseekwave_probe(struct platform_device *pdev)
{
    struct btseekwave_data *data;
    struct device *dev = &pdev->dev;
    struct sv6160_platform_data *pdata = dev->platform_data;
    struct hci_dev *hdev;
    int err;
    if(pdata == NULL)
    {
        SKWBT_ERROR("%s pdata is null", __func__);
        return -ENOMEM;
    }

    SKWBT_INFO("%s pdev name %s\n", __func__, pdata->port_name);

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
    {
        SKWBT_ERROR("%s alloc fail", __func__);
        return -ENOMEM;
    }

    skw_data = data;
    data->plt_notifier_set = 0;
    data->bt_is_open = 0;

    data->pdata = pdata;

    INIT_WORK(&data->work, btseekwave_work);

    skb_queue_head_init(&data->cmd_txq);
    skb_queue_head_init(&data->data_txq);
    skb_queue_head_init(&data->audio_txq);

    hdev = hci_alloc_dev();
    if (!hdev)
    {
        return -ENOMEM;
    }

    hdev->bus = HCI_SDIO;
    hci_set_drvdata(hdev, data);

    data->hdev = hdev;

    SET_HCIDEV_DEV(hdev, dev);

    hdev->open     = btseekwave_open;
    hdev->close    = btseekwave_close;
    hdev->flush    = btseekwave_flush;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    hdev->send     = btseekwave_send_frame;
#else
    hdev->send     = btseekwave_send_frame_lv;
#endif
    hdev->setup    = btseekwave_setup;

    atomic_set(&hdev->promisc, 0);

    //set_bit(HCI_QUIRK_SIMULTANEOUS_DISCOVERY, &hdev->quirks);
    //set_bit(HCI_QUIRK_NO_SUSPEND_NOTIFIER, &hdev->quirks);

    err = hci_register_dev(hdev);
    if (err < 0)
    {
        hci_free_dev(hdev);
        return err;
    }

    platform_set_drvdata(pdev, data);

    skw_bd_addr_gen_init();
    atomic_set(&cmd_reject, 0);
    atomic_set(&atomic_close_sync, BT_STATE_DEFAULT);

#if SKWBT_LOG_PORT_EN
    skwbt_log_port_init();
    skwbt_log_port_set_pdata(pdata);
#endif

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
static void btseekwave_remove(struct platform_device *pdev)
#else
static int btseekwave_remove(struct platform_device *pdev)
#endif
{
    int state = atomic_read(&atomic_close_sync);

    SKWBT_INFO("func %s, atomic_read:%d", __func__, state);

    atomic_set(&cmd_reject, 0);
    if(BT_STATE_DEFAULT == state)
    {
        int ret;
        atomic_set(&atomic_close_sync, BT_STATE_REMOVE);
        ret = wait_event_timeout(close_wait_queue,
                                 (BT_STATE_CLOSE == atomic_read(&atomic_close_sync)),
                                 msecs_to_jiffies(500));
        if(!ret)
        {
            SKWBT_ERROR("%s timeout, ret:%d", __func__, ret);
        }
    }

    atomic_set(&atomic_close_sync, BT_STATE_DEFAULT);
    skw_data = NULL;
    if(pdev)
    {
        struct btseekwave_data *data = platform_get_drvdata(pdev);
        struct hci_dev *hdev;

        if (!data)
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
            return;
#else
            return 0;
#endif
        }
        hdev = data->hdev;
        data->bt_is_open = 0;

        if(data->pdata && data->pdata->modem_unregister_notify && data->plt_notifier_set)
        {
            SKWBT_INFO("func %s modem_unregister_notify", __func__);
            data->pdata->modem_unregister_notify(&data->plt_notifier);
            data->plt_notifier_set = 0;
        }

        btseekwave_flush(hdev);

        platform_set_drvdata(pdev, NULL);

        hci_unregister_dev(hdev);

        hci_free_dev(hdev);

#if SKWBT_LOG_PORT_EN
        skwbt_log_port_exit();
#endif
    }
    SKWBT_INFO("func %s end", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 15)
    return;
#else
    return 0;
#endif
}

static struct platform_driver  btseekwave_driver =
{
    .driver = {
        .name   = (char *)"btseekwave",
        .bus    = &platform_bus_type,
        .pm     = NULL,
    },
    .probe      = btseekwave_probe,
    .remove     = btseekwave_remove,
};

int  btseekwave_init(void)
{
    SKWBT_INFO("Seekwave Bluetooth driver ver %s\n", VERSION);
    init_waitqueue_head(&nv_wait_queue);
    init_waitqueue_head(&recovery_wait_queue);
    init_waitqueue_head(&close_wait_queue);
    atomic_set(&evt_recv, 0);

    wakeup_adv_info.data_len = 0;

#ifdef BLE_WAKEUP_ADV_INFO
    skw_parse_wakeup_adv_conf(BLE_WAKEUP_ADV_INFO, &wakeup_adv_info);
#endif

#if BT_HCI_LOG_EN
    skw_btsnoop_init();
#endif
#if BT_CP_LOG_EN
    skwlog_init();
#endif
    return platform_driver_register(&btseekwave_driver);
}

void  btseekwave_exit(void)
{
#if BT_HCI_LOG_EN
    skw_btsnoop_close();
#endif
#if BT_CP_LOG_EN
    skwlog_close();
#endif

    platform_driver_unregister(&btseekwave_driver);
}

module_init(btseekwave_init);
module_exit(btseekwave_exit);

MODULE_DESCRIPTION("Seekwave Bluetooth driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");

