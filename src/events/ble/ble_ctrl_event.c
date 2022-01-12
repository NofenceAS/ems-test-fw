/*
 * Copyright (c) 2021 Nofence AS
 */

#include <assert.h>
#include <stdio.h>

#include "ble_ctrl_event.h"

/**
 * @brief Bluetooth control event function for debugging/information.
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 *
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_ble_ctrl_event(const struct event_header *eh, char *buf,
			      size_t buf_len)
{
	const struct ble_ctrl_event *event = cast_ble_ctrl_event(eh);

	return snprintf(buf, buf_len, "cmd:%d", event->cmd);
}

EVENT_TYPE_DEFINE(ble_ctrl_event, true, log_ble_ctrl_event, NULL);
