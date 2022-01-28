/*
 * Copyright (c) 2022 Nofence AS
 */

#include <stdio.h>
#include "storage_events.h"

/**
 * @brief Useful function to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_write_event(const struct event_header *eh, char *buf,
			       size_t buf_len)
{
	struct stg_write_event *event = cast_stg_write_event(eh);
	return snprintf(buf, buf_len, "Write data event: %p", event);
}

EVENT_TYPE_DEFINE(stg_write_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_write_event, NULL);

/**
 * @brief Useful function to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_ack_write_event(const struct event_header *eh, char *buf,
				   size_t buf_len)
{
	struct stg_ack_write_event *event = cast_stg_ack_write_event(eh);
	return snprintf(buf, buf_len, "Ack write data event: %p", event);
}

EVENT_TYPE_DEFINE(stg_ack_write_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_ack_write_event, NULL);

/**
 * @brief Useful function to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters written to the buffer
 */
static int log_stg_read_event(const struct event_header *eh, char *buf,
			      size_t buf_len)
{
	struct stg_read_event *event = cast_stg_read_event(eh);
	return snprintf(buf, buf_len, "Read data event: %p", event);
}

EVENT_TYPE_DEFINE(stg_read_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_read_event, NULL);

/**
 * @brief Useful function to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters read to the buffer
 */
static int log_stg_ack_read_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	struct stg_ack_read_event *event = cast_stg_ack_read_event(eh);
	return snprintf(buf, buf_len, "Ack read data event: %p", event);
}

EVENT_TYPE_DEFINE(stg_ack_read_event, IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_ack_read_event, NULL);

/**
 * @brief Useful function to log event
 * @param eh event_header struct
 * @param buf buffer to store the log
 * @param buf_len size of buffer
 * @return return the number of characters read to the buffer
 */
static int log_stg_consumed_read_event(const struct event_header *eh, char *buf,
				       size_t buf_len)
{
	struct stg_consumed_read_event *event =
		cast_stg_consumed_read_event(eh);
	return snprintf(buf, buf_len, "Consumed read event: %p", event);
}

EVENT_TYPE_DEFINE(stg_consumed_read_event,
		  IS_ENABLED(CONFIG_STORAGE_EVENTS_LOG),
		  log_stg_consumed_read_event, NULL);
