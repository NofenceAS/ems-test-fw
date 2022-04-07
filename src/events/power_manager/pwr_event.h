/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _PWR_EVENT_H_
#define _PWR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for different electric pulse status */
enum pwr_state_flag {
	PWR_NORMAL = 0,
	PWR_LOW,
	PWR_CRITICAL,
	PWR_BATTERY,
	PWR_CHARGING
};

/** @brief Struct containg status messages regarding electric pulse events. */
struct pwr_status_event {
	struct event_header header;

	enum pwr_state_flag pwr_state;
	uint16_t battery_mv;
	uint16_t charging_ma;
};

struct request_pwr_battery_event {
	struct event_header header;
};

struct request_pwr_charging_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(pwr_status_event);
EVENT_TYPE_DECLARE(request_pwr_battery_event);
EVENT_TYPE_DECLARE(request_pwr_charging_event);

#endif /* _PWR_EVENT_H_ */