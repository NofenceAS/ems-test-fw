/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _TEST_EVENT_H_
#define _TEST_EVENT_H_

/**
 * @brief Test Event
 * @defgroup test_event Test Event
 * @{
 */

#include <event_manager.h>
#include <event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

struct test_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(test_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _ACK_EVENT_H_ */
