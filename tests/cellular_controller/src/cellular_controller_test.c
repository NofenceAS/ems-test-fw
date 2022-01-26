/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "cellular_controller.h"
#include "cellular_controller_events.h"
#include ""
#include "mock_cellular_helpers.h"

/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(cellular_ack, 0, 1);
static K_SEM_DEFINE(cellular_proto_in, 0, 1);
static K_SEM_DEFINE(cellular_error, 0, 1);

void test_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
    ztest_returns_value(lteInit,1);
    ztest_returns_value(lteIsReady,true);
    ztest_returns_value(socket_connect,0);
    ztest_returns_value(socket_receive,0);

    int8_t err = cellular_controller_init();
    zassert_equal(err, 0, "Cellular controller initialization incomplete!");
}

void test_publish_event_with_a_received_msg(void) /* happy scenario - msg
 * received from server is pushed to messaging module! */
{
    ztest_returns_value(lteInit, 1);
    ztest_returns_value(lteIsReady, true);
//    ztest_returns_value(lteIsReady, 1);
    ztest_returns_value(socket_connect,0);
    ztest_returns_value(socket_receive, 20);
    int8_t err = cellular_controller_init();
    err = k_sem_take(&cellular_proto_in, K_SECONDS(30));
    zassert_equal(err, 0, "Expected cellular_proto_in event was not "
                          "published ");

//    zassert_equal(err, 0, "Cellular controller initialization incomplete!");

//    zassert_equal(received, 0, "Cellular controller initialization incomplete!");
}

void test_messaging_module_publishes_a_msg_to_send(void) /* happy scenario -
 * msg published by the messaging module is sent to server! */
{

}

void test_gsm_device_not_ready(void)
{

}

void test_socket_connect_fails(void)
{
    ztest_returns_value(lteInit, 1);
    ztest_returns_value(lteIsReady, true);
    ztest_returns_value(socket_connect, -1);
    int8_t err = cellular_controller_init();
    zassert_not_equal(err, 0, "Cellular controller initialization "
                             "incomplete!");
}

void test_socket_rcv_fails(void)
{

}

void test_socket_send_fails(void)
{

}

void test_failure_to_get_messaging_module_ack(void)
{

}

void test_connection_lost(void)
{

}

void test_main(void)
{
	ztest_test_suite(
	        cellular_controller_tests, ztest_unit_test(test_init),
                  ztest_unit_test(test_publish_event_with_a_received_msg),
                  ztest_unit_test(test_socket_connect_fails)
                  );

	ztest_run_test_suite(cellular_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
    if (is_cellular_proto_in_event(eh)) {
        k_sem_give(&cellular_proto_in);
        printk("released semaphore for cellular_proto_in!\n");
        return false;
    }
    return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, cellular_proto_in_event);
EVENT_SUBSCRIBE(test_main, cellular_ack_event);
