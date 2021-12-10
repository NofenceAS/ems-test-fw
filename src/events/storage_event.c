/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "storage_event.h"

/**
 * @brief Internal functions to convert event type to string.
 * @param type enum with storage event type
 * @return char - string of event type
 */
static char *get_evt_type_str(enum storage_event_type type)
{
	switch (type) {
	case STORAGE_EVT_READ_SERIAL_NR:
		return "STORAGE_EVT_READ_SERIAL_NR";
	case STORAGE_EVT_WRITE_SERIAL_NR:
		return "STORAGE_EVT_WRITE_SERIAL_NR";
	case STORAGE_EVT_ERASE_FLASH:
		return "STORAGE_EVT_ERASE_FLASH";
	case STORAGE_EVT_ERROR_CODE:
		return "STORAGE_EVT_ERROR_CODE";
	case STORAGE_EVT_READ_IP_ADDRESS:
		return "STORAGE_EVT_READ_IP_ADDRESS";
	case STORAGE_EVT_WRITE_IP_ADDRESS:
		return "STORAGE_EVT_WRITE_IP_ADDRESS";
    case STORAGE_EVT_READ_FW_VERSION:
		return "STORAGE_EVT_READ_FW_VERSION";
	case STORAGE_EVT_WRITE_FW_VERSION:
		return "STORAGE_EVT_WRITE_FW_VERSION";
	default:
		return "Unknown event";
	}
}

/**
 * @brief Useful fuction to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_event(const struct event_header *eh, char *buf,
				 size_t buf_len)
{
	struct storage_event *event = cast_storage_event(eh);
    if (event->type == STORAGE_EVT_WRITE_SERIAL_NR) 
    {
        return snprintf(buf, buf_len, "%s - #:%d",
				get_evt_type_str(event->type), event->data.pvt.serial_number);
    }
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

#if defined(CONFIG_PROFILER)
/**
 * @brief Useful fuction to profile event
 * @param buf buffer pointer to send to profiler
 * @param eh event_header struct
 */
static void profile_event(struct log_event_buf *buf,
				      const struct event_header *eh)
{
	struct storage_event *event = cast_storage_event(eh);

	ARG_UNUSED(event);
	profiler_log_encode_u32(buf, event->type);
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


