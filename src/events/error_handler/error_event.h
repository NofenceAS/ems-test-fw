/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ERROR_EVENT_H_
#define _ERROR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for defining the modules that can publish to this event. 
 *         Will be used by error handler module to perform the correct actions.
 *         Add new values before ERR_END_OF_LIST.
 */
enum error_sender_module {
	ERR_FW_UPGRADE = 0,
	ERR_AMC = 1,
	ERR_STORAGE_CONTROLLER = 2,
	ERR_ENV_SENSOR = 3,
	ERR_EP_MODULE = 4,
	ERR_PWR_MODULE = 5,
	ERR_GNSS_CONTROLLER = 6,
	ERR_SOUND_CONTROLLER = 7,
	ERR_MESSAGING = 8,
	ERR_EEPROM = 9,
	ERR_DIAGNOSTIC = 10,
	ERR_BLE_MODULE = 11,
	ERR_CELLULAR_CONTROLLER = 12,
	ERR_MOVEMENT_CONTROLLER = 13,
	ERR_WATCHDOG = 14,
	ERR_BEACON = 15,
	ERR_MODEM = 16,
	ERR_END_OF_LIST
};

/**
 * @brief Registers a fatal error caused by some module/logic that must
 *        be handled. So severe that the entire system shuts down if not treated
 *        immediately.
 * 
 * @param[in] sender which module triggered the warning.
 * @param[in] code error code from the sender.
 * @param[in] msg custom user message attached to the event. Can be NULL.
 * @param[in] msg_len length of user message, 
 *                    maximum CONFIG_ERROR_MAX_USER_MESSAGE_SIZE characters.
 */
void nf_app_fatal(enum error_sender_module sender, int code, char *msg, size_t msg_len);

/**
 * @brief Registers a non-fatal error caused by some module/logic that must
 *        be handled. May not cause total system failure, but should still be
 *        processed and dealt with.
 * 
 * @param[in] sender which module triggered the warning.
 * @param[in] code error code from the sender.
 * @param[in] msg custom user message attached to the event. Can be NULL.
 * @param[in] msg_len length of user message, 
 *                    maximum CONFIG_ERROR_MAX_USER_MESSAGE_SIZE characters.
 */
void nf_app_error(enum error_sender_module sender, int code, char *msg, size_t msg_len);

/**
 * @brief Registers a warning caused by some module/logic that must
 *        not necessarily be handled. May be ignored, but should be treated
 *        if possible and dealt with.
 * 
 * @param[in] sender which module triggered the warning.
 * @param[in] code error code from the sender.
 * @param[in] msg custom user message attached to the event. Can be NULL.
 * @param[in] msg_len length of user message, 
 *                    maximum CONFIG_ERROR_MAX_USER_MESSAGE_SIZE characters.
 */
void nf_app_warning(enum error_sender_module sender, int code, char *msg, size_t msg_len);

/** @brief Enum for error severity. Can be replaced with error level in the
 *         future corresponding to integer values instead.
 */
enum error_severity { ERR_SEVERITY_FATAL = 0, ERR_SEVERITY_ERROR = 1, ERR_SEVERITY_WARNING = 2 };

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

EVENT_TYPE_DYNDATA_DECLARE(error_event);

#endif /* _ERROR_EVENT_H_ */
