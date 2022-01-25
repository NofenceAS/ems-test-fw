/*
 * Copyright (c) 2022 Nofence AS
 */

#include <stdio.h>
#include "storage_events.h"

/**
 * @brief Useful fuction to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_write_memrec_event(const struct event_header *eh, char *buf,
				      size_t buf_len)
{
	struct stg_write_memrec_event *event = cast_stg_write_memrec_event(eh);
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

EVENT_TYPE_DEFINE(stg_write_memrec_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_write_memrec_event, NULL);

/**
 * @brief Useful fuction to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_ack_write_memrec_event(const struct event_header *eh,
					  char *buf, size_t buf_len)
{
	struct stg_ack_write_memrec_event *event =
		cast_stg_ack_write_memrec_event(eh);
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

EVENT_TYPE_DEFINE(stg_ack_write_memrec_event,
		  IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_ack_write_memrec_event, NULL);

/**
 * @brief Useful fuction to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_read_memrec_event(const struct event_header *eh, char *buf,
				     size_t buf_len)
{
	struct stg_read_memrec_event *event = cast_stg_read_memrec_event(eh);
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

EVENT_TYPE_DEFINE(stg_read_memrec_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_read_memrec_event, NULL);
