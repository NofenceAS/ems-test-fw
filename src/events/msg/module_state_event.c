/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>
#include <assert.h>

#include "module_state_event.h"

static const char *const state_name[] = {
#define X(name) STRINGIFY(name),
	MODULE_STATE_LIST
#undef X
};

/**
 * @brief Module state event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_module_state_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct module_state_event *event = cast_module_state_event(eh);

	BUILD_ASSERT(ARRAY_SIZE(state_name) == MODULE_STATE_COUNT,
		     "Invalid number of elements");

	__ASSERT_NO_MSG(event->state < MODULE_STATE_COUNT);

	return snprintf(buf, buf_len, "module:%s state:%s",
			(const char *)event->module_id,
			state_name[event->state]);
}

EVENT_TYPE_DEFINE(module_state_event, IS_ENABLED(CONFIG_LOG_MODULE_STATE_EVENT),
		  log_module_state_event, NULL);
