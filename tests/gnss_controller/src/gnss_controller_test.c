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
static K_SEM_DEFINE(gnss_new_fix, 0, 1);
static K_SEM_DEFINE(error, 0, 1);

void test_init_ok(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_set_lastfix_cb, 0);
	ztest_returns_value(mock_gnss_setup, 0);
	int8_t err = gnss_controller_init();

	zassert_equal(err, 0, "Gnss controller initialization incomplete!");
}

void test_init_fails1(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0,
		      "Expected error event was not published!");
	zassert_equal(ret, -1, "Gnss controller initialization "
				    "incomplete!");
}

void test_init_fails2(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_set_lastfix_cb, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0,
		      "Expected error event was not published!");
	zassert_equal(ret, -1, "Gnss controller initialization "
			       "incomplete!");
}

void test_init_fails3(void)
{
	ztest_returns_value(mock_gnss_set_data_cb, 0);
	ztest_returns_value(mock_gnss_set_lastfix_cb, 0);
	ztest_returns_value(mock_gnss_setup, -1);
	int8_t ret = gnss_controller_init();
	int8_t err = k_sem_take(&error, K_MSEC(100));
	zassert_equal(err, 0,
		      "Expected error event was not published!");
	zassert_equal(ret, -1, "Gnss controller initialization "
			       "incomplete!");
}
//
//static uint8_t dummy_test_msg[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
//int8_t received;
const gnss_struct_t dummy_gnss_data = {
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
};

const gnss_last_fix_struct_t dummy_gnss_fix;

void test_publish_event_with_gnss_data_callback(void)
{
	test_init_ok();
	simulate_new_gnss_data(dummy_gnss_data);
	int8_t err = k_sem_take(&gnss_data_out, K_SECONDS(0.5));
	zassert_equal(err, 0,
		      "Expected gnss data event was not published!");
}

void test_publish_event_with_gnss_last_fix_callback(void)
{
	test_init_ok();
	simulate_new_gnss_last_fix(dummy_gnss_fix);
	int8_t err = k_sem_take(&gnss_new_fix, K_SECONDS(0.1));
	zassert_equal(err, 0,
		      "Expected gnss fix event was not published!");
}

