/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _BLE_CTRL_EVENT_H_
#define _BLE_CTRL_EVENT_H_

/**
 * @brief BLE Control Event
 * @defgroup ble_ctrl_event BLE Control Event
 * @{
 */

#include <string.h>

#include "event_manager.h"

/** @brief Enum for defining the differen bluetooth control commands
 * 		   to enable or disable advertisement, and update ad array. 
 */
enum ble_ctrl_cmd {
	BLE_CTRL_ENABLE,
	BLE_CTRL_DISABLE,
	BLE_CTRL_BATTERY_UPDATE,
	BLE_CTRL_ERROR_FLAG_UPDATE,
	BLE_CTRL_COLLAR_MODE_UPDATE,
	BLE_CTRL_COLLAR_STATUS_UPDATE,
	BLE_CTRL_FENCE_STATUS_UPDATE,
	BLE_CTRL_PASTURE_UPDATE,
	BLE_CTRL_FENCE_DEF_VER_UPDATE
};

/** BLE control event. */
struct ble_ctrl_event {
	struct event_header header;

	enum ble_ctrl_cmd cmd;
	union {
		const char *name_update;
		uint8_t battery;
		uint8_t error_flags;
		uint8_t collar_mode;
		uint8_t collar_status;
		uint8_t fence_status;
		uint8_t valid_pasture;
		uint16_t fence_def_ver;
	} param;
};

EVENT_TYPE_DECLARE(ble_ctrl_event);

#endif /* _BLE_CTRL_EVENT_H_ */
