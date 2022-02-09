/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "messaging.h"
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "lte_proto_event.h"

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
	struct lte_proto_event *ev_lte_proto = new_lte_proto_event();

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
	struct ble_data_event *ev_ble_data = new_ble_data_event();
	struct lte_proto_event *ev_lte_proto = new_lte_proto_event();

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_ble_data);
	EVENT_SUBMIT(ev_lte_proto);

	/* Wait for semaphores. */
	int err = k_sem_take(&ble_ctrl_sem, K_SECONDS(30));
	zassert_equal(err, 0, "ble_ctrl_sem hanged.");

	err = k_sem_take(&ble_data_sem, K_SECONDS(30));
	zassert_equal(err, 0, "ble_data_sem hanged.");

	err = k_sem_take(&lte_proto_sem, K_SECONDS(30));
	zassert_equal(err, 0, "lte_proto_sem hanged.");
}

void test_main(void)
{
	ztest_test_suite(messaging_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_event_contents),
			 ztest_unit_test(test_event_poller));
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
		if (is_lte_proto_event(eh)) {
			struct lte_proto_event *ev = cast_lte_proto_event(eh);
			zassert_equal(ev->len, sizeof(dummy_data),
				      "Buffer length mismatch");
			zassert_mem_equal(ev->buf, dummy_data, ev->len,
					  "Contents not equal.");
			cur_id = TEST_EVENT_THREAD_POLLER;
		}
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, lte_proto_event);
