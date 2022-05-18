/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include <zephyr.h>
#include "movement_events.h"
#include "movement_controller.h"
#include "event_manager.h"

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	ztest_returns_value(sensor_attr_set, 0);
	ztest_returns_value(sensor_trigger_set, 0);

	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	zassert_false(init_movement_controller(),
		      "Error when initializing movement controller");
}

void test_main(void)
{
	ztest_test_suite(movement_tests, ztest_unit_test(test_init));
	ztest_run_test_suite(movement_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_movement_out_event(eh)) {
		return false;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, movement_out_event);
