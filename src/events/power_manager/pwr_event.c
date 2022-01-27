/*
 * Copyright (c) 2021 Nofence AS
 */

#include "pwr_event.h"
#include <logging/log.h>
#include <stdio.h>

/**
 * @brief Power Manager status event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_pwr_status_event(const struct event_header *eh, char *buf,
				size_t buf_len)
{
	struct pwr_status_event *event = cast_pwr_status_event(eh);
	return snprintf(buf, buf_len, "PWR_status=%d", event->pwr_state);
}

EVENT_TYPE_DEFINE(pwr_status_event, IS_ENABLED(CONFIG_LOG_PWR_EVENT),
		  log_pwr_status_event, NULL);