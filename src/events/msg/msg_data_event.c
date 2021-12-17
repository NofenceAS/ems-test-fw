/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <assert.h>

#include "msg_data_event.h"

static int log_msg_data_event(const struct event_header *eh, char *buf,
			      size_t buf_len)
{
	const struct msg_data_event *event = cast_msg_data_event(eh);

	return snprintf(buf, buf_len, "buf:%p len:%d", event->buf, event->len);
}

EVENT_TYPE_DEFINE(msg_data_event, IS_ENABLED(CONFIG_LOG_MSG_DATA_EVENT),
		  log_msg_data_event, NULL);
