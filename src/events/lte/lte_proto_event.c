/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>
#include <assert.h>

#include "lte_proto_event.h"

/**
 * @brief GSM Modem LTE protobuf message event function 
 *        for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_lte_proto_event(const struct event_header *eh, char *buf,
			       size_t buf_len)
{
	const struct lte_proto_event *event = cast_lte_proto_event(eh);

	return snprintf(buf, buf_len, "len:%d", event->len);
}

EVENT_TYPE_DEFINE(lte_proto_event, IS_ENABLED(CONFIG_LOG_LTE_PROTO_EVENT),
		  log_lte_proto_event, NULL);
