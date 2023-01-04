/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "gnss_controller.h"
#include "gnss_controller_events.h"
#include "error_event.h"
#include "mock_gnss.h"

/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(gnss_data_out, 0, 1);
static K_SEM_DEFINE(error, 0, 1);

static int timeout_count;
static int data_count;

extern k_tid_t pub_gnss_thread_id;

const int DEFAULT_MIN_RATE_MS = CONFIG_GNSS_MINIMUM_ALLOWED_GNSS_RATE;
const int DEFAULT_SLACK_TIME_TIMEOUT_MS = CONFIG_GNSS_TIMEOUT_SLACK_MS;
const int DEFAULT_TIMEOUTS_BEFORE_RESET = CONFIG_GNSS_TIMEOUTS_BEFORE_RESET;

void test_init_ok(void)
{
	zassert_false(event_manager_init(), "Error when initializing event manager");

	if (pub_gnss_thread_id != 0) {
		k_thread_abort(pub_gnss_thread_id);
	}
	ztest_returns_value(mock_gnss_wakeup, 0);
	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_setup, 0);
	int8_t err = gnss_controller_init();
	zassert_equal(err, 0, "Gnss controller initialization incomplete!");
	zassert_equal(z_cleanup_mock(), 0, "");
}

void test_init_fails1(void)
{
	for (int i = 0; i < (CONFIG_GNSS_INIT_MAX_COUNT); i++) {
		ztest_returns_value(mock_gnss_set_data_cb, -1);
	}
	int8_t ret = gnss_controller_init();

	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}

void test_init_fails2(void)
{
	for (int i = 0; i < (CONFIG_GNSS_INIT_MAX_COUNT); i++) {
		ztest_returns_value(mock_gnss_set_data_cb, 0);
		ztest_returns_value(mock_gnss_wakeup, -1);
	}

	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}

void test_init_fails3(void)
{
	for (int i = 0; i < (CONFIG_GNSS_INIT_MAX_COUNT); i++) {
		ztest_returns_value(mock_gnss_set_data_cb, 0);
		ztest_returns_value(mock_gnss_wakeup, 0);
		ztest_returns_value(mock_gnss_setup, -1);
	}

	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}

static const gnss_t default_data = { .latest = { .lat = 633743868, .lon = 103412316 },
				     .fix_ok = true,
				     .lastfix = { .unix_timestamp = 100 },
				     .has_lastfix = true };

static gnss_t dummy_gnss_data;

void test_publish_event_with_gnss_data_callback(void)
{
	test_init_ok();
	memcpy(&dummy_gnss_data, &default_data, sizeof(default_data));
	simulate_new_gnss_data(dummy_gnss_data);
	int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.5));
	zassert_equal(err, 0, "Expected gnss data event was not published!");
}

void setup_mock_reset(uint16_t *dummy_rate, uint16_t mask)
{
	ztest_returns_value(mock_gnss_wakeup, 0);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, mask);
	ztest_returns_value(mock_gnss_setup, 0);
	ztest_expect_value(mock_gnss_set_power_mode, mode, GNSSMODE_MAX);
	ztest_returns_value(mock_gnss_set_power_mode, 0);
	ztest_return_data(mock_gnss_get_rate, rate, dummy_rate);
	ztest_returns_value(mock_gnss_get_rate, 0);
}

