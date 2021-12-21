#include <stdio.h>
#include "sensor_event.h"

/**
 * @brief Function for debugging/information. This logs the given values
 * from the simulated sensor and gives number of bytes for that operation
 * @param eh event_header for the sensor_event
 * @param buf data from the event
 * @param buf_len length of the data from the event
 * @returns number of bytes stored with snprintf
 */
static int log_sensor_event(const struct event_header *eh, char *buf,
				 size_t buf_len)
{
	struct sensor_event *event = cast_sensor_event(eh);

	return snprintf(buf, buf_len, "val1=%d val2=%d val3=%d", event->value1,
			event->value2, event->value3);
}

/**
 * @brief Function for debugging/information. Uses the profiler tool to make it easier to
 * debug what is happening on the event bus
 * @param buf triggered event's log event buffer
 * @param ev event_header for given event
 */
static void profile_sensor_event(struct log_event_buf *buf,
				      const struct event_header *eh)
{
	struct sensor_event *event = cast_sensor_event(eh);

	ARG_UNUSED(event);
	profiler_log_encode_u32(buf, event->value1);
	profiler_log_encode_u32(buf, event->value2);
	profiler_log_encode_u32(buf, event->value3);
}

EVENT_INFO_DEFINE(sensor_event,
		  ENCODE(PROFILER_ARG_S8, PROFILER_ARG_S16, PROFILER_ARG_S32),
		  ENCODE("value1", "value2", "value3"),
		  profile_sensor_event);

EVENT_TYPE_DEFINE(sensor_event,
		  true,
		  log_sensor_event,
		  &sensor_event_info);