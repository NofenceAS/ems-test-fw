/*
 * Copyright (c) 2022 Nofence AS
 */

#include <zephyr.h>

#include <ztest.h>
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "ble_conn_event.h"
#include "ble_beacon_event.h"

#include "ble_controller.h"
#include "event_manager.h"
#include "bt.h"
#include "beacon_processor.h"
#include "eeprom.h"

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
static K_SEM_DEFINE(beacon_hysteresis_sem, 0, 1);
static K_SEM_DEFINE(beacon_out_of_range, 0, 1);

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
	ztest_returns_value(eep_read_serial,0);
	ztest_returns_value(bt_enable, 0);
	ztest_returns_value(bt_nus_init, 0);
	ztest_returns_value(bt_le_adv_start, 0);
	ztest_returns_value(bt_le_scan_start, 0);

	zassert_equal(event_manager_init(), 0,
		      "Error when initializing event manager");

	zassert_equal(ble_module_init(), 0,
		      "Error when initializing ble module");
}

/* Checik if ble_module_init fails sucessfully */
void test_init_error(void)
{
	ztest_returns_value(eep_read_serial,0);
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

void test_ble_beacon_scanner(void)
{
	const uint32_t now_1 = k_uptime_get_32();
	adv_data_t adv_data_1;
	adv_data_1.rssi = 197; // signed
	bt_addr_t address_1;
	address_1.val[0] = 0x46;
	address_1.val[1] = 0x01;
	address_1.val[2] = 0x00;
	address_1.val[3] = 0x30;
	address_1.val[4] = 0x02;
	address_1.val[5] = 0xc2;
	bt_addr_le_t addr_1;
	addr_1.type = 1;
	memcpy(addr_1.a.val, address_1.val, sizeof(bt_addr_t));
	int measured_rssi_1 = -60; // corresponds to 1 meter
	int range1 = beacon_process_event(now_1, &addr_1, measured_rssi_1,
					  &adv_data_1);
	zassert_equal(range1, 1, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	const uint32_t now_2 = k_uptime_get_32();
	adv_data_t adv_data_2;
	adv_data_2.rssi = 197; // signed
	bt_addr_t address_2;
	address_2.val[0] = 0x46;
	address_2.val[1] = 0x01;
	address_2.val[2] = 0x00;
	address_2.val[3] = 0x30;
	address_2.val[4] = 0x02;
	address_2.val[5] = 0xb3;
	bt_addr_le_t addr_2;
	addr_2.type = 1;
	memcpy(addr_2.a.val, address_2.val, sizeof(bt_addr_t));
	int measured_rssi_2 = -76; // corresponds to 6 meter
	int range2 = beacon_process_event(now_2, &addr_2, measured_rssi_2,
					  &adv_data_2);
	zassert_equal(range2, 1, "Shortest distance calculation is wrong");

	int err = k_sem_take(&beacon_hysteresis_sem, K_SECONDS(5));
	zassert_equal(err, 0, "Test beacon event execution hanged.");
}

void test_ble_beacon_out_of_range(void)
{
	/* Clear the beacon list */
	init_beacon_list();

	const uint32_t now = k_uptime_get_32();
	adv_data_t adv_data;
	adv_data.rssi = 197; // signed
	bt_addr_t address;
	address.val[0] = 0x46;
	address.val[1] = 0x01;
	address.val[2] = 0x00;
	address.val[3] = 0x30;
	address.val[4] = 0x02;
	address.val[5] = 0xb3;
	bt_addr_le_t addr;
	addr.type = 1;
	memcpy(addr.a.val, address.val, sizeof(bt_addr_t));
	int measured_rssi = -120; // corresponds to 6 meter
	int range = beacon_process_event(now, &addr, measured_rssi, &adv_data);

	zassert_equal(range, -EIO, "We received a wrong return code");

	int err = k_sem_take(&beacon_out_of_range, K_SECONDS(10));
	zassert_equal(err, 0, "Test beacon out of range execution hanged.");
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
			 ztest_unit_test(test_ble_beacon_scanner),
			 ztest_unit_test(test_ble_beacon_out_of_range),
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
		case BLE_CTRL_SCAN_START:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_SCAN_STOP:
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
	if (is_ble_beacon_event(eh)) {
		const struct ble_beacon_event *event =
			cast_ble_beacon_event(eh);
		switch (event->status) {
		case BEACON_STATUS_REGION_NEAR:
			printk("Beacon status region near detected\n");
			k_sem_give(&beacon_hysteresis_sem);
			break;
		case BEACON_STATUS_REGION_FAR:
			printk("Beacon status region far\n");
			break;
		case BEACON_STATUS_NOT_FOUND:
			printk("Beacon status not found\n");
			break;
		case BEACON_STATUS_OUT_OF_RANGE:
			printk("Beacon status out for range\n");
			k_sem_give(&beacon_out_of_range);
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
EVENT_SUBSCRIBE(test_main, ble_beacon_event);
EVENT_SUBSCRIBE_FINAL(test_main, ble_ctrl_event);