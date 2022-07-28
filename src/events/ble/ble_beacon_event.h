/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _BLE_BEACON_EVENT_H_
#define _BLE_BEACON_EVENT_H_

/**
 * @brief BLE Beacon Event
 * @defgroup ble_conn_event BLE Connection Event
 * @{
 */

#include <string.h>

#include "event_manager.h"

enum beacon_status_type {
	BEACON_STATUS_REGION_NEAR = 0,
	BEACON_STATUS_REGION_FAR,
	BEACON_STATUS_NOT_FOUND,
	BEACON_STATUS_OFF
};

/** BLE connection event. */
struct ble_beacon_event {
	struct event_header header;

	enum beacon_status_type status;
};

EVENT_TYPE_DECLARE(ble_beacon_event);

#endif /* _BLE_CONN_EVENT_H_ */
