#include <stdio.h>
#include "req_event.h"

/**
 * @brief Function for debugging/information. Uses the profiler tool to make it easier to
 * debug what is happening on the event bus
 * @param buf triggered event's log event buffer
 * @param ev event_header for given event
 */
static void profile_req_event(struct log_event_buf *buf,
			      const struct event_header *eh)
{
}

EVENT_INFO_DEFINE(req_event, ENCODE(), ENCODE(), profile_req_event);

EVENT_TYPE_DEFINE(req_event, true, NULL, &req_event_info);