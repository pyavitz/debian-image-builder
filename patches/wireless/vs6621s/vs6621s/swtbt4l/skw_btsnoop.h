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



#ifndef __SKW_BTSNOOP_H__
#define __SKW_BTSNOOP_H__


#include <linux/types.h>



/* HCI Packet types */
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_ISODATA_PKT		0x05
#define HCI_EVENT_SKWLOG    0x07


void skw_btsnoop_init(void);

void skw_btsnoop_close(void);

void skw_btsnoop_capture(const unsigned char *packet, unsigned char is_received);

uint64_t skw_btsnoop_timestamp(void);



#endif