void test_gnss_timeout_and_resets(void)
{
	static uint16_t dummy_rate = DEFAULT_MIN_RATE_MS;
	memcpy(&dummy_gnss_data, &default_data, sizeof(default_data));
	ztest_returns_value(mock_gnss_wakeup, 0);
	ztest_expect_value(mock_gnss_set_power_mode, mode, GNSSMODE_MAX);
	ztest_returns_value(mock_gnss_set_power_mode, 0);

	ztest_returns_value(mock_gnss_get_rate, 0);
	ztest_return_data(mock_gnss_get_rate, rate, &dummy_rate);

	/* set controller to expect max data rate */
	struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_MAX;
	EVENT_SUBMIT(ev);
	k_sleep(K_MSEC(DEFAULT_MIN_RATE_MS + 10));

	/* Data arrives on time */
	data_count = 0;
	simulate_new_gnss_data(dummy_gnss_data);
	int8_t err = k_sem_take(&gnss_data_out, K_MSEC(DEFAULT_MIN_RATE_MS));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(data_count, 1, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Sleep for long enough time for the gnss thread to get N timeouts, but no reset */
	timeout_count = 0;
	data_count = 0;
	k_sleep(K_MSEC((DEFAULT_MIN_RATE_MS + DEFAULT_SLACK_TIME_TIMEOUT_MS + 10) *
		       DEFAULT_TIMEOUTS_BEFORE_RESET));
	err = k_sem_take(&gnss_data_out, K_MSEC(1000));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 3, "");
	zassert_equal(data_count, 0, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Sleep one more time to get a HOT reset */
	timeout_count = 0;
	data_count = 0;
	setup_mock_reset(&dummy_rate, GNSS_RESET_MASK_HOT);
	k_sleep(K_MSEC(DEFAULT_MIN_RATE_MS + DEFAULT_SLACK_TIME_TIMEOUT_MS + 10));

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 1, "");
	zassert_equal(data_count, 0, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Sleep for more time to get a WARM reset */
	timeout_count = 0;
	data_count = 0;
	setup_mock_reset(&dummy_rate, GNSS_RESET_MASK_WARM);
	k_sleep(K_MSEC((DEFAULT_MIN_RATE_MS + DEFAULT_SLACK_TIME_TIMEOUT_MS + 10) *
		       (DEFAULT_TIMEOUTS_BEFORE_RESET + 1)));
	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 4, "");
	zassert_equal(data_count, 0, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Sleep for more time to get a COLD reset */
	timeout_count = 0;
	data_count = 0;
	setup_mock_reset(&dummy_rate, GNSS_RESET_MASK_COLD);
	k_sleep(K_MSEC((DEFAULT_MIN_RATE_MS + DEFAULT_SLACK_TIME_TIMEOUT_MS + 10) *
		       (DEFAULT_TIMEOUTS_BEFORE_RESET + 1)));
	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 4, "");
	zassert_equal(data_count, 0, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Generate data again */
	timeout_count = 0;
	data_count = 0;
	simulate_new_gnss_data(dummy_gnss_data);
	err = k_sem_take(&gnss_data_out, K_MSEC(DEFAULT_MIN_RATE_MS));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(data_count, 1, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	/* Special case, Cold-reset the receiver if MSSS > 49 days */
	timeout_count = 0;
	data_count = 0;
	setup_mock_reset(&dummy_rate, GNSS_RESET_MASK_COLD);
	dummy_gnss_data.latest.msss = 4233600000 + 1;
	simulate_new_gnss_data(dummy_gnss_data);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 0, "");
	zassert_equal(data_count, 1, "");
	zassert_equal(z_cleanup_mock(), 0, "");
}

void test_semisteady_gnss_data_stream(void)
{
	k_thread_suspend(pub_gnss_thread_id);
	test_init_ok();
	memcpy(&dummy_gnss_data, &default_data, sizeof(default_data));
	/* set controller to expect max data rate */
	uint16_t dummy_rate = DEFAULT_MIN_RATE_MS;
	ztest_returns_value(mock_gnss_wakeup, 0);
	ztest_expect_value(mock_gnss_set_power_mode, mode, GNSSMODE_MAX);
	ztest_returns_value(mock_gnss_set_power_mode, 0);
	ztest_return_data(mock_gnss_get_rate, rate, &dummy_rate);
	ztest_returns_value(mock_gnss_get_rate, 0);

	struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_MAX;
	EVENT_SUBMIT(ev);
	k_sleep(K_SECONDS(0.25));
	zassert_equal(z_cleanup_mock(), 0, "");

	k_thread_resume(pub_gnss_thread_id);
	// Receiving 1000 data points
	for (int i = 0; i < 100; i++) {
		simulate_new_gnss_data(dummy_gnss_data);
		int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.3));
		zassert_equal(err, 0, "Expected gnss data event was not published!");
	}

	zassert_equal(z_cleanup_mock(), 0, "");
	/* Drop one data point */
	timeout_count = 0;
	k_sleep(K_MSEC(DEFAULT_MIN_RATE_MS + DEFAULT_SLACK_TIME_TIMEOUT_MS + 10));

	int err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");
	zassert_equal(timeout_count, 1, "");
	zassert_equal(z_cleanup_mock(), 0, "");

	// Receiving 100 data points
	for (int i = 0; i < 100; i++) {
		simulate_new_gnss_data(dummy_gnss_data);
		int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.5));
		zassert_equal(err, 0, "Expected gnss data event was not published!");
		k_sleep(K_SECONDS(0.25));
	}
}

static void test_gnss_set_backup_mode_on_inactive_event(void)
{
	ztest_returns_value(mock_gnss_wakeup, 0);
	ztest_returns_value(mock_gnss_set_backup_mode, 0);
	struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_INACTIVE;
	EVENT_SUBMIT(ev);
	k_sleep(K_SECONDS(0.25));
}

static bool event_handler(const struct event_header *eh)
{
	int ret;
	if (is_gnss_data(eh)) {
		struct gnss_data *ev = cast_gnss_data(eh);
		gnss_t new_data = ev->gnss_data;
		if (!ev->timed_out) {
			data_count++;
			k_sem_give(&gnss_data_out);
			ret = memcmp(&new_data, &dummy_gnss_data, sizeof(gnss_t));
			zassert_equal(ret, 0, "Published GNSS data mis-match");
		} else {
			timeout_count++;
			uint8_t *raw_gnss = (uint8_t *)&new_data;
			for (int i = 0; i < sizeof(gnss_t); i++) {
				zassert_equal(raw_gnss[i], 0, "Non-zero byte during GNSS timeout!");
			}
			k_sem_give(&gnss_data_out);
		}
		return false;
	} else if (is_error_event(eh)) {
		k_sem_give(&error);
		printk("released semaphore for error event!\n");
		return false;
	}
	return false;
}

void test_main(void)
{
	ztest_test_suite(gnss_controller_tests, ztest_unit_test(test_init_ok),
			 ztest_unit_test(test_init_fails1), ztest_unit_test(test_init_fails2),
			 ztest_unit_test(test_init_fails3),
			 ztest_unit_test(test_publish_event_with_gnss_data_callback),
			 ztest_unit_test(test_gnss_timeout_and_resets),
			 ztest_unit_test(test_semisteady_gnss_data_stream),
			 ztest_unit_test(test_gnss_set_backup_mode_on_inactive_event));

	ztest_run_test_suite(gnss_controller_tests);
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, gnss_data);
EVENT_SUBSCRIBE(test, error_event);
