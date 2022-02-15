/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include <zephyr.h>
#include <drivers/sensor.h>
#include "env_sensor_event.h"

K_SEM_DEFINE(env_data_sem, 0, 1);

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
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

void test_event_contents(void)
{
	ztest_returns_value(sensor_sample_fetch, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);

	struct request_env_sensor_event *ev = new_request_env_sensor_event();
	EVENT_SUBMIT(ev);

	int err = k_sem_take(&env_data_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_main(void)
{
	ztest_test_suite(env_sensor_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_event_contents));
	ztest_run_test_suite(env_sensor_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_env_sensor_event(eh)) {
		k_sem_give(&env_data_sem);
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, env_sensor_event);
