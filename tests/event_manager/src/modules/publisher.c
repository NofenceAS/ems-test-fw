#include <zephyr.h>
#include "sensor_event.h"
#include "req_event.h"
#include "publisher.h"

#define MODULE publisher_example

void update_sensor(int value1, int value2, int value3)
{
	struct sensor_event *event = new_sensor_event();

	/* Storing values into the event so modules subscribing to it can retrieve it. 
    This will be replaced by communication with the driver API calls to retrieve the data */
	event->value1 = value1;
	event->value2 = value2;
	event->value3 = value3;

	EVENT_SUBMIT(event);
}

/**
 * @brief Main event handler function that handles all the events this module subscribes to,
 * basically a large *switch* case using if's and prefedined event triggers to check against given
 * event_header param.
 * @param eh event_header for the if-chain to use to recognize which event triggered
 */
static bool event_handler(const struct event_header *eh)
{
	if (is_req_event(eh)) {
		update_sensor(1, 2, 3);
		/* Return true to consume event, since we don't want further listeners to receive this request */
		return true;
	}
	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, req_event);