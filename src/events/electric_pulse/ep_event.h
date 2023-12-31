/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _EP_EVENT_H_
#define _EP_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for different electric pulse status */
enum ep_status_flag { EP_RELEASE = 0 };

/** @brief Struct containg status messages regarding electric pulse events. */
struct ep_status_event {
	struct event_header header;

	enum ep_status_flag ep_status;
	bool is_first_pulse;
};

EVENT_TYPE_DECLARE(ep_status_event);

#endif /* _EP_EVENT_H_ */