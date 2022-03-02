/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _SOUND_EVENT_H_
#define _SOUND_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

enum sound_event_type {
	SND_OFF = 0,
	SND_FIND_ME,
	SND_WARN,
	SND_WELCOME,
	SND_SETUPMODE,
	SND_PERSPELMANN,
	SND_CATTLE,
	SND_SOLAR_TEST,
	SND_SHORT_100,
	SND_SHORT_200,
	/* Set once the sound finishes playing, should always be last enum. */
	SND_READY_FOR_NEXT_TYPE
};

/** @brief Sound event struct containg information of which sound to play. */
struct sound_event {
	struct event_header header;

	/** Which sound should be played and 
	 *  forwarded to the sound controller. 
	 */
	enum sound_event_type type;
};

enum sound_event_status_type {
	SND_STATUS_IDLE,
	SND_STATUS_PLAYING,
	SND_STATUS_PLAYING_WARN,
	/* EP can check if this enum is set in the sound status event. */
	SND_STATUS_PLAYING_MAX
};

/** @brief Status event of which mode the buzzer is at. */
struct sound_status_event {
	struct event_header header;

	enum sound_event_status_type status;
};

/** @brief Status event of which mode the buzzer is at. */
struct sound_set_warn_freq_event {
	struct event_header header;

	/* Frequency in microseconds. (1000000 / uint32_t freq). */
	uint32_t freq;
};

EVENT_TYPE_DECLARE(sound_event);
EVENT_TYPE_DECLARE(sound_status_event);
EVENT_TYPE_DECLARE(sound_set_warn_freq_event);

#endif /* _SOUND_EVENT_H_ */