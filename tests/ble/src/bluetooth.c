/*
 * Copyright (c) 2022 Nofence AS
 */

#include <zephyr.h>

#include <ztest.h>
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "ble_conn_event.h"

#include "ble_controller.h"
#include "event_manager.h"

static K_SEM_DEFINE(test_status, 0, 1);

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init_ok(void)
{
	ztest_returns_value(bt_enable, 0);
	ztest_returns_value(bt_nus_init, 0);
	ztest_returns_value(bt_le_adv_start, 0);

	zassert_equal(event_manager_init(), 0,
		      "Error when initializing event manager");

	zassert_equal(ble_module_init(), 0,
		      "Error when initializing ble module");
}

/* Checik if ble_module_init fails sucessfully */
void test_init_error(void)
{
	ztest_returns_value(bt_enable, -2);
	zassert_equal(event_manager_init(), 0,
		      "Error when initializing event manager");

	zassert_equal(ble_module_init(), -2,
		      "Error when initializing ble module");
}

void test_advertise(void)
{
	// TODO: Let adv be event controlled
	// Currently ble_module_init() also starts adv.
	// Check if adv is on after ble_ready
	// Submut event to start adv
}

void test_ble_connection(void)
{
	struct ble_conn_event *event = new_ble_conn_event();
	event->conn_state = BLE_STATE_CONNECTED;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&test_status, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

void test_ble_disconnection(void)
{
	struct ble_conn_event *event = new_ble_conn_event();
	event->conn_state = BLE_STATE_DISCONNECTED;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&test_status, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

void test_ble_data_event(void)
{
	struct ble_data_event *event = new_ble_data_event();
	uint8_t data_to_send[2] = { 0x01, 0x02 };
	event->buf = data_to_send;
	event->len = sizeof(data_to_send);
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&test_status, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

/* Submit a battery change */
void test_ble_ctrl_event(void)
{
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_BATTERY_UPDATE;
	event->param.battery = 65;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&test_status, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

/* Test case main entry */
void test_main(void)
{
	ztest_test_suite(test_bluetooth, ztest_unit_test(test_init_ok),
			 ztest_unit_test(test_init_error),
			 ztest_unit_test(test_ble_connection),
			 ztest_unit_test(test_ble_disconnection),
			 ztest_unit_test(test_ble_data_event),
			 ztest_unit_test(test_ble_ctrl_event));

	ztest_run_test_suite(test_bluetooth);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_ble_data_event(eh)) {
		printk("BLE data event received!\n");
		const struct ble_data_event *event = cast_ble_data_event(eh);
		uint8_t expected_data[2] = { 0x01, 0x02 };
		zassert_mem_equal(event->buf, expected_data, 2,
				  "Error buffer not equal");
		k_sem_give(&test_status);
		return false;
	}

	if (is_ble_conn_event(eh)) {
		printk("BLE conn event received\n");
		const struct ble_conn_event *event = cast_ble_conn_event(eh);
		if (event->conn_state == BLE_STATE_CONNECTED) {
			printk("BLE CONNECTED\n");
			zassert_equal(event->conn_state, BLE_STATE_CONNECTED,
				      "Error in BLE conn event");
		} else {
			printk("BLE DISCONNECTED\n");
			zassert_equal(event->conn_state, BLE_STATE_DISCONNECTED,
				      "Error in BLE conn event");
		}
		k_sem_give(&test_status);
		return false;
	}

	if (is_ble_ctrl_event(eh)) {
		printk("BLE ctrl event received!\n");
		const struct ble_ctrl_event *event = cast_ble_ctrl_event(eh);
		switch (event->cmd) {
		case BLE_CTRL_ENABLE:
		case BLE_CTRL_DISABLE:
			break;
		case BLE_CTRL_BATTERY_UPDATE:
			zassert_equal(event->param.battery, 65,
				      "Error not correct value");
			break;
		case BLE_CTRL_ERROR_FLAG_UPDATE:
			zassert_equal(event->param.error_flags, 0,
				      "Error not correct value");
			break;
		case BLE_CTRL_COLLAR_MODE_UPDATE:
			zassert_equal(event->param.collar_mode, 1,
				      "Error not correct value");
			break;
		case BLE_CTRL_COLLAR_STATUS_UPDATE:
			zassert_equal(event->param.collar_status, 5,
				      "Error not correct value");
			break;
		case BLE_CTRL_FENCE_STATUS_UPDATE:
			zassert_equal(event->param.fence_status, 1,
				      "Error not correct value");
			break;
		case BLE_CTRL_PASTURE_UPDATE:
			zassert_equal(event->param.valid_pasture, 1,
				      "Error not correct value");
			break;
		case BLE_CTRL_FENCE_DEF_VER_UPDATE:
			zassert_equal(event->param.fence_def_ver, 0x00A1,
				      "Error not correct value");
			break;
		}

		k_sem_give(&test_status);
		return false;
	}

	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, ble_conn_event);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);