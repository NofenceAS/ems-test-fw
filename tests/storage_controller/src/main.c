/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_event_manager_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

void test_storage_init(void)
{
	zassert_false(init_storage_controller(),
		      "Error when initializing storage controller.");
}

void test_main(void)
{
	ztest_test_suite(storage_controller_tests,
			 ztest_unit_test(test_event_manager_init),
			 ztest_unit_test(test_storage_init));
	ztest_run_test_suite(storage_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, stg_write_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_read_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_ack_read_memrec_event);
