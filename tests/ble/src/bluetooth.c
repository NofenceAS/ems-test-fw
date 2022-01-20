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
#include "bt.h"

static K_SEM_DEFINE(ble_connected_sem, 0, 1);
static K_SEM_DEFINE(ble_disconnected_sem, 0, 1);
static K_SEM_DEFINE(ble_data_sem, 0, 1);
static K_SEM_DEFINE(ble_battery_sem, 0, 1);
static K_SEM_DEFINE(ble_error_flag_sem, 0, 1);
static K_SEM_DEFINE(ble_collar_mode_sem, 0, 1);
static K_SEM_DEFINE(ble_collar_status_sem, 0, 1);
static K_SEM_DEFINE(ble_fence_status_sem, 0, 1);
static K_SEM_DEFINE(ble_pasture_status_sem, 0, 1);
static K_SEM_DEFINE(ble_fence_def_ver_sem, 0, 1);

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
	int err = k_sem_take(&ble_connected_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

void test_ble_disconnection(void)
{
	struct ble_conn_event *event = new_ble_conn_event();
	event->conn_state = BLE_STATE_DISCONNECTED;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_disconnected_sem, K_SECONDS(10));
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
	int err = k_sem_take(&ble_data_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

/* Submit a battery change */
void test_ble_ctrl_event_battery(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_battery_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_BATTERY_UPDATE;
	event->param.battery = 65;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_battery_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_battery_adv_data_correct(event->param.battery),
		     "Adv data not correct");
}

/* Submit error flag change */
void test_ble_ctrl_event_error_flag(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_error_flag_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_ERROR_FLAG_UPDATE;
	event->param.error_flags = 2;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_error_flag_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_error_flag_adv_data_correct(
			     event->param.error_flags),
		     "Adv data not correct");
}

/* Submit collar mode change */
void test_ble_ctrl_event_collar_mode(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_collar_mode_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_COLLAR_MODE_UPDATE;
	event->param.collar_mode = 2;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_collar_mode_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_collar_mode_adv_data_correct(
			     event->param.collar_mode),
		     "Adv data not correct");
}

/* Submit collar status change */
void test_ble_ctrl_event_collar_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_collar_status_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_COLLAR_STATUS_UPDATE;
	event->param.collar_status = 2;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_collar_status_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_collar_status_adv_data_correct(
			     event->param.collar_status),
		     "Adv data not correct");
}

/* Submit Fence status change */
void test_ble_ctrl_event_fence_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_status_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_FENCE_STATUS_UPDATE;
	event->param.fence_status = 2;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_fence_status_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_fence_status_adv_data_correct(
			     event->param.fence_status),
		     "Adv data not correct");
}

/* Submit Pasture status change */
void test_ble_ctrl_event_pasture_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_pasture_status_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_PASTURE_UPDATE;
	event->param.valid_pasture = 2;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_pasture_status_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_pasture_status_adv_data_correct(
			     event->param.valid_pasture),
		     "Adv data not correct");
}

/* Submit Fence Def ver change */
void test_ble_ctrl_event_fence_def_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_def_ver_sem, K_NO_WAIT);
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct ble_ctrl_event *event = new_ble_ctrl_event();
	event->cmd = BLE_CTRL_FENCE_DEF_VER_UPDATE;
	event->param.fence_def_ver = 100;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_fence_def_ver_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");

	/* Semaphore given */
	zassert_true(bt_mock_is_fence_def_ver_adv_data_correct(
			     event->param.fence_def_ver),
		     "Adv data not correct");
}

/* Test case main entry */
void test_main(void)
{
	ztest_test_suite(test_bluetooth, ztest_unit_test(test_init_ok),
			 ztest_unit_test(test_init_error),
			 ztest_unit_test(test_ble_connection),
			 ztest_unit_test(test_ble_disconnection),
			 ztest_unit_test(test_ble_data_event),
			 ztest_unit_test(test_ble_ctrl_event_battery),
			 ztest_unit_test(test_ble_ctrl_event_error_flag),
			 ztest_unit_test(test_ble_ctrl_event_collar_mode),
			 ztest_unit_test(test_ble_ctrl_event_collar_status),
			 ztest_unit_test(test_ble_ctrl_event_fence_status),
			 ztest_unit_test(test_ble_ctrl_event_pasture_status),
			 ztest_unit_test(test_ble_ctrl_event_fence_def_status));

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
		k_sem_give(&ble_data_sem);
		return false;
	}

	if (is_ble_conn_event(eh)) {
		printk("BLE conn event received\n");
		const struct ble_conn_event *event = cast_ble_conn_event(eh);
		if (event->conn_state == BLE_STATE_CONNECTED) {
			printk("BLE CONNECTED\n");
			zassert_equal(event->conn_state, BLE_STATE_CONNECTED,
				      "Error in BLE conn event");
			k_sem_give(&ble_connected_sem);
		} else {
			printk("BLE DISCONNECTED\n");
			zassert_equal(event->conn_state, BLE_STATE_DISCONNECTED,
				      "Error in BLE conn event");
			k_sem_give(&ble_disconnected_sem);
		}
		return false;
	}

	if (is_ble_ctrl_event(eh)) {
		printk("BLE ctrl event received!\n");
		const struct ble_ctrl_event *event = cast_ble_ctrl_event(eh);
		switch (event->cmd) {
		case BLE_CTRL_ADV_ENABLE:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_ADV_DISABLE:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_BATTERY_UPDATE:
			k_sem_give(&ble_battery_sem);
			break;
		case BLE_CTRL_ERROR_FLAG_UPDATE:
			k_sem_give(&ble_error_flag_sem);
			break;
		case BLE_CTRL_COLLAR_MODE_UPDATE:
			k_sem_give(&ble_collar_mode_sem);
			break;
		case BLE_CTRL_COLLAR_STATUS_UPDATE:
			k_sem_give(&ble_collar_status_sem);
			break;
		case BLE_CTRL_FENCE_STATUS_UPDATE:
			k_sem_give(&ble_fence_status_sem);
			break;
		case BLE_CTRL_PASTURE_UPDATE:
			k_sem_give(&ble_pasture_status_sem);
			break;
		case BLE_CTRL_FENCE_DEF_VER_UPDATE:
			k_sem_give(&ble_fence_def_ver_sem);
			break;
		}
		return false;
	}

	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, ble_conn_event);
EVENT_SUBSCRIBE_FINAL(test_main, ble_ctrl_event);