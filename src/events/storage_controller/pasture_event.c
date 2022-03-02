/*
 * Copyright (c) 2022 Nofence AS
 */

#include "pasture_event.h"
#include <logging/log.h>
#include <stdio.h>

/**
 * @brief New pasture data available event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_pasture_ready_event(const struct event_header *eh, char *buf,
				   size_t buf_len)
{
	struct pasture_ready_event *event = cast_pasture_ready_event(eh);

	return snprintf(buf, buf_len, "New pasture available. Event pointer %p",
			event);
}

EVENT_TYPE_DEFINE(pasture_ready_event, true, log_pasture_ready_event, NULL);