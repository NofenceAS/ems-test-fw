/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "cellular_controller.h"
#include "cellular_controller_events.h"
#include "messaging_module_events.h"
#include "mock_cellular_helpers.h"
#include "error_event.h"
#include "pwr_event.h"

/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(cellular_ack, 0, 1);
static K_SEM_DEFINE(cellular_proto_in, 0, 1);
static K_SEM_DEFINE(cellular_error, 0, 1);
static K_SEM_DEFINE(wake_up, 0, 1);
static K_SEM_DEFINE(send_out_sem, 0, 1);
bool simulate_modem_down = false;
extern struct k_sem listen_sem;

void reset_test_semaphores(void) {
	k_sem_reset(&cellular_ack);
	k_sem_reset(&cellular_proto_in);
	k_sem_reset(&cellular_error);
	k_sem_reset(&wake_up);
	k_sem_reset(&send_out_sem);
}

void test_init(void)
{
	reset_test_semaphores();
	struct pwr_status_event *batt_ev = new_pwr_status_event();
	batt_ev->pwr_state = PWR_NORMAL;
	batt_ev->battery_mv = 4;
	batt_ev->battery_mv_max = 4;
	batt_ev->battery_mv = 4;
	EVENT_SUBMIT(batt_ev);

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);
	int8_t err = cellular_controller_init();
	zassert_equal(err, 0, "Cellular controller initialization incomplete!");
}

static char dummy_test_msg[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
int received;
void test_socket_send_fails(void)
{
	ztest_returns_value(check_ip, 0);
	ztest_expect_value(send_tcp_q, *dummy, dummy_test_msg[0]);
	ztest_expect_value(send_tcp_q, dummy_len, sizeof(dummy_test_msg));

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	struct messaging_proto_out_event *test_msgOut =
		new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	k_sleep(K_MSEC(100));

	/* in case of failure, the sending thread will publish a
	 * stop_connection_event*/
	struct messaging_stop_connection_event *stop_connection =
		new_messaging_stop_connection_event();
	EVENT_SUBMIT(stop_connection);

	k_sleep(K_MSEC(100));

	reset_test_semaphores();
}

void test_socket_send_recovery(void)
{
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(eep_read_host_port, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);
	ztest_expect_value(send_tcp_q, *dummy, dummy_test_msg[0]);
	ztest_expect_value(send_tcp_q, dummy_len, sizeof(dummy_test_msg));

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	struct messaging_proto_out_event *test_msgOut =
		new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	struct free_message_mem_event *free_mem_ev =
		new_free_message_mem_event();
	EVENT_SUBMIT(free_mem_ev);

	int err = k_sem_take(&cellular_ack, K_MSEC(100));
	zassert_equal(err, 0,
		      "Expected cellular_ack event was not"
		      " published ");
	reset_test_semaphores();
}

void test_send_many_messages(void)
{
	char test_msg[100];
	for (int i=1; i<100; i++) {
		for (int j=0; j<i; j++) {
			test_msg[j] = (char)(i+j);
		}
		ztest_returns_value(check_ip, 0);
		ztest_expect_value(send_tcp_q, *dummy, test_msg[0]);
		ztest_expect_value(send_tcp_q, dummy_len, sizeof(test_msg));

		struct check_connection *ev = new_check_connection();
		EVENT_SUBMIT(ev);

		struct messaging_proto_out_event *test_msgOut =
			new_messaging_proto_out_event();
		test_msgOut->buf = &test_msg[0];
		test_msgOut->len = sizeof(test_msg);
		EVENT_SUBMIT(test_msgOut);

		struct free_message_mem_event *free_mem_ev =
			new_free_message_mem_event();
		EVENT_SUBMIT(free_mem_ev);

		int err = k_sem_take(&cellular_ack, K_MSEC(50));
		zassert_equal(err, 0,
			      "Expected cellular_ack event was not"
			      " published ");
	}

	/*TODO: must refactor some source files to be able to test arguments
	 * passed to send_tcp function and the free-up message memory event.*/
//	err = k_sem_take(&send_out_sem, K_MSEC(500));
//	zassert_equal(err, 0,
//		      "Expected free message memory event was not"
//		      " published ");
	reset_test_semaphores();
}



void test_publish_event_with_a_received_msg(void) /* happy scenario - msg
 * received from server is pushed to messaging module! */
{

	received = 30;
	ztest_returns_value(socket_receive, received);
	int err = k_sem_take(&cellular_proto_in, K_MSEC(300));
	zassert_equal(err, 0,
		      "Expected cellular_proto_in event was not "
		      "published ");
	/* simulate messaging module not publishing it's ack- for the next test
	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);
	 */
	reset_test_semaphores();
}

void test_ack_from_messaging_module_missed(void)
{

	received = 30;
	ztest_returns_value(socket_receive, received);
	int err = k_sem_take(&cellular_error, K_MSEC(850));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not "
		      "published ");
	err = k_sem_take(&cellular_proto_in, K_MSEC(150));
	zassert_not_equal(err, 0,
			  "Unexpected cellular_proto_in event was "
			  "published ");
	reset_test_semaphores();
}

