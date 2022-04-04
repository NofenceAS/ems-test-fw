/*
 * Copyright (c) 2022 Nofence AS
 */

#include <stdio.h>

#include "ble_cmd_event.h"

static int log_ble_cmd_event(const struct event_header *eh, char *buf,
			     size_t buf_len)
{
	const struct ble_cmd_event *event = cast_ble_cmd_event(eh);

	return snprintf(buf, buf_len, "event: ble_cmd %i", event->cmd);
}

EVENT_TYPE_DEFINE(ble_cmd_event, IS_ENABLED(CONFIG_LOG_BLE_CMD_EVENT),
		  log_ble_cmd_event, NULL);
