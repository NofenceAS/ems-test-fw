/*
 * Copyright (c) 2022 Nofence AS
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

	if (event->dyndata.size > 0) {
		return snprintf(buf, buf_len,
				"<%s>, Error code=%d, Severity=%d, Sender=%d",
				log_strdup(event->dyndata.data), event->code,
				event->severity, event->sender);
	} else {
		return snprintf(
			buf, buf_len,
			"<No message>, Error code=%d, Severity=%d, Sender=%d",
			event->code, event->severity, event->sender);
	}
}

static inline void submit_app_status(enum error_sender_module sender,
				     enum error_severity severity, int code,
				     char *msg, size_t msg_len)
{
	size_t dyn_msg_size = msg_len;

	/* Check if string is greater than limit, remove 
	 * the entire message if exceeding. 
	 */
	if (msg_len > CONFIG_ERROR_USER_MESSAGE_SIZE) {
		dyn_msg_size = 0;
	}

	struct error_event *event = new_error_event(dyn_msg_size);

	/* -EINVALS to error handler, nothing to do about that. */
	/* Not negative error code. */
	if (code >= 0) {
		return;
	}

	/* If not part of the ENUMs. */
	if (sender < 0 || sender > ERR_SENDER_FW_UPGRADE) {
		return;
	}

	/* If not part of the ENUMs. */
	if (severity < 0 || severity > ERR_SEVERITY_WARNING) {
		return;
	}

	event->code = code;
	event->sender = sender;
	event->severity = severity;

	if (dyn_msg_size > 0 && msg != NULL) {
		memcpy(event->dyndata.data, msg, dyn_msg_size);
	}

	/* Submit event. */
	EVENT_SUBMIT(event);
}

void NF_APP_FATAL(enum error_sender_module sender, int code, char *msg,
		  size_t msg_len)
{
	submit_app_status(sender, ERR_SEVERITY_FATAL, code, msg, msg_len);
}

void NF_APP_ERROR(enum error_sender_module sender, int code, char *msg,
		  size_t msg_len)
{
	submit_app_status(sender, ERR_SEVERITY_ERROR, code, msg, msg_len);
}

void NF_APP_WARNING(enum error_sender_module sender, int code, char *msg,
		    size_t msg_len)
{
	submit_app_status(sender, ERR_SEVERITY_WARNING, code, msg, msg_len);
}

EVENT_TYPE_DEFINE(error_event, true, log_error_event, NULL);