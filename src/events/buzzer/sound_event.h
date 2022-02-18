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
	SND_MAX,
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

EVENT_TYPE_DECLARE(sound_event);

#endif /* _SOUND_EVENT_H_ */