#ifndef _SENSOR_EVT_H_
#define _SENSOR_EVT_H_

/**
 * @brief Simulated sensor measurement event
 * @defgroup Sensor event
 * @{
 */

#include <event_manager.h>

struct sensor_event {
	struct event_header header;

	int8_t value1;
	int16_t value2;
	int32_t value3;
};

EVENT_TYPE_DECLARE(sensor_event);

#endif /* _SENSOR_EVT_H_ */