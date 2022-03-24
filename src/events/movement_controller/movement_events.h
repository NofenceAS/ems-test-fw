/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _MOVEMENT_EVENT_H_
#define _MOVEMENT_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

typedef enum {
	ACTIVITY_NO = 0,
	ACTIVITY_LOW = 1,
	ACTIVITY_MED = 2,
	ACTIVITY_HIGH = 3
} acc_activity_t;

typedef enum { MODE_OFF = 0, MODE_1_6_HZ = 1, MODE_12_5_HZ = 2 } acc_mode_t;

struct movement_set_mode_event {
	struct event_header header;

	acc_mode_t acc_mode;
};

struct movement_out_event {
	struct event_header header;

	acc_activity_t activity;
	uint16_t stepcount;
};

EVENT_TYPE_DECLARE(movement_set_mode_event);
EVENT_TYPE_DECLARE(movement_out_event);

#endif /* _MOVEMENT_EVENT_H_ */