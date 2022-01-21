/*
 * Copyright (c) 2022 Nofence AS
 */

#include "request_events.h"
#include <logging/log.h>
#include <stdio.h>

/**
 * @brief Requst fence data event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_request_fencedata_event(const struct event_header *eh, char *buf,
				       size_t buf_len)
{
	struct request_fencedata_event *event =
		cast_request_fencedata_event(eh);

	return snprintf(buf, buf_len, "Fencedata request to adr %p",
			event->fence);
}

EVENT_TYPE_DEFINE(request_fencedata_event, true, log_request_fencedata_event,
		  NULL);

/**
 * @brief Requst GNSS data event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_request_gnssdata_event(const struct event_header *eh, char *buf,
				      size_t buf_len)
{
	struct request_gnssdata_event *event = cast_request_gnssdata_event(eh);

	return snprintf(buf, buf_len, "GNSS data request to adr %p",
			event->gnss);
}

EVENT_TYPE_DEFINE(request_gnssdata_event, true, log_request_gnssdata_event,
		  NULL);

/**
 * @brief ACK data event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_amc_ack_event(const struct event_header *eh, char *buf,
			     size_t buf_len)
{
	struct amc_ack_event *event = cast_amc_ack_event(eh);

	return snprintf(buf, buf_len, "AMC ack received=%d", event->type);
}

EVENT_TYPE_DEFINE(amc_ack_event, true, log_amc_ack_event, NULL);