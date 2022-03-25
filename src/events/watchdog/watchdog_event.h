/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _WATCHDOG_EVENT_H_
#define _WATCHDOG_EVENT_H_

#include "error_event.h"
#include "event_manager.h"
#include <zephyr.h>

struct watchdog_alive_event {
	struct event_header header;

	/** Which module triggered the event. */
	enum error_sender_module sender;
};

EVENT_TYPE_DECLARE(watchdog_alive_event);

#endif /* _WATCHDOG_EVENT_H_ */