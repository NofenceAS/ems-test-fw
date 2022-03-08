/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

struct request_flash_erase_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(request_flash_erase_event);

#endif /* _STORAGE_EVENT_H_ */