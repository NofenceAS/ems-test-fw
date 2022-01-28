/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _REQUEST_EVENTS_H_
#define _REQUEST_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Indicates modules that need to know that we now have a new pasture
 *         available. In regards to the AMC, it will then publish a request
 *         event for the data which will be written to the passed pointer.
*/
struct pasture_ready_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(pasture_ready_event);

#endif /*_REQUEST_EVENTS_H_ */