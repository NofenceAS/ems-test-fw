/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _STROAGE_EVENT_H_
#define _STROAGE_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

struct request_flash_erase_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(request_flash_erase_event);

#endif /* _PWR_EVENT_H_ */