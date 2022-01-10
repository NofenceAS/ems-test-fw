/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ERROR_EVENT_H_
#define _ERROR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for defining the modules that can publish to this event. 
 *         Will be used by error handler module to perform the correct actions.
 */
enum error_sender_module { ERR_SENDER_FW_UPGRADE = 0 };

/** @brief Enum for error severity. Can be replaced with error level in the
 *         future corresponding to integer values instead.
 */
enum error_severity {
	ERR_SEVERITY_FATAL = 0,
	ERR_SEVERITY_ERROR = 1,
	ERR_SEVERITY_WARNING = 2
};

/** @brief Struct containg status messages regarding the firmware upgrade. */
struct error_event {
	struct event_header header;

	/** Which module triggered the error event. */
	enum error_sender_module sender;

	/** errno.h error code. */
	int code;

	enum error_severity severity;

	/** A temporary debug user message if needed. */
	struct event_dyndata dyndata;
};

/**
 * @brief Submits the given parameters to the error event bus.
 * 
 * @param[in] sender which module triggered the warning.
 * @param[in] severity severity of the error.
 * @param[in] code error code from the sender.
 * @param[in] msg custom user message attached to the event. Can be NULL.
 * @param[in] msg_len length of user message, 
 *                    maximum CONFIG_ERROR_USER_MESSAGE_SIZE characters.
 */
void submit_error(enum error_sender_module sender, enum error_severity severity,
		  int code, char *msg, size_t msg_len);

EVENT_TYPE_DYNDATA_DECLARE(error_event);

#endif /* _ERROR_EVENT_H_ */