/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _MOVEMENT_EVENT_H_
#define _MOVEMENT_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

struct movement_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(movement_event);

#endif /* _MOVEMENT_EVENT_H_ */