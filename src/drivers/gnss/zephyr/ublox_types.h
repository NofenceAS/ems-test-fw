#ifndef UBLOX_TYPES_H_
#define UBLOX_TYPES_H_

#include <stdint.h>

/* Structures are tightly packed, using little-endian format, 
 * to be directly translateable to U-blox format. 
 */
#define UBLOX_STORAGE_ATTR                                                                         \
	__attribute__((packed, scalar_storage_order("little-endian"), aligned(1)))

/** @brief Definition of layers used for storage */
enum ublox_cfg_val_layer { RAM_LAYER = 0, BBR_LAYER = 1, FLASH_LAYER = 2, DEFAULT_LAYER = 7 };

/** @brief U-blox 16-bit checksum. 
 *         Calculations are done on the 2 8-bit values 
 */
union UBLOX_STORAGE_ATTR ublox_checksum {
	uint16_t ck;
	struct {
		uint8_t a;
		uint8_t b;
	} ck_val;
};

/** @brief U-blox header. */
struct UBLOX_STORAGE_ATTR ublox_header {
	uint8_t sync1;
	uint8_t sync2;

	uint8_t msg_class;
	uint8_t msg_id;

	uint16_t length;
};

/** @brief U-blox ACK-ACK/NAK message. */
struct UBLOX_STORAGE_ATTR ublox_ack_ack {
	uint8_t clsID;
	uint8_t msgID;
};

/** @brief U-blox CFG-VAL message. */
struct UBLOX_STORAGE_ATTR ublox_cfg_val {
	uint8_t version;
	uint8_t layer;
	uint16_t position;
	uint32_t keys;
};

/** @brief U-blox CFG-RST message. */
struct UBLOX_STORAGE_ATTR ublox_cfg_rst {
	uint16_t navBbrMask;
	uint8_t resetMode;
	uint8_t reserved0;
};

/** @brief U-blox NAV-DOP message. */
struct UBLOX_STORAGE_ATTR ublox_nav_dop {
	uint32_t iTOW;
	uint16_t gDOP;
	uint16_t pDOP;
	uint16_t tDOP;
	uint16_t vDOP;
	uint16_t hDOP;
	uint16_t nDOP;
	uint16_t eDOP;
};

/** @brief U-blox NAV-PVT message. */
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

/** @brief U-blox NAV-STATUS message. */
struct UBLOX_STORAGE_ATTR ublox_nav_status {
	uint32_t iTOW;
	uint8_t gpsFix;
	uint8_t flags;
	uint8_t fixStat;
	uint8_t flags2;
	uint32_t ttff;
	uint32_t msss;
};

/** @brief U-blox NAV-PL message. */
struct UBLOX_STORAGE_ATTR ublox_nav_pl {
	uint8_t msgVersion;
	uint8_t tmirCoeff;
	int8_t tmirExp;
	uint8_t plPosValid;
	uint8_t plPosFrame;
	uint8_t plVelValid;
	uint8_t plVelFrame;
	uint8_t plTimeValid;
	uint8_t reserved0[4];
	uint32_t iTow;
	uint32_t plPos1;
	uint32_t plPos2;
	uint32_t plPos3;
	uint32_t plVel1;
	uint32_t plVel2;
	uint32_t plVel3;
	uint16_t plPosHorizOrient;
	uint16_t plVelHorizOrient;
	uint32_t plTime;
	uint8_t reserved1[4];
};

/**
 * @brief RXM-PMREQ message
 * @see https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf
 */
struct UBLOX_STORAGE_ATTR ublox_rxm_pmreq {
	uint8_t version;
	uint8_t reserved0[3];
	uint32_t duration;
	uint32_t flags;
	uint32_t wakeupSources;
};

/** @brief U-blox MGA-ACK message. */
struct UBLOX_STORAGE_ATTR ublox_mga_ack {
	uint8_t type;
	uint8_t version;
	uint8_t infoCode;
	uint8_t msgId;
	uint8_t msgPayloadStart[4];
};

/* Helper macros for getting various data types from buffer */
#define GET_LE8(x) ((x[0] << 0))
#define GET_LE16(x) ((x[0] << 0) | (x[1] << 8))
#define GET_LE32(x) ((x[0] << 0) | (x[1] << 8) | (x[2] << 16) | (x[3] << 24))
#define GET_LE64(x)                                                                                \
	(((uint64_t)x[0] << 0) | ((uint64_t)x[1] << 8) | ((uint64_t)x[2] << 16) |                  \
	 ((uint64_t)x[3] << 24) | ((uint64_t)x[4] << 32) | ((uint64_t)x[5] << 40) |                \
	 ((uint64_t)x[6] << 48) | ((uint64_t)x[7] << 56))

#endif /* UBLOX_TYPES_H_ */