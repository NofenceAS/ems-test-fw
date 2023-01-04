/*
 * Copyright (c) 2022 Nofence AS
 */

#include "request_events.h"
#include <logging/log.h>
#include <stdio.h>

/**
 * @brief Requst GNSS data event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_gnssdata_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	struct gnssdata_event *event = cast_gnssdata_event(eh);

	return snprintf(buf, buf_len, "GNSS data lat %d lon %d", event->gnss.lat, event->gnss.lon);
}

EVENT_TYPE_DEFINE(gnssdata_event, true, log_gnssdata_event, NULL);