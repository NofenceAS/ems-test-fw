/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "watchdog_event.h"
#include <stdio.h>
#include <string.h>

static K_SEM_DEFINE(watchdog_feed_sem, 0, 1);

uint8_t module_alive_array_seen[WDG_END_OF_LIST];

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
	zassert_false(event_manager_init(), "Error when initializing event manager");
}

void test_wathcdog_feed(void)
{
	memset(module_alive_array_seen, 0, sizeof(module_alive_array_seen));
	watchdog_report_module_alive(WDG_PWR_MODULE);
	watchdog_report_module_alive(WDG_BLE_SCAN);
	int err = k_sem_take(&watchdog_feed_sem, K_SECONDS(CONFIG_WATCHDOG_TIMEOUT_SEC));
	zassert_equal(err, 0, "Watchdog feed error");
}

void test_wathcdog_timeout(void)
{
	memset(module_alive_array_seen, 0, sizeof(module_alive_array_seen));
	watchdog_report_module_alive(WDG_PWR_MODULE);
	memset(module_alive_array_seen, 0, sizeof(module_alive_array_seen));
	int err = k_sem_take(&watchdog_feed_sem, K_SECONDS(CONFIG_WATCHDOG_TIMEOUT_SEC));
	zassert_equal(err, -EAGAIN, "Timeout test error");
}

void test_main(void)
{
	ztest_test_suite(watchdog_handler_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_wathcdog_feed),
			 ztest_unit_test(test_wathcdog_timeout));

	ztest_run_test_suite(watchdog_handler_tests);
}

uint8_t expected_array_seen[WDG_END_OF_LIST] = { [0 ... WDG_END_OF_LIST - 1] = 1 };

static bool event_handler(const struct event_header *eh)
{
	if (is_watchdog_alive_event(eh)) {
		struct watchdog_alive_event *ev = cast_watchdog_alive_event(eh);
		if (ev->magic == WATCHDOG_ALIVE_MAGIC) {
			if (ev->module < WDG_END_OF_LIST) {
				module_alive_array_seen[ev->module] = 1;
				int ret = memcmp(module_alive_array_seen, expected_array_seen,
						 WDG_END_OF_LIST);
				if (ret == 0) {
					printk("All modules alive. Feeding watchdog.\n");
					k_sem_give(&watchdog_feed_sem);
				}
			}
		}
		return false;
	}
	zassert_true(false, "Unexpected event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, watchdog_alive_event);
