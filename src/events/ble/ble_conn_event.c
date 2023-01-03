/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "ble_conn_event.h"

static int log_ble_conn_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	const struct ble_conn_event *event = cast_ble_conn_event(eh);

	return snprintf(buf, buf_len, "%s",
			event->conn_state == BLE_STATE_CONNECTED ? "CONNECTED" : "DISCONNECTED");
}

EVENT_TYPE_DEFINE(ble_conn_event, IS_ENABLED(CONFIG_LOG_BLE_CONN_EVENT), log_ble_conn_event, NULL);
