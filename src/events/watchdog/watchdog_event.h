/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _WATCHDOG_EVENT_H_
#define _WATCHDOG_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Only periodic modules are included in this list */
enum watchdog_alive_module {
	WDG_PWR_MODULE = 0,
	WDG_BLE_SCAN = 1,
	//WDG_GNSS_CONTROLLER = 3,
	WDG_END_OF_LIST
};

#define WATCHDOG_ALIVE_MAGIC 0xEBA53002
struct watchdog_alive_event {
	struct event_header header;

	/* Which module report alive */
	enum watchdog_alive_module module;

	/* Magic check for data consitency */
	int magic;
};

/**
 * @brief System monitor function to validate that all the modules are
 *        alive and working at intended. Will feed watchdog if all modules 
 *        reported within required time.
 * 
 * @param[in] module which module responds to be valid and alive.
 */
void watchdog_report_module_alive(enum watchdog_alive_module module);

EVENT_TYPE_DECLARE(watchdog_alive_event);

#endif /* _WATCHDOG_EVENT_H_ */