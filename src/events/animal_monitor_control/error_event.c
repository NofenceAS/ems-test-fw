/*
 * Copyright (c) 2021 Nofence AS
 */

#include "fw_upgrade_events.h"
#include <logging/log.h>
#include <stdio.h>

#define LOG_MODULE_NAME fw_upgrade_events
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_FW_UPGRADE_LOG_LEVEL);

/**
 * @brief Error event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_error_event(const struct event_header *eh, char *buf,
			   size_t buf_len)
{
	struct error_event *event = cast_error_event(eh);

	return snprintf(buf, buf_len, "Error code=%d, Severity=%d, Sender=%d",
			event->code, event->sender, event->severity);
}

EVENT_TYPE_DEFINE(error_event, true, log_error_event, NULL);