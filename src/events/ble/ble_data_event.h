/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _BLE_DATA_EVENT_H_
#define _BLE_DATA_EVENT_H_

/**
 * @brief BLE Data Event
 * @defgroup ble_data_event BLE Data Event
 * @{
 */

#include <string.h>

#include "event_manager.h"

/** Peer connection event. */
struct ble_data_event {
	struct event_header header;

	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(ble_data_event);

#endif /* _BLE_DATA_EVENT_H_ */
