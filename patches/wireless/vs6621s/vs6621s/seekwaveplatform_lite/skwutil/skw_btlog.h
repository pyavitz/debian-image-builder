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

#ifndef __SKW_BTLOG_H__
#define __SKW_BTLOG_H__

//#define SKWBT_LOG_PORT_EN 0

#ifndef SKWBT_LOG_PORT_EN
#ifdef CONFIG_BT_SEEKWAVE
#define SKWBT_LOG_PORT_EN 0
#else

#define SKWBT_LOG_PORT_EN 1

#endif
#endif



void skwbt_log_port_init(void);

void skwbt_log_port_set_pdata(void *pdata);

void skwbt_log_port_exit(void);

void skwbt_log_port_data_write(uint8_t is_from_cp, uint8_t *data, uint16_t data_len);

void skwbt_log_port_set_bt_open(char is_open);

#endif





