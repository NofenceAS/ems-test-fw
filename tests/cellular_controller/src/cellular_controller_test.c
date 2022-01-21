/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "cellular_controller.h"
#include "cellular_helpers_header.h"
#include "cellular_controller_events.h"


/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(cellular_ack, 0, 1);
static K_SEM_DEFINE(cellular_proto_in, 0, 1);
static K_SEM_DEFINE(cellular_error, 0, 1);

/*
enum test_event_id {
	TEST_EVENT_APPLY_DFU = 0,
	TEST_EVENT_EXCEED_SIZE = 1,
	TEST_EVENT_INIT = 2
};


static enum test_event_id cur_id = TEST_EVENT_INIT;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */

/*
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}
*/

void test_init(void)
{
	ztest_returns_value(dfu_target_reset, 0);
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	zassert_false(cellular_controller_init(),
		      "Error when initializing cellular controller module");
}

void test_publish_event_with_a_received_msg(void) /* happy scenario - msg
 * received from server is pushed to messaging module! */
{

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
	ztest_test_suite(cellular_controller_tests, ztest_unit_test(test_init));

	ztest_run_test_suite(cellular_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_dfu_status_event(eh)) {
		struct dfu_status_event *ev = cast_dfu_status_event(eh);

		if (cur_id == TEST_EVENT_EXCEED_SIZE) {
			if (dfu_status_sequence == 0) {
				/* In progress, no errors. */
				zassert_equal(ev->dfu_error, 0,
					      "Error reported at IN PROGRESS.");
				dfu_status_sequence = 1;
			} else if (dfu_status_sequence == 1) {
				/* Error reported, file size should exceed. */
				zassert_not_equal(ev->dfu_error, 0,
						  "No error reported \
					      when it should.");
			}
		} else if (cur_id == TEST_EVENT_APPLY_DFU) {
			/* We only give semaphore if dfu process went through
			 * and successfully changed to REBOOT flag.
			 */
			if (ev->dfu_status !=
			    DFU_STATUS_SUCCESS_REBOOT_SCHEDULED) {
				return false;
			}
		} else if (cur_id == TEST_EVENT_INIT) {
			zassert_equal(
				ev->dfu_status, DFU_STATUS_IDLE,
				"Status not idle as it should after init.");
		}
		k_sem_give(&test_status);
		return false;
	}
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, messaging_ack_event);
EVENT_SUBSCRIBE(test_main, messaging_proto_out_event);
