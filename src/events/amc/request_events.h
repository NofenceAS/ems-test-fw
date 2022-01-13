/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _REQUEST_EVENTS_H_
#define _REQUEST_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Request fence data from the storage module. This should be cached on
 *         RAM, so we send a pointer to a location in the AMC module
 *         so that the storage module can memcpy the data to this pointer
 *         location. We can then send an ACK event when this has been done.
*/
struct request_fencedata_event {
	struct event_header header;

	uint8_t *data;
	size_t len;
};

/** @brief Request gnss data from the GNSS module. See comment in
 *         request_fencedata_event. We probably want to read out the GNSS data
 *         immediately, so we do not want to wait for an event? Needs discussion.
*/
struct request_gnssdata_event {
	struct event_header header;

	uint8_t *data;
	size_t len;
};

enum amc_request_events { AMC_REQ_FENCEDATA = 0, AMC_REQ_GNSSDATA = 1 };

/** @brief Event that acknowledges that the data has been successfully written
 *         to given pointer address. The AMC module will subscribe to this
 *         event in order to perform its necessary calculation functions when
 *         given ack type was given.
*/
struct amc_ack_event {
	struct event_header header;

	/* Which request type that was acked. */
	enum amc_request_events type;
};

EVENT_TYPE_DECLARE(request_fencedata_event);
EVENT_TYPE_DECLARE(request_gnssdata_event);
EVENT_TYPE_DECLARE(amc_ack_event);

#endif /*_REQUEST_EVENTS_H_ */