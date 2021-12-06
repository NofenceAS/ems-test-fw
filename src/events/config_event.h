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


/** @brief Config event types submitted by Configuration module. */
enum config_event_type {
	CONFIG_EVT_START,
	CONFIG_EVT_ERROR
};

struct config_event {
	struct event_header header;
	enum config_event_type type;
};

EVENT_TYPE_DECLARE(config_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _CONFIG_EVENT_H_ */
