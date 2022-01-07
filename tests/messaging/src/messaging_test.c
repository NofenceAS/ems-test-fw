/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "messaging.h"
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "lte_data_event.h"

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
	struct lte_data_event *ev_lte_data = new_lte_data_event();

	/* BLE CTRL event. */
	ev_ble_ctrl->cmd = BLE_CTRL_BATTERY_UPDATE;
	ev_ble_ctrl->param.battery = dummy_ctrl_event.param.battery;

	/* BLE data event. */
	ev_ble_data->buf = dummy_data;
	ev_ble_data->len = sizeof(dummy_data);

	/* LTE data event. */
	ev_lte_data->buf = dummy_data;
	ev_lte_data->len = sizeof(dummy_data);

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_ble_data);
	EVENT_SUBMIT(ev_lte_data);
}

//void test_event_poller(void)
//{
//}

void test_main(void)
{
	ztest_test_suite(messaging_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_double_schedule));
	ztest_run_test_suite(messaging_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_ble_ctrl_event(eh)) {
		struct ble_ctrl_event *ev = cast_ble_ctrl_event(eh);
		switch (ev->cmd) {
		case BLE_CTRL_BATTERY_UPDATE:
			zassert_equal(ev->param.battery,
				      dummy_ctrl_event.param.battery,
				      "Battery update mismatch.");
			break;
		default:
			zassert_unreachable("Unexpected command event.");
		}
	}
	if (is_ble_data_event(eh)) {
		struct ble_data_event *ev = cast_ble_data_event(eh);
		zassert_equal(ev->len, sizeof(dummy_data),
			      "Buffer length mismatch");
		zassert_mem_equal(ev->buf, dummy_data, ev->len,
				  "Contents not equal.");
	}
	if (is_lte_data_event(eh)) {
		struct lte_data_event *ev = cast_lte_data_event(eh);
		zassert_equal(ev->len, sizeof(dummy_data),
			      "Buffer length mismatch");
		zassert_mem_equal(ev->buf, dummy_data, ev->len,
				  "Contents not equal.");
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, lte_data_event);
