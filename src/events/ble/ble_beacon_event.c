/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "ble_beacon_event.h"

static int log_ble_beacon_event(const struct event_header *eh, char *buf,
				size_t buf_len)
{
	const struct ble_beacon_event *event = cast_ble_beacon_event(eh);

	return snprintf(buf, buf_len, "status: %d", event->status);
}

EVENT_TYPE_DEFINE(ble_beacon_event, IS_ENABLED(CONFIG_LOG_BLE_BEACON_EVENT),
		  log_ble_beacon_event, NULL);
