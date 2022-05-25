/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _DIAGNOSTICS_EVENTS_H_
#define _DIAGNOSTICS_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Struct containg status messages regarding the diagnostics. */
struct diag_status_event {
	struct event_header header;

	/** If an error occurs this is not 0. */
	int diag_error;
};

EVENT_TYPE_DECLARE(diag_status_event);

#endif /* _DIAGNOSTICS_EVENTS_H_ */