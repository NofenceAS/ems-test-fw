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

	/** errno.h error code attached. */
	int code;

	enum error_severity severity;

	/** A temporary debug user message if needed. */
	char user_message[CONFIG_ERROR_USER_MESSAGE_SIZE];
};

EVENT_TYPE_DECLARE(error_event);

#endif /* _ERROR_EVENT_H_ */