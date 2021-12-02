#include <zephyr.h>
#include "req_event.h"
#include "sensor_event.h"
#include "listener.h"

void request_sensor_data()
{
	struct req_event *event = new_req_event();
	EVENT_SUBMIT(event);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sensor_event(eh)) {
		struct sensor_event *event = cast_sensor_event(eh);
		/* Do something with the data */
		printk("value1 %d, value2 %d, value3 %d", event->value1,
		       event->value2, event->value3);
	}
	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, sensor_event);
