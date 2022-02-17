#ifndef UBLOX_TYPES_H_
#define UBLOX_TYPES_H_

#include <stdint.h>

enum ublox_cfg_val_layer {
	RAM_LAYER = 0,
	BBR_LAYER = 1,
	FLASH_LAYER = 2,
	DEFAULT_LAYER = 7
};

#define UBLOX_STORAGE_ATTR __attribute__((packed,scalar_storage_order("little-endian"),aligned(1)))

union UBLOX_STORAGE_ATTR ublox_checksum {
	uint16_t ck;
	struct {
		uint8_t a;
		uint8_t b;
	} ck_val;
};

struct UBLOX_STORAGE_ATTR ublox_header {
	uint8_t sync1;
	uint8_t sync2;
	
	uint8_t msg_class;
	uint8_t msg_id;

	uint16_t length;
};

struct UBLOX_STORAGE_ATTR ublox_ack_ack {
	uint8_t clsID;
	uint8_t msgID;
};

struct UBLOX_STORAGE_ATTR ublox_cfg_val {
	uint8_t version;
	uint8_t layer;
	uint16_t position;
	uint32_t keys;
};

struct UBLOX_STORAGE_ATTR ublox_nav_pvt {
	uint32_t iTOW;
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t valid;
	uint32_t tAcc;
	int32_t nano;
	uint8_t fixType;
	uint8_t flags;
	uint8_t flags2;
	uint8_t numSV;
	int32_t lon;
	int32_t lat;
	int32_t height;
	int32_t hMSL;
	uint32_t hAcc;
	uint32_t vAcc;
	int32_t velN;
	int32_t velE;
	int32_t velD;
	int32_t gSpeed;
	int32_t headMot;
	uint32_t sAcc;
	uint32_t headAcc;
	uint16_t pDOP;
	uint16_t flags3;
	uint8_t reserved0[4];
	int32_t headVeh;
	int16_t magDec;
	uint16_t magAcc;
};

#define GET_LE8(x)	((x[0]<<0))
#define GET_LE16(x)	((x[0]<<0)|\
			 (x[1]<<8))
#define GET_LE32(x)	((x[0]<<0)|\
			 (x[1]<<8)|\
			 (x[2]<<16)|\
			 (x[3]<<24))
#define GET_LE64(x)	(((uint64_t)x[0]<<0)|\
			 ((uint64_t)x[1]<<8)|\
			 ((uint64_t)x[2]<<16)|\
			 ((uint64_t)x[3]<<24)|\
			 ((uint64_t)x[4]<<32)|\
			 ((uint64_t)x[5]<<40)|\
			 ((uint64_t)x[6]<<48)|\
			 ((uint64_t)x[7]<<56))

#endif /* UBLOX_TYPES_H_ */