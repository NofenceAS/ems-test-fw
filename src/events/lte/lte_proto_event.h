/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _LTE_PROTO_EVENT_H_
#define _LTE_PROTO_EVENT_H_

/**
 * @brief LTE proto Event
 * @defgroup lte_proto_event LTE proto Event
 * @{
 */

#include <zephyr.h>
#include "event_manager.h"

/** Modem protobuf message. */
struct lte_proto_event {
	struct event_header header;

	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(lte_proto_event);

#endif /* _LTE_proto_EVENT_H_ */
