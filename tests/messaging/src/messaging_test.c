/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "messaging.h"
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "cellular_controller_events.h"
#include "messaging_module_events.h"

enum test_event_id { TEST_EVENT_CONTENTS = 0, TEST_EVENT_THREAD_POLLER = 1 };
static enum test_event_id cur_id = TEST_EVENT_CONTENTS;

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

/* Store dummy data. */
static uint8_t dummy_data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
struct ble_ctrl_event dummy_ctrl_event = { .param.battery = 5 };

void test_event_contents(void)
{
	/* Allocate events. */
	struct ble_ctrl_event *ev_ble_ctrl = new_ble_ctrl_event();
	struct ble_data_event *ev_ble_data = new_ble_data_event();
	struct cellular_proto_in_event *ev_lte_proto = new_cellular_proto_in_event();

	/* BLE CTRL event. */
	ev_ble_ctrl->cmd = BLE_CTRL_BATTERY_UPDATE;
	ev_ble_ctrl->param.battery = dummy_ctrl_event.param.battery;

	/* BLE data event. */
	ev_ble_data->buf = dummy_data;
	ev_ble_data->len = sizeof(dummy_data);

	/* LTE data event. */
	ev_lte_proto->buf = dummy_data;
	ev_lte_proto->len = sizeof(dummy_data);

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_ble_data);
	EVENT_SUBMIT(ev_lte_proto);
}

void test_event_poller(void)
{
	/* Allocate events. */
	struct ble_ctrl_event *ev_ble_ctrl = new_ble_ctrl_event();
	struct cellular_proto_in_event *ev_lte_proto = new_cellular_proto_in_event();

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_lte_proto);

	/* Wait for semaphores. */
	int err = k_sem_take(&ble_ctrl_sem, K_SECONDS(30));
	zassert_equal(err, 0, "ble_ctrl_sem hanged.");

	err = k_sem_take(&ble_data_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "ble_data_sem should hang.");

	err = k_sem_take(&lte_proto_sem, K_SECONDS(30));
	zassert_equal(err, 0, "lte_proto_sem hanged.");
}

void setup_take_semaphores(void)
{
	/* Make sure semaphores are taken so we depend on waiting for them
	 * from the poller thread. Remove this when we have proper
	 * outputs from the messaging module we can monitor.
	 */
	k_sem_take(&ble_ctrl_sem, K_SECONDS(30));
	k_sem_take(&ble_data_sem, K_SECONDS(30));
	k_sem_take(&lte_proto_sem, K_SECONDS(30));
}

void teardown_clear_threads(void)
{
	/* Sleep test thread so other threads can finish what they're doing
	 * before test terminates.
	 */
	k_sleep(K_SECONDS(30));
}

/* Test expected events published by messaging*/
//ack - fence_ready - ano_ready - msg_out - host_address
void test_message_out_published_on_poll_request(void)
{
//	modem_poll_work_fn();
//	ztest_returns_value(send_message, 0);
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	struct cellular_ack_event *ack = new_cellular_ack_event();
	EVENT_SUBMIT(ack);
//	messaging_module_init();
	struct update_fence_version *ev = new_update_fence_version();
	ev->fence_version = 4;
	EVENT_SUBMIT(ev);
}



/* Test expected error events published by messaging*/
// time_out waiting for ack from cellular_controller

void test_main(void)
{
	ztest_test_suite(messaging_tests, ztest_unit_test(test_init),
			 ztest_unit_test_setup_teardown(test_event_contents,
							setup_take_semaphores,
							teardown_clear_threads),

			 ztest_unit_test_setup_teardown(
				 test_event_poller, setup_take_semaphores,
				 teardown_clear_threads),

			 ztest_unit_test(test_message_out_published_on_poll_request)
			 );

	ztest_run_test_suite(messaging_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (cur_id == TEST_EVENT_CONTENTS) {
		if (is_ble_ctrl_event(eh)) {
			struct ble_ctrl_event *ev = cast_ble_ctrl_event(eh);
			switch (ev->cmd) {
			case BLE_CTRL_BATTERY_UPDATE:
				zassert_equal(ev->param.battery,
					      dummy_ctrl_event.param.battery,
					      "Battery update mismatch.");
				break;
			default:
				zassert_unreachable(
					"Unexpected command event.");
			}
		}
		if (is_ble_data_event(eh)) {
			struct ble_data_event *ev = cast_ble_data_event(eh);
			zassert_equal(ev->len, sizeof(dummy_data),
				      "Buffer length mismatch");
			zassert_mem_equal(ev->buf, dummy_data, ev->len,
					  "Contents not equal.");
		}
		if (is_cellular_proto_in_event(eh)) {
			struct cellular_proto_in_event *ev = cast_cellular_proto_in_event(eh);
			zassert_equal(ev->len, sizeof(dummy_data),
				      "Buffer length mismatch");
			zassert_mem_equal(ev->buf, dummy_data, ev->len,
					  "Contents not equal.");
			cur_id = TEST_EVENT_THREAD_POLLER;
		}
	}

	if(is_messaging_proto_out_event(eh)){
		struct messaging_proto_out_event *ev = cast_messaging_proto_out_event(eh);
		printk("length of encoded poll message = %d", ev->len);
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, cellular_proto_in_event);
EVENT_SUBSCRIBE(test_main, messaging_proto_out_event);
