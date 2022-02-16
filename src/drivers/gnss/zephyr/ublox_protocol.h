
#ifndef UBLOX_PROTOCOL_H_
#define UBLOX_PROTOCOL_H_

#include "ublox_types.h"
#include "ubx_cfg_ids.h"

#include <stdint.h>
#include <stdbool.h>

#define UBLOX_SYNC_CHAR_1	0xB5
#define UBLOX_SYNC_CHAR_2	0x62

#define UBLOX_OFFS_SC_1		0
#define UBLOX_OFFS_SC_2		1
#define UBLOX_OFFS_CLASS	2
#define UBLOX_OFFS_ID		3
#define UBLOX_OFFS_LENGTH	4
#define UBLOX_OFFS_PAYLOAD	6
#define UBLOX_OFFS_CK_A(x)	(6+x)
#define UBLOX_OFFS_CK_B(x)	(7+x)

#define UBLOX_CHK_SIZE		2

#define UBLOX_MIN_PACKET_SIZE		(UBLOX_OFFS_PAYLOAD+UBLOX_CHK_SIZE)

#define UBLOX_MSG_IDENTIFIER(c,i)	((c<<8)|(i))
#define UBLOX_MSG_IDENTIFIER_EMPTY	0

struct ublox_handler {
	uint8_t msg_class;
	uint8_t msg_id;
	int (*handle)(void*,void*,uint32_t);
	void* context;
};

void ublox_protocol_init(void);

int ublox_register_handler(uint8_t msg_class, uint8_t msg_id, 
			   int (*handle)(void*,void*,uint32_t), void* context);

uint32_t ublox_parse(uint8_t* data, uint32_t size);

int ublox_reset_poll_and_ack(void);

int ublox_send_cfg_valget(enum ublox_cfg_val_layer layer, 
			  uint16_t position, 
			  uint32_t* keys, 
			  uint8_t key_cnt,
			  int (*put_fnc)(uint8_t*,uint32_t),
			  int (*poll_cb)(void*,uint8_t,uint8_t,void*,uint32_t),
			  int (*ack_cb)(void*,uint8_t,uint8_t,bool),
			  void* context);

#endif /* UBLOX_PROTOCOL_H_ */