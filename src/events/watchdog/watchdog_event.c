/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include "watchdog_event.h"

void watchdog_report_module_alive(enum watchdog_alive_module module)
{
	struct watchdog_alive_event *event = new_watchdog_alive_event();
	event->module = module;
	event->magic = WATCHDOG_ALIVE_MAGIC;
	/* Submit event. */
	EVENT_SUBMIT(event);
}

EVENT_TYPE_DEFINE(watchdog_alive_event, true, NULL, NULL);