/******************************************************************************
 *
 *  Copyright (C) 2020-2021 SeekWave Technology
 *
 *
 ******************************************************************************/

#ifndef __SKW_LOG_H__
#define __SKW_LOG_H__

void skwlog_init(void);

void skwlog_write(unsigned char *buffer, unsigned int length);

void skwlog_close(void);


#endif
