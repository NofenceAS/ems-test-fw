/*
 * Copyright (c) 2022 Nofence AS
 */

#include "env_sensor_event.h"
#include <logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_ENV_SENSOR_LOG_LEVEL);

/**
 * @brief Env sensor event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_env_sensor_event(const struct event_header *eh, char *buf,
				size_t buf_len)
{
	struct env_sensor_event *event = cast_env_sensor_event(eh);

	return snprintf(buf, buf_len, "Environment sensor, event adr %p",
			event);
}

EVENT_TYPE_DEFINE(env_sensor_event, true, log_env_sensor_event, NULL);

/**
 * @brief Request env sensor event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_request_env_sensor_event(const struct event_header *eh,
					char *buf, size_t buf_len)
{
	struct request_env_sensor_event *event =
		cast_request_env_sensor_event(eh);

	return snprintf(buf, buf_len,
			"Request environment sensor data, event adr %p", event);
}

EVENT_TYPE_DEFINE(request_env_sensor_event, true, log_request_env_sensor_event,
		  NULL);