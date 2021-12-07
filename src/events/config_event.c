/*
 * Copyright (c) 2021 Nofence AS
 */
 
#include <stdio.h>

#include "config_event.h"

static char *get_evt_type_str(enum config_event_type type)
{
	switch (type) {
	case CONFIG_EVT_START:
		return "CONFIG_EVT_START";
	case CONFIG_EVT_ERROR:
		return "CONFIG_EVT_ERROR";
	default:
		return "Unknown event";
	}
}


static int log_config_event(const struct event_header *eh, char *buf,
			    size_t buf_len)
{
	struct config_event *event = cast_config_event(eh);

	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

#if defined(CONFIG_PROFILER)

static void profile_config_event(struct log_event_buf *buf,
				 const struct event_header *eh)
{
	struct config_event *event = cast_config_event(eh);

	ARG_UNUSED(event);
	profiler_log_encode_u32(buf, event->type);
}


EVENT_INFO_DEFINE(config_event,
		  ENCODE(PROFILER_ARG_U32),
		  ENCODE("type"),
		  profile_config_event);

#endif /* CONFIG_PROFILER */

EVENT_TYPE_DEFINE(config_event,
		  CONFIG_CONFIG_EVENTS_LOG,
		  log_config_event,
#if defined(CONFIG_PROFILER)
		  &config_event_info);
#else
		  NULL);
#endif