//
//void test_ack_from_messaging_module_missed(void)
//{
//	test_publish_event_with_a_received_msg(); /* first transaction */
//
//	struct messaging_proto_out_event *test_msgIn =
//		new_messaging_proto_out_event();
//	test_msgIn->buf = dummy_test_msg;
//	test_msgIn->len = sizeof(dummy_test_msg);
//
//	EVENT_SUBMIT(test_msgIn);
//	ztest_returns_value(socket_connect, 0);
//	ztest_returns_value(send_tcp, sizeof(dummy_test_msg));
//	ztest_returns_value(socket_receive, received);
//
//	int8_t err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not "
//		      "published ");
//
//	err = k_sem_take(&cellular_proto_in, K_SECONDS(1));
//	zassert_not_equal(err, 0,
//			  "Unexpected cellular_proto_in event was "
//			  "published ");
//}
//
//void test_gsm_device_not_ready(void)
//{
//	simulate_modem_down = true;
//	int8_t err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not"
//		      " published on modem down!");
//	err = cellular_controller_init();
//	zassert_not_equal(err, 0,
//			  "Cellular controller initialization "
//			  "incomplete!");
//}
//
//void test_socket_connect_fails(void)
//{
//	ztest_returns_value(lte_init, 1);
//	ztest_returns_value(lte_is_ready, true);
//	ztest_returns_value(eep_read_host_port, 0);
//	ztest_returns_value(socket_connect, -1);
//
//	int8_t err = cellular_controller_init();
//	zassert_not_equal(err, 0,
//			  "Cellular controller initialization "
//			  "incomplete!");
//	err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not"
//		      " published on socket connect error!");
//}
//
//void test_socket_rcv_fails(void)
//{
//	test_init();
//	received = -1;
//	struct messaging_proto_out_event *test_msgIn =
//		new_messaging_proto_out_event();
//	test_msgIn->buf = dummy_test_msg;
//	test_msgIn->len = sizeof(dummy_test_msg);
//	EVENT_SUBMIT(test_msgIn);
//
//	ztest_returns_value(socket_connect, 0);
//	ztest_returns_value(send_tcp, 0);
//	ztest_returns_value(socket_receive, -1);
//	int8_t err = k_sem_take(&cellular_ack, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_ack event was not"
//		      "published ");
//	err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not"
//		      " published on receive error!");
//}
//
//void test_socket_send_fails(void)
//{
//	test_init();
//	received = 10;
//	struct messaging_proto_out_event *test_msgIn =
//		new_messaging_proto_out_event();
//	test_msgIn->buf = dummy_test_msg;
//	test_msgIn->len = sizeof(dummy_test_msg);
//	EVENT_SUBMIT(test_msgIn);
//
//	ztest_returns_value(socket_connect, 0);
//	ztest_returns_value(send_tcp, -1);
//	int8_t err = k_sem_take(&cellular_ack, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_ack event was not"
//		      "published ");
//	err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not"
//		      " published on send error!");
//}
//
//void test_socket_reconnect_fails(void)
//{
//	test_init();
//	received = -1;
//	struct messaging_proto_out_event *test_msgIn =
//		new_messaging_proto_out_event();
//	test_msgIn->buf = dummy_test_msg;
//	test_msgIn->len = sizeof(dummy_test_msg);
//	EVENT_SUBMIT(test_msgIn);
//
//	ztest_returns_value(socket_connect, -1);
//	int8_t err = k_sem_take(&cellular_ack, K_SECONDS(1));
//	zassert_equal(err, 0, "Expected cellular_ack event was not published ");
//	err = k_sem_take(&cellular_error, K_SECONDS(1));
//	zassert_equal(err, 0,
//		      "Expected cellular_error event was not"
//		      " published on socket connect error!");
//}

void test_main(void)
{
	ztest_test_suite(
		gnss_controller_tests, ztest_unit_test(test_init_ok),
		ztest_unit_test(test_publish_event_with_gnss_data_callback),
		ztest_unit_test(test_publish_event_with_gnss_last_fix_callback),
		ztest_unit_test(test_init_fails1),
		ztest_unit_test(test_init_fails2),
		ztest_unit_test(test_init_fails3));

	ztest_run_test_suite(gnss_controller_tests);
}
//
static bool event_handler(const struct event_header *eh)
{
	int ret;
	if (is_new_gnss_data(eh)) {
		k_sem_give(&gnss_data_out);
		printk("released semaphore for gnss data cb!\n");
		struct new_gnss_data *ev = cast_new_gnss_data(eh);
		gnss_struct_t new_data = ev->gnss_data;
		ret = memcmp(&new_data, &dummy_gnss_data, sizeof
				 (gnss_struct_t));
		zassert_equal(ret, 0, "Published GNSS data mis-match");
		return false;
	} else if (is_error_event(eh)) {
		k_sem_give(&error);
		printk("released semaphore for error event!\n");
		return false;
	} else if (is_new_gnss_fix(eh)) {
		k_sem_give(&gnss_new_fix);
		printk("released semaphore for gnss fix cb!\n");
		struct new_gnss_fix *ev = cast_new_gnss_fix(eh);
		gnss_last_fix_struct_t new_data = ev->fix;
		ret = memcmp(&new_data, &dummy_gnss_fix,
			     sizeof(gnss_last_fix_struct_t));
		zassert_equal(ret, 0, "Published GNSS fix mis-match");
		return false;
	}
	return false;
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, new_gnss_data);
EVENT_SUBSCRIBE(test, new_gnss_fix);
EVENT_SUBSCRIBE(test, error_event);

//EVENT_SUBSCRIBE(test, cellular_error_event);
