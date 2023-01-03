/*
 * Copyright (c) 2022 Nofence AS
 */

#include "sound_event.h"
#include <logging/log.h>
#include <stdio.h>

/**
 * @brief Sound event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_sound_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	struct sound_event *event = cast_sound_event(eh);

	return snprintf(buf, buf_len, "Sound type=%d", event->type);
}

EVENT_TYPE_DEFINE(sound_event, true, log_sound_event, NULL);

/**
 * @brief Sound event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_sound_status_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	struct sound_status_event *event = cast_sound_status_event(eh);

	return snprintf(buf, buf_len, "Sound status=%d", event->status);
}

EVENT_TYPE_DEFINE(sound_status_event, true, log_sound_status_event, NULL);

/**
 * @brief Sound event function for debugging/information. 
 *        Uses the log to make it easier to
 *        debug what is happening on the event bus.
 * 
 * @param[in] ev event_header for given event.
 * @param[in] buf triggered event's log event buffer.
 * @param[in] buf_len length of the buffer received.
 */
static int log_sound_set_warn_freq_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	struct sound_set_warn_freq_event *event = cast_sound_set_warn_freq_event(eh);

	return snprintf(buf, buf_len, "Sound warnzone freq updated=%d", event->freq);
}

EVENT_TYPE_DEFINE(sound_set_warn_freq_event, IS_ENABLED(CONFIG_LOG_SOUND_EVENT),
		  log_sound_set_warn_freq_event, NULL);