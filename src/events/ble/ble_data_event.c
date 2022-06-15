/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "ble_data_event.h"

EVENT_TYPE_DEFINE(ble_data_event, IS_ENABLED(CONFIG_LOG_BLE_DATA_EVENT),
		  NULL, NULL);
