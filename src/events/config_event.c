/*
 * Copyright (c) 2021 Nofence AS
 */
 
#include <stdio.h>

#include "config_event.h"


static void profile_config_event(struct log_event_buf *buf,
				 const struct event_header *eh)
{
	struct config_event *event = cast_config_event(eh);

	ARG_UNUSED(event);
	profiler_log_encode_u32(buf, event->serial_number);
}

static int log_config_event(const struct event_header *eh, char *buf,
			    size_t buf_len)
{
	struct config_event *event = cast_config_event(eh);

	return snprintf(buf, buf_len, "serial_number=%d", event->serial_number);
}

EVENT_INFO_DEFINE(config_event,
		  ENCODE(PROFILER_ARG_U32),
		  ENCODE("serial_number"),
		  profile_config_event);

EVENT_TYPE_DEFINE(config_event,
		  true,
		  log_config_event,
		  &config_event_info);
