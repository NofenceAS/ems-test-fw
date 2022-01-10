/*
 * Copyright (c) 2021 Nofence AS
 */

#include "error_event.h"
#include <logging/log.h>
#include <stdio.h>

#define LOG_MODULE_NAME fw_upgrade_events
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_ERROR_HANDLER_LOG_LEVEL);

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

void submit_error(enum error_sender_module sender, enum error_severity severity,
		  int code, char *msg, size_t msg_len)
{
	/* Allocate event. */
	struct error_event *event = new_error_event();

	/* Write data to error event. */
	event->sender = sender;
	event->code = code;
	event->severity = severity;

	if (msg_len != 0 && msg_len <= CONFIG_ERROR_USER_MESSAGE_SIZE &&
	    msg != NULL) {
		strncpy(event->user_message, msg, msg_len);
	}

	/* Submit event. */
	EVENT_SUBMIT(event);
}

EVENT_TYPE_DEFINE(error_event, true, log_error_event, NULL);