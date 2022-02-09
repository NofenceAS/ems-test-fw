/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ENV_SENSOR_EVENT_H_
#define _ENV_SENSOR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

struct env_sensor_event {
	struct event_header header;

	union {
		int32_t temp_integer;
		int32_t temp_frac;
	};

	union {
		int32_t press_integer;
		int32_t press_frac;
	};

	union {
		int32_t humidity_integer;
		int32_t humidity_frac;
	};
};

struct request_env_sensor_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(env_sensor_event);
EVENT_TYPE_DECLARE(request_env_sensor_event);

#endif /* _ENV_SENSOR_EVENT_H_ */