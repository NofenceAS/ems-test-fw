#ifndef X3_FW_NF_COMMON_H
#define X3_FW_NF_COMMON_H


#include <zephyr.h>

/** @brief Struct containing GNSS data. */
typedef struct {
	int32_t lat;
	int32_t lon;

	/** Corrected position.*/
	int16_t x;
	/** Corrected position.*/
	int16_t y;

	/** Set if overflow because of too far away from origin position.*/
	uint8_t overflow;

	/** Height above ellipsoid [dm].*/
	int16_t height;

	/** 2-D speed [cm/s]*/
	uint16_t speed;

	/** Movement direction (-18000 to 18000 Hundred-deg).*/
	int16_t head_veh;

	/** Horizontal dilution of precision.*/
	uint16_t h_dop;

	/** Horizontal Accuracy Estimate [DM].*/
	uint16_t h_acc_dm;

	/** Vertical Accuracy Estimate [DM].*/
	uint16_t v_acc_dm;

	/** Heading accuracy estimate [Hundred-deg].*/
	uint16_t head_acc;

	/** Number of SVs used in Nav Solution.*/
	uint8_t num_sv;

	/** UBX-NAV-PVT flags as copied.*/
	uint8_t pvt_flags;

	/** UBX-NAV-PVT valid flags as copies.*/
	uint8_t pvt_valid;

	/** Milliseconds since position report from UBX.*/
	uint16_t age;

	/** UBX-NAV-SOL milliseconds since receiver start or reset.*/
	uint32_t msss;

	/** UBX-NAV-SOL milliseconds since First Fix.*/
	uint32_t ttff;
} gnss_struct_t;

/** @brief See gps_struct_t for descriptions. */
typedef struct {
	int32_t lat;
	int32_t lon;
	uint32_t unix_timestamp;
	uint16_t h_acc_d;
	int16_t head_veh;
	uint16_t head_acc;
	uint8_t pvt_flags;
	uint8_t num_sv;
	uint16_t hdop;
	int16_t height;
	int16_t baro_height;
	uint32_t msss;
	uint8_t gps_mode;
} gnss_last_fix_struct_t;

typedef struct
{
	int32_t Lat;
	int32_t Lon;
	int16_t X;				// Corrected position
	int16_t Y;				// Corrected position
	uint8_t Overflow;		// Set if overflow because of too far away from origin position
	int16_t Height;			// Height above ellipsoid [dm]
	uint16_t Speed;			// 2-D speed [cm/s]
	int16_t headVeh;		// Movement direction (-18000 to 18000 Hundred-deg)
	uint16_t hDOP;			// Horizontal dilution of precision
	uint16_t hAccDm;		// Horizontal Accuracy Estimate [DM]
	uint16_t vAccDm;		// Vertical Accuracy Estimate [DM]
	uint16_t headAcc;		// Heading accuracy estimate [Hundred-deg]
	uint8_t numSV;			// Number of SVs used in Nav Solution
	uint8_t pvt_flags;      // UBX-NAV-PVT flags as copied
	uint8_t pvt_valid;      // UBX-NAV-PVT valid flags as copies
	uint16_t Age;			// milliseconds since position report from UBX
	//uint16_t LastAge;		// Time between the two last position reports from UBX
	uint32_t msss;          // UBX-NAV-SOL milliseconds since receiver start or reset
	uint32_t ttff;          // UBX-NAV-SOL milliseconds since First Fix
} gps_struct_t;

typedef struct {
	int32_t Lat;
	int32_t Lon;
	uint32_t unixTimestamp;
	uint16_t hAccDm;
	int16_t headVeh;
	uint16_t headAcc;
	uint8_t pvt_flags;
	uint8_t numSV;
	uint16_t hdop;
	int16_t Height;
	int16_t baro_Height;
	uint32_t msss;
	uint8_t gps_mode;
} gps_last_fix_struct_t;

#endif //X3_FW_NF_COMMON_H