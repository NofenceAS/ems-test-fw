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

/** @brief Struct containg messages regarding diagnostic turning on and off threads. */
struct diag_thread_cntl_event {
	struct event_header header;
	/** Determin wheather you want to start or stop the cellular thread. */
	bool run_cellular_thread;
	bool allow_fota;
	int force_gnss_mode;
};

EVENT_TYPE_DECLARE(diag_thread_cntl_event);

#endif /* _DIAGNOSTICS_EVENTS_H_ */