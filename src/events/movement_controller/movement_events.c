/*
 * Copyright (c) 2022 Nofence AS
 */

#include "movement_events.h"
#include "event_manager.h"
#include <zephyr.h>
#include <logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(movement_log_event, CONFIG_MOVE_CONTROLLER_LOG_LEVEL);

/**
 * @brief Request env sensor event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_movement_event(const struct event_header *eh, char *buf,
			      size_t buf_len)
{
	struct movement_event *event = cast_movement_event(eh);

	return snprintf(buf, buf_len,
			"Request environment sensor data, event adr %p", event);
}

EVENT_TYPE_DEFINE(movement_event, true, log_movement_event, NULL);