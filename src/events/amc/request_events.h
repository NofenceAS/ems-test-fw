/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _REQUEST_EVENTS_H_
#define _REQUEST_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>
#include "nf_common.h"

/** @note These events and definitions WILL be moved to their respective module
 *        once they're in place.
 */

#define GNSS_DEFINITION_SIZE (FENCE_MAX * sizeof(gnss_struct_t))

/** @brief Request gnss data from the GNSS module. See comment in
 *         request_pasture_event. We probably want to read out the GNSS data
 *         immediately, so we do not want to wait for an event? Needs discussion.
*/
struct gnssdata_event {
	struct event_header header;

	/** GNSS data structure. */
	gnss_struct_t gnss;
};

EVENT_TYPE_DECLARE(gnssdata_event);

#endif /*_REQUEST_EVENTS_H_ */