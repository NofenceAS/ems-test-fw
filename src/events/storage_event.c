/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "storage_event.h"

static char *get_evt_type_str(enum storage_event_type type)
{
	switch (type) {
	case STORAGE_EVT_READ_SERIAL_NR:
		return "STORAGE_EVT_READ_SERIAL_NR";
	case STORAGE_EVT_WRITE_SERIAL_NR:
		return "STORAGE_EVT_WRITE_SERIAL_NR";
	default:
		return "Unknown event";
	}
}

#if defined(CONFIG_PROFILER)

static int log_event(const struct event_header *eh, char *buf,
				 size_t buf_len)
{
	struct storage_event *event = cast_storage_event(eh);
    if (event->type == STORAGE_EVT_READ_SERIAL_NR) 
    {
        return snprintf(buf, buf_len, "%s - #:%d",
				get_evt_type_str(event->type), event->data.pvt.serial_number);
    }
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

static void profile_event(struct log_event_buf *buf,
				      const struct event_header *eh)
{
	struct storage_event *event = cast_storage_event(eh);

	ARG_UNUSED(event);
	profiler_log_encode_u32(buf, event->seral_number);
}

EVENT_INFO_DEFINE(storage_event,
		  ENCODE(PROFILER_ARG_U32),
		  ENCODE("type"),
		  profile_event);

#endif /* CONFIG_PROFILER */

EVENT_TYPE_DEFINE(storage_event,
		  CONFIG_STORAGE_EVENTS_LOG,
		  log_event,
#if defined(CONFIG_PROFILER)
		  &storage_event_info);
#else
		  NULL);
#endif


