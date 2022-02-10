/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ENV_SENSOR_EVENT_H_
#define _ENV_SENSOR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

struct env_sensor_event {
	struct event_header header;

	/* These values follow struct sensor_values using
	 * integer and fractional part, calculated using:
	 * val1 + val2 * 10^(-6).
	 */
	struct {
		int32_t integer;
		int32_t frac;
	} temp;

	struct {
		int32_t integer;
		int32_t frac;
	} press;

	struct {
		int32_t integer;
		int32_t frac;
	} humidity;
};

struct request_env_sensor_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(env_sensor_event);
EVENT_TYPE_DECLARE(request_env_sensor_event);

#endif /* _ENV_SENSOR_EVENT_H_ */