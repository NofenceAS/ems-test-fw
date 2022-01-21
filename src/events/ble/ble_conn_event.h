/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _BLE_CONN_EVENT_H_
#define _BLE_CONN_EVENT_H_

/**
 * @brief BLE Connection Event
 * @defgroup ble_conn_event BLE Connection Event
 * @{
 */

#include <string.h>

#include "event_manager.h"

enum ble_conn_state { BLE_STATE_CONNECTED, BLE_STATE_DISCONNECTED };

/** BLE connection event. */
struct ble_conn_event {
	struct event_header header;

	enum ble_conn_state conn_state;
};

EVENT_TYPE_DECLARE(ble_conn_event);

#endif /* _BLE_CONN_EVENT_H_ */
