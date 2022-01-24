/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _REQUEST_EVENTS_H_
#define _REQUEST_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>

/** @note These events and definitions WILL be moved to their respective module
 *        once they're in place.
 */

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

#define GNSS_DEFINITION_SIZE (FENCE_MAX * sizeof(gnss_struct_t))

/** @todo PSH, the structure is aligned to 32 bit unsigned 
 * as returned from server. However, y values are placed
 * before X-values. Look into this. Currently, we must put Y before X.
 */
typedef struct {
	/** Relative coordinated of fence pole 
         *  in DECIMETERS from global origin.
         */

	/** NORTHING. */
	int16_t s_x_dm;

	/** EASTING. */
	int16_t s_y_dm;
} fence_coordinate_t;

typedef struct {
	/** Fence ID. */
	uint16_t us_id;

	/** Number of coordinates in polygon. */
	uint8_t n_points;

	/** Fece type. */
	uint8_t e_fence_type;

	/** Direct pointer to fence coordinate start. */
	fence_coordinate_t *p_c; //
} fence_header_t;

#define FENCE_MAX 10
#define FENCE_MAX_TOTAL_COORDINATES 300
#define FENCE_MAX_DEFINITION_SIZE                                              \
	(FENCE_MAX * sizeof(fence_header_t) +                                  \
	 FENCE_MAX_TOTAL_COORDINATES * sizeof(fence_coordinate_t))

/** @brief Request fence data from the storage module. This should be cached on
 *         RAM, so we send a pointer to a location in the AMC module
 *         so that the storage module can memcpy the data to this pointer
 *         location. We can then send an ACK event when this has been done.
*/
struct request_pasture_event {
	struct event_header header;

	/** Pointer to where the cached 
	 *  fence header data should be written to as well as the coordinates.
	 *  The module writing to this area must use fence->p_c to get
	 *  the pointer of the cached coordinate location.
	 */
	fence_header_t *fence;
};

/** @brief Indicates modules that need to know that we now have a new pasture
 *         available. In regards to the AMC, it will then publish a request
 *         event for the data which will be written to the passed pointer.
*/
struct pasture_ready_event {
	struct event_header header;
};

/** @brief Request gnss data from the GNSS module. See comment in
 *         request_pasture_event. We probably want to read out the GNSS data
 *         immediately, so we do not want to wait for an event? Needs discussion.
*/
struct gnssdata_event {
	struct event_header header;

	/** GNSS data structure. */
	gnss_struct_t gnss;
};

/** @brief Event that acknowledges that the pasture has 
 *         been successfully written to given pointer address.
*/
struct ack_pasture_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(request_pasture_event);
EVENT_TYPE_DECLARE(gnssdata_event);
EVENT_TYPE_DECLARE(pasture_ready_event);
EVENT_TYPE_DECLARE(ack_pasture_event);

#endif /*_REQUEST_EVENTS_H_ */