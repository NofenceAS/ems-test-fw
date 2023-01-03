/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "ble_ctrl_event.h"

/** @brief Function to convert enum to string */
static char *stringFromEnum(enum ble_ctrl_cmd cmd)
{
	static char *strings[] = { "BLE_CTRL_ADV_ENABLE",
				   "BLE_CTRL_ADV_DISABLE",
				   "BLE_CTRL_BATTERY_UPDATE",
				   "BLE_CTRL_ERROR_FLAG_UPDATE",
				   "BLE_CTRL_COLLAR_MODE_UPDATE",
				   "BLE_CTRL_COLLAR_STATUS_UPDATE",
				   "BLE_CTRL_FENCE_STATUS_UPDATE",
				   "BLE_CTRL_PASTURE_UPDATE",
				   "BLE_CTRL_FENCE_DEF_VER_UPDATE",
				   "BLE_CTRL_SCAN_START",
				   "BLE_CTRL_SCAN_STOP",
				   "BLE_CTRL_DISCONNECT_PEER" };

	return strings[cmd];
}

/**
 * @brief Bluetooth control event function for debugging/information.
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 *
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_ble_ctrl_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	const struct ble_ctrl_event *event = cast_ble_ctrl_event(eh);

	return snprintf(buf, buf_len, "BLE ctrl state: %s", stringFromEnum(event->cmd));
}

EVENT_TYPE_DEFINE(ble_ctrl_event, IS_ENABLED(CONFIG_LOG_BLE_CTRL_EVENT), log_ble_ctrl_event, NULL);
