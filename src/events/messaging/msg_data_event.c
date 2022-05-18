/*
 * Copyright (c) 2021 Nofence AS
 */

#include <stdio.h>

#include "msg_data_event.h"

EVENT_TYPE_DEFINE(msg_data_event, IS_ENABLED(CONFIG_LOG_MSG_DATA_EVENT),
		  NULL, NULL);
