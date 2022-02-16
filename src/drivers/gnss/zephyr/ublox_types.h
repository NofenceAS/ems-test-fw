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


struct UBLOX_STORAGE_ATTR ublox_cfg_valget {
	uint8_t version;
	uint8_t layer;
	uint8_t reserved0[2];
	uint32_t keys;
};

#endif /* UBLOX_TYPES_H_ */