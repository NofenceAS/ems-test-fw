/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _REQ_EVENT_H_
#define _REQ_EVENT_H_

/**
 * @brief REQ Event
 * @defgroup req_event
 * @{
 */

#include "event_manager.h"

struct req_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(req_event);


#endif /* _REQ_EVENT_H_ */