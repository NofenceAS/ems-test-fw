/*
 * Copyright (c) 2021 Nofence AS
 */
 
#include "ble/nf_ble.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <logging/log.h>
#include <event_manager.h>
#include <config_event.h>
/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */

LOG_MODULE_REGISTER(main);

#define INIT_VALUE1 3

void main(void)
{
	LOG_INF("Starting nofence application");
	if (event_manager_init()) {
		LOG_ERR("Event manager not initialized");
	} else {
		struct config_event *event = new_config_event();
		event->init_value1 = INIT_VALUE1;
		EVENT_SUBMIT(event);
	}
}