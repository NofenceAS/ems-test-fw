/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _SOUND_EVENT_H_
#define _SOUND_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for error severity. Can be replaced with error level in the
 *         future corresponding to integer values instead.
 *         See https://gitlab.com/nofence/nofence/-/blob/master/NofenceEmbedded/NoFence/Buzzer.h
 */
enum sound_event_type {
	SND_WELCOME = 0,
	SND_SOLAR_TEST = 1,
	SND_WARN = 2,
	SND_OFF = 3,
	SND_MAX = 4,
	SND_SETUPMODE = 5,
	SND_PERSPELMANN = 6,
	SND_CATTLE = 7,
	SND_SHORT_200 = 8,
	SND_SHORT_100 = 9,
	SND_FIND_ME = 10
};

/** @brief Struct containg status messages regarding the firmware upgrade. */
struct sound_event {
	struct event_header header;

	/** Which sound should be played and 
	 *  forwarded to the sound controller. 
	 */
	enum sound_event_type type;
};

EVENT_TYPE_DECLARE(sound_event);

#endif /* _SOUND_EVENT_H_ */