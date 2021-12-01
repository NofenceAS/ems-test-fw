#include <event_manager.h>

void request_sensor_data()
{
	struct ack_event *event = new_ack_event();
	EVENT_SUBMIT(event);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sensor_event(eh)) {
		struct sensor_event *event = cast_sensor_event();
		LOG_INF("Value1: %d, Value2: %d, Value3: %d", event->value1,
			event->value2, event->value3);
	}
	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, sensor_event);