/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _BLE_CMD_EVENT_H_
#define _BLE_CMD_EVENT_H_

#include "event_manager.h"

/** Peer connection event. */
struct ble_cmd_event {
	struct event_header header;

	uint8_t cmd;
};

EVENT_TYPE_DECLARE(ble_cmd_event);

#endif /* _BLE_CMD_EVENT_H_ */
