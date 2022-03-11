/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_handler.h"
#include "sound_event.h"
#include "messaging_module_events.h"
#include "pasture_structure.h"
#include "nf_common.h"
#include "request_events.h"

#include "error_event.h"

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init_and_update_pasture(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	ztest_returns_value(stg_read_pasture_data, 0);
	zassert_false(amc_module_init(), "Error when initializing AMC module");
}

static bool event_handler(const struct event_header *eh)
{
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests,
			 ztest_unit_test(test_init_and_update_pasture));
	ztest_run_test_suite(amc_tests);
}

EVENT_LISTENER(test_main, event_handler);