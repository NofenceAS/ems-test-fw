/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "req_event.h"


static void profile_req_event(struct log_event_buf *buf,
			      const struct event_header *eh)
{
}

EVENT_INFO_DEFINE(req_event,
		  ENCODE(),
		  ENCODE(),
		  profile_req_event);

EVENT_TYPE_DEFINE(req_event,
		  true,
		  NULL,
		  &req_event_info);