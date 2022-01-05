/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));

void test_init(void)
{
	zassert_false(event_manager_init(), "Error when initializing");
}

void test_main(void)
{
	ztest_test_suite(event_manager_tests, ztest_unit_test(test_init));

	ztest_run_test_suite(event_manager_tests);
}

static bool event_handler(const struct event_header *eh)
{
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
