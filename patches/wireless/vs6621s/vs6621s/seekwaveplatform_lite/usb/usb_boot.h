#define MAX_PACKET_SIZE		0x400		//4K
#define PACKET_MAGIC		0x7e7e7e7e
/******************************************************************
 * **Description:download image，addrress and img size
 * Seekwave tech LTD
 * *	StartDownload 	0x0001
 * *
 * * MAGIC 4B	Length 4B MessageType 2B  	2B	CRC 0x7E7E7E7E
 * **
 * ******************************************************************/

struct connect_ack {
   unsigned int packet_size;
   union packet_attr_tag
   {
		   struct connect_attr_map
		   {
				   unsigned int check_sum	   :1;
				   unsigned int smp			 :1;
				   unsigned int boot			:1;
				   unsigned int res0			:1;
				   unsigned int strapin		 :2;
				   unsigned int usb_sdio_dis	:2;
				   unsigned int res1			:24;
		   }bitmap;
		   unsigned int dwValue;
   }flags;
   unsigned int  chip_id[4];
};

typedef struct PACKET_BODY_tag{
	unsigned int magic;	 //magic
	unsigned int size;			  //length,length - 12
	unsigned short type;			  //type,the type defferent cmd
	unsigned short crc;		//checksum
	unsigned char content[MAX_PACKET_SIZE];
}PACKET_T;
#define PACKET_HEADER_SIZE		(sizeof(struct PACKET_BODY_tag) - MAX_PACKET_SIZE)
#ifndef MIN
#define MIN(a,b)			(((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)			(((a) > (b)) ? (a) : (b))
#endif
#define DEBUG
#define NORMAL_BOOT 0
#define HANG_REBOOT 1
#define START_ADDR	0x100000
#define MAX_IMAGE_SIZE	0x7a000

static const char client_version[] = { 0x7E, 0x7E, 0x7E, 0x7E }; /* magic only to probe client */
static const char client_version_ack[] = {0x7E, 0x7E, 0x7E, 0x7E,/* magic */
	0x18, 0x00, 0x00, 0x00,/*size*/
	0x81, 0x00, /*message type*/
	0x00, 0x00, /* crc16 = 0 */
	0x42, 0x6f, 0x6f, 0x74, 0x20, 0x4c, 0x6f, 0x61, 0x64, 0x65, 0x72, 0x20,
	0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20, 0x31, 0x2e, 0x30, 0x00};

static const char connect[] = {0x7E, 0x7E, 0x7E, 0x7E,/* magic */
	0x18, 0x00, 0x00, 0x00,/*size*/
	0x00, 0x00, /* message type, 0: connect command */
	0x00, 0x00, /*crc16*/
	0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static  char connect_ack[] = {0x7E, 0x7E, 0x7E, 0x7E,/* magic */
	0x18, 0x00, 0x00, 0x00,/*size*/
	0x80, 0x00, /*message type*/
	0x00, 0x00, /* crc16 */
	0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x53, 0x56, 0x36, 0x31,
	0x36, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const char common_ack[] = { 0x7e, 0x7e, 0x7e, 0x7e, /* magic */
	0x00, 0x00, 0x00, 0x00,/*size*/
	0x80, 0x00, /*message type*/
	0x00, 0x00};/* crc16 */

static const char exec_ack[] = { 0x7e, 0x7e, 0x7e, 0x7e,
	0x00, 0x00, 0x00, 0x00,/*size*/
	0x80, 0x00,/*message type*/
	0x00, 0x00};/* crc16 */

static int dloader_send_command(const char *command,  int command_len, const char *ack, int ack_len);

#define dloader_command_client_probe()				\
    do {                         \
	ret = dloader_send_command(client_version, sizeof(client_version),	\
			client_version_ack, sizeof(client_version_ack)); \
    }while(0)

#define dloader_command_connect()				\
    do {                         \
	ret = dloader_send_command(connect, sizeof(connect), connect_ack, sizeof(connect_ack)); \
    }while(0)
