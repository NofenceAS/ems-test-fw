/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "cellular_controller.h"
#include "cellular_controller_events.h"
#include "messaging_module_events.h"
#include "mock_cellular_helpers.h"
#include "error_event.h"

/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(cellular_ack, 0, 1);
static K_SEM_DEFINE(cellular_proto_in, 0, 1);
static K_SEM_DEFINE(cellular_error, 0, 1);
static K_SEM_DEFINE(wake_up, 0, 1);
bool simulate_modem_down = false;
extern struct k_sem listen_sem;

void test_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(eep_read_host_port, 0);
	ztest_returns_value(socket_listen, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);
	int8_t err = cellular_controller_init();
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	while (!cellular_controller_is_ready()) {
		k_sleep(K_MSEC(100));
	}
	zassert_equal(err, 0, "Cellular controller initialization incomplete!");
}

static uint8_t dummy_test_msg[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
int8_t received;
void test_publish_event_with_a_received_msg(void) /* happy scenario - msg
 * received from server is pushed to messaging module! */
{
	received = 30;
	ztest_returns_value(socket_receive, received);
	int8_t err = k_sem_take(&cellular_proto_in, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_proto_in event was not "
		      "published ");
	/* simulate messaging module not publishing it's ack- for the next test
	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);
	 */
}

void test_ack_from_messaging_module_missed(void)
{
	struct check_connection *check = new_check_connection();
	EVENT_SUBMIT(check);
	ztest_returns_value(check_ip, 0);
	received = 30;
	ztest_returns_value(socket_receive, received);

	int err = k_sem_take(&cellular_error, K_SECONDS(2));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not "
		      "published ");
	err = k_sem_take(&cellular_proto_in, K_MSEC(100));
	zassert_not_equal(err, 0,
			  "Unexpected cellular_proto_in event was "
			  "published ");
	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);
}

void test_socket_rcv_fails(void)
{
	ztest_returns_value(socket_receive, -1);
	int err;
	err = k_sem_take(&cellular_error, K_SECONDS(5));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on socket receive error!");
}


void test_socket_connect_fails(void)
{
	k_sleep(K_SECONDS(2)); //wait until the socket receive thread times
	// out and the previous connection is closed.
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, -1);
	int err;
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on socket connect error!");
}


void test_socket_send_fails(void)
{
	struct messaging_proto_out_event *test_msgIn =
		new_messaging_proto_out_event();
	test_msgIn->buf = &dummy_test_msg[0];
	test_msgIn->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgIn);
	ztest_returns_value(send_tcp_q, -1);
	int8_t err = k_sem_take(&cellular_ack, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_ack event was not"
		      "published ");
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not"
		      " published on send error!");
}

void test_notify_messaging_module_when_nudged_from_server(void)
{
	k_sem_give(&listen_sem); /*TODO: when unit testing the modem driver,
 * make sure the semaphore is given as expected.*/
	int err = k_sem_take(&wake_up, K_MSEC(100));
	zassert_equal(err, 0,
		      "Failed to notify messaging module when nudged from "
		      "server!");
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
}

void test_main(void)
{
	ztest_test_suite(
		cellular_controller_tests, ztest_unit_test(test_init),
		ztest_unit_test(test_publish_event_with_a_received_msg),
		ztest_unit_test(test_ack_from_messaging_module_missed),
		ztest_unit_test(test_socket_rcv_fails),
		ztest_unit_test(test_socket_connect_fails),
		ztest_unit_test(test_socket_send_fails),
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
		return true;
	} else if (is_cellular_ack_event(eh)) {
		k_sem_give(&cellular_ack);
		printk("released semaphore for cellular_ack!\n");
		return true;
	} else if (is_error_event(eh)) {
		k_sem_give(&cellular_error);
		printk("released semaphore for cellular_error!\n");
		return true;
	} else if (is_send_poll_request_now(eh)) {
		k_sem_give(&wake_up);
		printk("released semaphore for cellular_error!\n");
		return true;
	}
	return false;
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, cellular_proto_in_event);
EVENT_SUBSCRIBE(test, cellular_ack_event);
EVENT_SUBSCRIBE(test, error_event);
EVENT_SUBSCRIBE(test, send_poll_request_now);
