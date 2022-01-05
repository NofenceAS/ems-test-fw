/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "http_downloader.h"

static enum test_id cur_test_id;
static K_SEM_DEFINE(test_end_sem, 0, 1);

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));

void test_init(void)
{
	zassert_false(event_manager_init(), "Error when initializing");
}

static void test_start(enum test_id test_id)
{
	cur_test_id = test_id;
	struct test_start_event *ts = new_test_start_event();

	zassert_not_null(ts, "Failed to allocate event");
	ts->test_id = test_id;
	EVENT_SUBMIT(ts);

	int err = k_sem_take(&test_end_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test execution hanged");
}

static void test_basic(void)
{
	test_start(TEST_BASIC);
}

void test_main(void)
{
	ztest_test_suite(event_manager_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_basic));

	ztest_run_test_suite(event_manager_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_test_end_event(eh)) {
		struct test_end_event *ev = cast_test_end_event(eh);

		zassert_equal(cur_test_id, ev->test_id,
			      "End test ID does not equal start test ID");
		cur_test_id = TEST_IDLE;
		k_sem_give(&test_end_sem);

		return false;
	}

	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, test_end_event);
