/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _LTE_DATA_EVENT_H_
#define _LTE_DATA_EVENT_H_

/**
 * @brief LTE Data Event
 * @defgroup lte_data_event LTE Data Event
 * @{
 */

#include <zephyr.h>
#include "event_manager.h"

/** Peer connection event. */
struct lte_data_event {
	struct event_header header;

	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(lte_data_event);

#endif /* _LTE_DATA_EVENT_H_ */
