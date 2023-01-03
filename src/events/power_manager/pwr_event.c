/*
 * Copyright (c) 2021 Nofence AS
 */

#include "pwr_event.h"
#include <logging/log.h>
#include <stdio.h>

/** @brief Function to convert enum to string */
static char *string_from_enum(enum pwr_state_flag state)
{
	static char *strings[] = { "PWR_NORMAL", "PWR_LOW", "PWR_CRITICAL", "PWR_BATTERY",
				   "PWR_CHARGING" };

	return strings[state];
}

/**
 * @brief Power Manager status event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_pwr_status_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	struct pwr_status_event *event = cast_pwr_status_event(eh);
	return snprintf(buf, buf_len, "PWR state: %s", string_from_enum(event->pwr_state));
}

EVENT_TYPE_DEFINE(pwr_status_event, IS_ENABLED(CONFIG_LOG_PWR_EVENT), log_pwr_status_event, NULL);

EVENT_TYPE_DEFINE(request_pwr_battery_event, true, NULL, NULL);

EVENT_TYPE_DEFINE(request_pwr_charging_event, true, NULL, NULL);

EVENT_TYPE_DEFINE(pwr_reboot_event, true, NULL, NULL);
