/*
 * Copyright (c) 2022 Nofence AS
 */

#include "diagnostics_events.h"
#include <logging/log.h>
#include <stdio.h>

#define LOG_MODULE_NAME diagnostics_events
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_DIAGNOSTICS_LOG_LEVEL);

/**
 * @brief Diagnostics status event.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_diag_status_event(const struct event_header *eh, char *buf,
				size_t buf_len)
{
	struct diag_status_event *event = cast_diag_status_event(eh);
	return snprintf(buf, buf_len, "DIAG_status=%d", event->diag_error);
}

EVENT_TYPE_DEFINE(diag_status_event, IS_ENABLED(CONFIG_LOG_DIAGNOSTICS_EVENT), log_diag_status_event, NULL);