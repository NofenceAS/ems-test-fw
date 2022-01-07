/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>
#include <assert.h>

#include "ble_data_event.h"

/**
 * @brief Bluetooth data event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_ble_data_event(const struct event_header *eh, char *buf,
			      size_t buf_len)
{
	const struct ble_data_event *event = cast_ble_data_event(eh);

	return snprintf(buf, buf_len, "buf:%p len:%d", event->buf, event->len);
}

EVENT_TYPE_DEFINE(ble_data_event, IS_ENABLED(CONFIG_LOG_BLE_DATA_EVENT),
		  log_ble_data_event, NULL);
