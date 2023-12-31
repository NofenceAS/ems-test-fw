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

typedef enum {
	/** If below sleep threshold. */
	STATE_SLEEP = 0,
	/** If above or equal to threshold. */
	STATE_NORMAL = 1,
	/* If inactive for n seconds, where n is configurable from user. */
	STATE_INACTIVE = 2
} movement_state_t;

typedef enum { MODE_OFF = 0, MODE_1_6_HZ = 1, MODE_12_5_HZ = 2 } acc_mode_t;

struct movement_set_mode_event {
	struct event_header header;

	acc_mode_t acc_mode;
};

struct movement_out_event {
	struct event_header header;

	movement_state_t state;
};

struct movement_timeout_event {
	struct event_header header;
};

struct activity_level {
	struct event_header header;
	acc_activity_t level;
};

struct step_counter_event {
	struct event_header header;
	uint32_t steps;
};

typedef enum { NO_ACTIVITY_SIGMA = 0, SLEEP_SIGMA = 1, OFF_ANIMAL_SIGMA = 2 } sigma_type_t;

struct acc_sigma_event {
	struct event_header header;
	sigma_type_t type;
	union {
		uint16_t sleep_sigma_value;
		uint16_t no_activity_sigma;
		uint16_t off_animal_value;
	} param;
};

struct acc_raw_event {
	struct event_header header;
	int16_t x;
	int16_t y;
	int16_t z;
};

EVENT_TYPE_DECLARE(movement_set_mode_event);
EVENT_TYPE_DECLARE(movement_out_event);
EVENT_TYPE_DECLARE(movement_timeout_event);
EVENT_TYPE_DECLARE(activity_level);
EVENT_TYPE_DECLARE(step_counter_event);
EVENT_TYPE_DECLARE(acc_sigma_event);
EVENT_TYPE_DECLARE(acc_raw_event);
#endif /* _MOVEMENT_EVENT_H_ */