void test_socket_rcv_fails(void)
{
	struct free_message_mem_event *dummy_ev = new_free_message_mem_event();
	EVENT_SUBMIT(dummy_ev);
	ztest_returns_value(socket_receive, -1);
	int err;
	err = k_sem_take(&cellular_error, K_MSEC(600));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on socket receive error!");
	reset_test_semaphores();
}


void test_socket_connect_fails(void)
{
	struct free_message_mem_event *dummy_ev = new_free_message_mem_event();
	EVENT_SUBMIT(dummy_ev);
	k_sleep(K_SECONDS(1));
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, -1);
	int err;
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on socket connect error!");
	reset_test_semaphores();
}

void test_notify_messaging_module_when_nudged_from_server(void)
{
	k_sem_give(&listen_sem); /*TODO: when unit testing the modem driver,
 * make sure the semaphore is given as expected.*/
	int err = k_sem_take(&wake_up, K_MSEC(100));
	zassert_equal(err, 0,
		      "Failed to notify messaging module when nudged from "
		      "server!");
	reset_test_semaphores();
}

void test_gsm_device_not_ready(void)
{
	simulate_modem_down = true;

	int8_t err = cellular_controller_init();
	zassert_not_equal(err, 0,
			  "Cellular controller initialization "
			  "incomplete!");
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on modem down!");
	reset_test_semaphores();
}

void test_main(void)
{
	ztest_test_suite(
		cellular_controller_tests, ztest_unit_test(test_init),
		ztest_unit_test(test_socket_send_fails),
		ztest_unit_test(test_socket_send_recovery),
		ztest_unit_test(test_send_many_messages),
		ztest_unit_test(test_publish_event_with_a_received_msg),
		ztest_unit_test(test_ack_from_messaging_module_missed),
		ztest_unit_test(test_socket_rcv_fails),
		ztest_unit_test(test_socket_connect_fails),
		ztest_unit_test(test_notify_messaging_module_when_nudged_from_server),
		ztest_unit_test(test_gsm_device_not_ready));

	ztest_run_test_suite(cellular_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_cellular_proto_in_event(eh)) {
		k_sem_give(&cellular_proto_in);
		printk("released semaphore for cellular_proto_in!\n");
		struct cellular_proto_in_event *ev =
			cast_cellular_proto_in_event(eh);
		if (received > 0) {
			zassert_equal(ev->len, received,
				      "Buffer length mismatch");
		}
		return false;
	} else if (is_cellular_ack_event(eh)) {
		k_sem_give(&cellular_ack);
		printk("released semaphore for cellular_ack, ");
		struct cellular_ack_event *ev =
			cast_cellular_ack_event(eh);
		if (ev->message_sent) {
			printk("sent successfully!\n");
		} else {
			printk("sending failed!\n");
		}
		return false;
	} else if (is_error_event(eh)) {
		k_sem_give(&cellular_error);
		printk("released semaphore for cellular_error!\n");
		return false;
	} else if (is_send_poll_request_now(eh)) {
		k_sem_give(&wake_up);
		printk("released semaphore for cellular_error!\n");
		return false;
	} else if (is_free_message_mem_event(eh)) {
		k_sem_give(&send_out_sem);
		return false;
	}
	return false;
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, cellular_proto_in_event);
EVENT_SUBSCRIBE(test, cellular_ack_event);
EVENT_SUBSCRIBE(test, error_event);
EVENT_SUBSCRIBE(test, send_poll_request_now);
EVENT_SUBSCRIBE(test, free_message_mem_event);
