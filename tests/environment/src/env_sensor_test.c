/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include <zephyr.h>
#include <drivers/sensor.h>
#include "env_sensor_event.h"
#include "error_event.h"

K_SEM_DEFINE(env_data_sem, 0, 1);

test_id_t cur_test = TEST_SANITY_PASS;

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
	/* We want to test and say that sanity should pass. */
	cur_test = TEST_SANITY_PASS;
	ztest_returns_value(sensor_sample_fetch, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);

	struct request_env_sensor_event *ev = new_request_env_sensor_event();
	EVENT_SUBMIT(ev);

	int err = k_sem_take(&env_data_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_event_contents_sanity_fail_temp(void)
{
	/* We want to test and say that sanity should pass. */
	cur_test = TEST_SANITY_FAIL_TEMP;
	ztest_returns_value(sensor_sample_fetch, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);

	struct request_env_sensor_event *ev = new_request_env_sensor_event();
	EVENT_SUBMIT(ev);

	/* Should hang since it never submits the event since sanity fails. */
	int err = k_sem_take(&env_data_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
}

void test_event_contents_sanity_fail_pressure(void)
{
	/* We want to test and say that sanity should pass. */
	cur_test = TEST_SANITY_FAIL_PRESSURE;
	ztest_returns_value(sensor_sample_fetch, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);

	struct request_env_sensor_event *ev = new_request_env_sensor_event();
	EVENT_SUBMIT(ev);

	/* Should hang since it never submits the event since sanity fails. */
	int err = k_sem_take(&env_data_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
}

void test_event_contents_sanity_fail_humidity(void)
{
	/* We want to test and say that sanity should pass. */
	cur_test = TEST_SANITY_FAIL_HUMIDITY;
	ztest_returns_value(sensor_sample_fetch, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);
	ztest_returns_value(sensor_channel_get, 0);

	struct request_env_sensor_event *ev = new_request_env_sensor_event();
	EVENT_SUBMIT(ev);

	/* Should hang since it never submits the event since sanity fails. */
	int err = k_sem_take(&env_data_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
}

void test_main(void)
{
	ztest_test_suite(
		env_sensor_tests, ztest_unit_test(test_init),
		ztest_unit_test(test_event_contents),
		ztest_unit_test(test_event_contents_sanity_fail_temp),
		ztest_unit_test(test_event_contents_sanity_fail_pressure),
		ztest_unit_test(test_event_contents_sanity_fail_humidity));
	ztest_run_test_suite(env_sensor_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_env_sensor_event(eh)) {
		k_sem_give(&env_data_sem);
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		zassert_equal(ev->sender, ERR_SENDER_ENV_SENSOR,
			      "Mismatched sender.");

		zassert_equal(ev->severity, ERR_SEVERITY_ERROR,
			      "Mismatched severity.");

		zassert_equal(ev->code, -ERANGE, "Mismatched error code.");
		char *expected_message;
		if (cur_test == TEST_SANITY_FAIL_TEMP) {
			expected_message =
				"Sanity check failed for temperature.";
		} else if (cur_test == TEST_SANITY_FAIL_PRESSURE) {
			expected_message = "Sanity check failed for pressure.";
		} else if (cur_test == TEST_SANITY_FAIL_HUMIDITY) {
			expected_message = "Sanity check failed for humidity.";
		} else {
			expected_message = "Unreachable.";
		}

		zassert_equal(ev->dyndata.size, strlen(expected_message),
			      "Mismatched message length.");

		zassert_mem_equal(ev->dyndata.data, expected_message,
				  ev->dyndata.size, "Mismatched user message.");
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, env_sensor_event);
EVENT_SUBSCRIBE(test_main, error_event);
