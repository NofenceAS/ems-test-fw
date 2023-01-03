/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "ble_beacon_event.h"

/** @brief Function to convert enum to string */
static char *stringFromEnum(enum beacon_status_type status)
{
	static char *strings[] = { "BEACON_STATUS_REGION_NEAR", "BEACON_STATUS_REGION_FAR",
				   "BEACON_STATUS_NOT_FOUND", "BEACON_STATUS_OFF" };

	return strings[status];
}

static int log_ble_beacon_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	const struct ble_beacon_event *event = cast_ble_beacon_event(eh);

	return snprintf(buf, buf_len, "Beacon status: %s", stringFromEnum(event->status));
}

EVENT_TYPE_DEFINE(ble_beacon_event, IS_ENABLED(CONFIG_LOG_BLE_BEACON_EVENT), log_ble_beacon_event,
		  NULL);
