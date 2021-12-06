/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _CONFIG_EVENT_H_
#define _CONFIG_EVENT_H_

/**
 * @brief Config Event
 * @defgroup config_event Config Event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct config_event {
	struct event_header header;

	uint32_t serial_number;
};

EVENT_TYPE_DECLARE(config_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _CONFIG_EVENT_H_ */
