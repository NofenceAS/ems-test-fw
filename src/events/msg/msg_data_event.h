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
#include <toolchain/common.h>

#include "event_manager.h"

/** Peer connection event. */
struct msg_data_event {
	struct event_header header;

	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(msg_data_event);

#endif /* _MSG_DATA_EVENT_H_ */
