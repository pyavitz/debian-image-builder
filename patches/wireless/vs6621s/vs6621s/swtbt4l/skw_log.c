/******************************************************************************
 *
 *  Copyright (C) 2020-2021 SeekWave Technology
 *
 *
 ******************************************************************************/

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <linux/timekeeping.h>
#include "skw_common.h"
#include "skw_log.h"
#include <linux/version.h>

#if BT_CP_LOG_EN

static struct mutex skwlog_lock;
extern int skwbt_log_disable;

struct file *skwlog_open(void)
{
    char log_path[256] = {0}, is_new_file = 1;
    int file_size = 0;
    int file_mode = O_CREAT | O_WRONLY | O_TRUNC;
    struct file *fp = NULL;


    snprintf(log_path, 256, "%s/skwbt_cp.log", SEEKWAVE_BT_LOG_PATH);

    fp = filp_open(log_path, O_RDONLY, 0644);
    if(!IS_ERR(fp))//file exist
    {
        file_size = (int)vfs_llseek(fp, 0, SEEK_END);
        //pr_info("bt cp log file size:%d", file_size);
        filp_close(fp, NULL);
		file_mode = O_WRONLY;
    }
    if(file_size >= MAX_BT_LOG_SIZE)
    {
        char tmp_path[256] = {0};
        snprintf(tmp_path, 256, "%s/skwbt_cp.log.last", SEEKWAVE_BT_LOG_PATH);
        skw_file_copy(log_path, tmp_path);
        file_mode = O_CREAT | O_WRONLY | O_TRUNC;
		file_size = 0;
    }
    else if(file_size > 0)
    {
        is_new_file = 0;
    }

    fp = filp_open(log_path, file_mode, 0644);

    if ((fp == NULL) || IS_ERR(fp))
    {
        return NULL;
    }
	fp->f_pos = file_size;
	
    if(is_new_file)
    {
        struct tm tm;
        struct timespec64 tv;
        unsigned char buffer[16] = {0x07, 0xFF, 0x08, 0x00, 0x01, 0xD0, 0x55, 0x55};
        ktime_get_real_ts64(&tv);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
        time64_to_tm(tv.tv_sec, 0, &tm);
#else
        time_to_tm(tv.tv_sec, 0, &tm);
#endif

        buffer[8] = tm.tm_sec;//[0,59]
        buffer[9] = tm.tm_min;//[0,59]
        buffer[10] = tm.tm_hour;//[0,23]
        buffer[11] = tm.tm_mday;//[1,31]


        skw_file_write(fp, "skwcplog\0\1\0\2\0\0\x3\xEA", 16);
        skw_file_write(fp, buffer, 12);
    }
    return fp;
}


void skwlog_init(void)
{
    mutex_init(&skwlog_lock);
}


void skwlog_write(unsigned char *buffer, unsigned int length)
{
	if(!skwbt_log_disable)
	{
		struct file *fp;
		mutex_lock(&skwlog_lock);
		fp = skwlog_open();
		
		if((fp == NULL) || IS_ERR(fp))
		{
			//pr_info("%s err:%ld", PTR_ERR(fp));
		}
		else
		{
			skw_file_write(fp, buffer, length);
			filp_close(fp, NULL);
		}
		mutex_unlock(&skwlog_lock);
	}
}

void skwlog_close(void)
{
    mutex_unlock(&skwlog_lock);
}

EXPORT_SYMBOL_GPL(skwlog_init);
EXPORT_SYMBOL_GPL(skwlog_write);
EXPORT_SYMBOL_GPL(skwlog_close);

#endif
