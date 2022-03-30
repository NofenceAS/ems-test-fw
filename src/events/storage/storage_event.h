/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

#define STORAGE_ERASE_MAGIC 0xEBA53001

struct request_flash_erase_event {
	struct event_header header;

	int magic;
};

EVENT_TYPE_DECLARE(request_flash_erase_event);

#endif /* _STORAGE_EVENT_H_ */