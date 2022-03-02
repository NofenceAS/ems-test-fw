/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _MSG_DATA_EVENT_H_
#define _MSG_DATA_EVENT_H_

/**
 * @brief MSG Data Event
 * @defgroup msg_data_event MSG Data Event
 * @{
 */

#include <string.h>

#include "event_manager.h"

/** Peer connection event. */
struct msg_data_event {
	struct event_header header;

        struct event_dyndata dyndata;
};

EVENT_TYPE_DYNDATA_DECLARE(msg_data_event);

#endif /* _MSG_DATA_EVENT_H_ */
