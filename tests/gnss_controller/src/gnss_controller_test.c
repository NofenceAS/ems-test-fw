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

static bool expecting_timeout = false;

void test_init_ok(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);
	int8_t err = gnss_controller_init();

	zassert_equal(err, 0, "Gnss controller initialization incomplete!");
}

void test_init_fails1(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}

void test_init_fails2(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_setup, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}

void test_init_fails3(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0, "Expected error event was not published!");
	zassert_equal(ret, -1,
		      "Gnss controller initialization "
		      "incomplete!");
}
//
//static uint8_t dummy_test_msg[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
//int8_t received;
const gnss_t dummy_gnss_data = {
	.latest = {
		.lat = 633743868,
		.lon = 103412316
	//	/** Corrected position.*/
	//	int16_t x;
	//
	//	/** Corrected position.*/
	//	int16_t y;
	//
	//	/** Set if overflow because of too far away from origin position.*/
	//	uint8_t overflow;
	//
	//	/** Height above ellipsoid [mm].*/
	//	int16_t height;
	//
	//	/** Ground speed 2-D [mm/s]*/
	//	uint16_t speed;
	//
	//	/** Movement direction (-18000 to 18000 Hundred-deg).*/
	//	int16_t head_veh;
	//
	//	/** Horizontal dilution of precision.*/
	//	uint16_t h_dop;
	//
	//	/** Horizontal Accuracy Estimate [mm].*/
	//	uint16_t h_acc_dm;
	//
	//	/** Vertical Accuracy Estimate [mm].*/
	//	uint16_t v_acc_dm;
	//
	//	/** Heading accuracy estimate [Hundred-deg].*/
	//	uint16_t head_acc;
	//
	//	/** Number of SVs used in Nav Solution.*/
	//	uint8_t num_sv;
	//
	//	/** UBX-NAV-PVT flags as copied.*/
	//	uint8_t pvt_flags;
	//
	//	/** UBX-NAV-PVT valid flags as copies.*/
	//	uint8_t pvt_valid;
	//
	//	/** Milliseconds system time when data was updated from GNSS.*/
	//	uint32_t updated_at;
	//
	//	/** UBX-NAV-STATUS milliseconds since receiver start or reset.*/
	//	uint32_t msss;
	//
	//	/** UBX-NAV-STATUS milliseconds since First Fix.*/
	//	uint32_t ttff;
	},
	.fix_ok = true,
	.lastfix = {},
	.has_lastfix = true
};

void test_publish_event_with_gnss_data_callback(void)
{
	test_init_ok();
	simulate_new_gnss_data(dummy_gnss_data);
	int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.5));
	zassert_equal(err, 0, "Expected gnss data event was not published!");
	k_sleep(K_SECONDS(1));
}

void test_old_gnss_last_fix_callback1(void)
{
	test_init_ok();
	//fix arrives on time
	simulate_new_gnss_data(dummy_gnss_data);
	int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 6 seconds
	k_sleep(K_SECONDS(6));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_HOT);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 11 seconds
	k_sleep(K_SECONDS(11));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_WARM);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 21 seconds
	k_sleep(K_SECONDS(21));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_COLD);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives on time
	expecting_timeout = false;
	simulate_new_gnss_data(dummy_gnss_data);
	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 6 seconds
	k_sleep(K_SECONDS(6));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_HOT);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 11 seconds
	k_sleep(K_SECONDS(11));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_WARM);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	//fix arrives after 21 seconds
	k_sleep(K_SECONDS(21));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_COLD);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	k_sleep(K_SECONDS(21));
	expecting_timeout = true;
	simulate_new_gnss_data(dummy_gnss_data);
	ztest_returns_value(mock_gnss_reset, 0);
	ztest_expect_value(mock_gnss_reset, mask, GNSS_RESET_MASK_COLD);

	ztest_returns_value(mock_gnss_setup, 0);
	ztest_returns_value(mock_gnss_set_rate, 0);
	ztest_returns_value(mock_gnss_get_rate, 0);

	err = k_sem_take(&gnss_data_out, K_SECONDS(0.1));
	zassert_equal(err, 0, "Expected gnss fix event was not published!");

	err = k_sem_take(&error, K_SECONDS(26));
	zassert_equal(err, 0, "Expected error event was not published!");
}

void test_main(void)
{
	ztest_test_suite(
		gnss_controller_tests, ztest_unit_test(test_init_ok),
		ztest_unit_test(test_publish_event_with_gnss_data_callback),
		ztest_unit_test(test_init_fails1),
		ztest_unit_test(test_init_fails2),
		ztest_unit_test(test_init_fails3)
		/** @note re-implement reset logic. */
		/*ztest_unit_test(test_old_gnss_last_fix_callback1)*/);

	ztest_run_test_suite(gnss_controller_tests);
}
//
static bool event_handler(const struct event_header *eh)
{
	int ret;
	if (is_gnss_data(eh)) {
		struct gnss_data *ev = cast_gnss_data(eh);
		gnss_t new_data = ev->gnss_data;
		zassert_equal(ev->timed_out, expecting_timeout,
			      "Unexpected timeout state received from GNSS!");
		if (!ev->timed_out) {
			printk("released semaphore for gnss data cb!\n");
			k_sem_give(&gnss_data_out);
			ret = memcmp(&new_data, &dummy_gnss_data,
				     sizeof(gnss_t));
			zassert_equal(ret, 0, "Published GNSS data mis-match");
		} else {
			expecting_timeout = false;
			uint8_t *raw_gnss = (uint8_t *)&new_data;
			for (int i = 0; i < sizeof(gnss_t); i++) {
				zassert_equal(
					raw_gnss[i], 0,
					"Non-zero byte during GNSS timeout!");
			}
		}
		return false;
	} else if (is_error_event(eh)) {
		k_sem_give(&error);
		printk("released semaphore for error event!\n");
		return false;
	}
	return false;
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, gnss_data);
EVENT_SUBSCRIBE(test, error_event);

//EVENT_SUBSCRIBE(test, cellular_error_event);
