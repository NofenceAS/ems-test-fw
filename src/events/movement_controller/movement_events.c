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
static int log_movement_out_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	struct movement_out_event *event = cast_movement_out_event(eh);

	return snprintf(buf, buf_len,
			"Request environment sensor data, event adr %p", event);
}

EVENT_TYPE_DEFINE(movement_out_event, true, log_movement_out_event, NULL);

static int log_movement_set_mode_event(const struct event_header *eh, char *buf,
				       size_t buf_len)
{
	struct movement_set_mode_event *event =
		cast_movement_set_mode_event(eh);

	return snprintf(buf, buf_len, "Request to set movement mode %i",
			event->acc_mode);
}

EVENT_TYPE_DEFINE(movement_set_mode_event, true, log_movement_set_mode_event,
		  NULL);