/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "test_event.h"


static void profile_test_event(struct log_event_buf *buf,
			      const struct event_header *eh)
{
}

EVENT_INFO_DEFINE(test_event,
		  ENCODE(),
		  ENCODE(),
		  profile_test_event);

EVENT_TYPE_DEFINE(test_event,
		  true,
		  NULL,
		  &test_event_info);
