
#ifndef UBLOX_PROTOCOL_H_
#define UBLOX_PROTOCOL_H_

#include <stdint.h>

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

#define UBLOX_MIN_PACKET_SIZE 	(UBLOX_OFFS_PAYLOAD+UBLOX_CHK_SIZE)

uint32_t ublox_parse(uint8_t* data, uint32_t size);

#endif /* UBLOX_PROTOCOL_H_ */