/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _PWR_EVENT_H_
#define _PWR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for different electric pulse status */
enum pwr_state_flag { PWR_IDLE = 0, PWR_ACTIVE, PWR_SLEEP, PWR_OFF };

/** @brief Struct containg status messages regarding electric pulse events. */
struct pwr_status_event {
	struct event_header header;

	enum pwr_state_flag pwr_state;
};

EVENT_TYPE_DECLARE(pwr_status_event);

#endif /* _PWR_EVENT_H_ */