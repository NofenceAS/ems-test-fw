/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include "watchdog_event.h"

EVENT_TYPE_DEFINE(watchdog_alive_event, true, NULL, NULL);