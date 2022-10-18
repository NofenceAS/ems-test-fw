/*
 * Copyright (c) 2022 Nofence AS
 */

#include <zephyr.h>

#include <ztest.h>
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "ble_conn_event.h"
#include "ble_beacon_event.h"

#include "messaging_module_events.h"
#include "collar_protocol.h"
#include "ble_controller.h"
#include "event_manager.h"
#include "bt.h"
#include "beacon_processor.h"
#include "nf_settings.h"

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
static K_SEM_DEFINE(beacon_near_sem, 0, 1);
static K_SEM_DEFINE(beacon_far_sem, 0, 1);
static K_SEM_DEFINE(beacon_not_found, 0, 1);

/** @brief : Variable for curent shortest beacon distance **/
static uint8_t m_shortest_dist;

/** @brief : Used to save the last seen beacon distance **/
static uint8_t last_distance = UINT8_MAX;
void evaluate_beacon(void);

typedef enum {
	CROSS_UNDEFINED = 0,
	CROSS_LOW_FROM_BELOW,
	CROSS_HIGH_FROM_ABOVE
} cross_type_t;

/** @brief : Used for hysteresis calculation **/
static cross_type_t cross_type = CROSS_UNDEFINED;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

/* TODO: mock away the scan callback function to be able to test more
 * realistic situations with multiple beacons with random distances to the
 * collar. */

void test_init_ok(void)
{
	ztest_returns_value(eep_uint32_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(bt_enable, 0);
	ztest_returns_value(bt_nus_init, 0);
	ztest_returns_value(bt_set_name, 0);
	ztest_returns_value(bt_le_adv_start, 0);
	ztest_returns_value(bt_le_scan_start, 0);

	zassert_equal(event_manager_init(), 0, "Error when initializing event manager");

	zassert_equal(ble_module_init(), 0, "Error when initializing ble module");
}

/* Checik if ble_module_init fails sucessfully */
void test_init_error(void)
{
	ztest_returns_value(eep_uint32_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(bt_enable, -2);
	zassert_equal(event_manager_init(), 0, "Error when initializing event manager");

	zassert_equal(ble_module_init(), -2, "Error when initializing ble module");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  Simple test with two unique beacons, one on 1 meter and another on 12 meter.
 */
void test_ble_beacon_scanner(void)
{
	k_sem_reset(&beacon_near_sem);
	init_beacon_list();
	adv_data_t adv_data_1;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_1.rssi = 197;
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
	const uint32_t now_1 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(now_1, &addr_1, measured_rssi_1, &adv_data_1);

	zassert_equal(m_shortest_dist, 1, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	adv_data_t adv_data_2;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_2.rssi = 197;
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
	int measured_rssi_2 = -76; // corresponds to 12 meter
	const uint32_t now_2 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(now_2, &addr_2, measured_rssi_2, &adv_data_2);
	// Since the list contain another beacon with distance 1, we expect this to be smaller and returned
	zassert_equal(m_shortest_dist, 1, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_near_sem, K_SECONDS(5)), 0, "");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  We test with one beacon from high to low distances, and check that the right event is triggered.
 */
void test_calculation_beacon_scanner(void)
{
	/* Clear the beacon list */
	init_beacon_list();
	k_sem_reset(&beacon_near_sem);
	adv_data_t adv_data_1;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_1.rssi = 197;
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
	int measured_rssi = -83; // corresponds to 30 meter
	uint32_t time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 30, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	measured_rssi = -81; // corresponds to 23 meter
	time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 23, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	measured_rssi = -77; // corresponds to 14 meter
	time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 14, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	measured_rssi = -68; // corresponds to 4 meter
	time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 4, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	measured_rssi = -64; // corresponds to 2 meter
	time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 2, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));

	measured_rssi = -63; // corresponds to 1 meter
	time_now = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now, &addr_1, measured_rssi, &adv_data_1);
	zassert_equal(m_shortest_dist, 1, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_near_sem, K_SECONDS(5)), 0, "");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  We test with beacons from low to high distances, and check that the right event is triggered.
 */
void test_beacon_shortest_dist(void)
{
	/* Clear the beacon list */
	init_beacon_list();
	k_sem_reset(&beacon_near_sem);
	adv_data_t adv_data_1;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_1.rssi = 197;
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
	int measured_rssi_1 = -68; // corresponds to 4 meter
	uint32_t time_now_1 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_1, &addr_1, measured_rssi_1, &adv_data_1);
	printk("Shortest range: %d\n", m_shortest_dist);
	zassert_equal(m_shortest_dist, 4, "Shortest distance calculation is wrong");
	k_sleep(K_SECONDS(1));
	adv_data_t adv_data_2;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_2.rssi = 197;
	bt_addr_t address_2;
	address_2.val[0] = 0x48;
	address_2.val[1] = 0x01;
	address_2.val[2] = 0x00;
	address_2.val[3] = 0x30;
	address_2.val[4] = 0x02;
	address_2.val[5] = 0xc2;
	bt_addr_le_t addr_2;
	addr_2.type = 1;
	memcpy(addr_2.a.val, address_2.val, sizeof(bt_addr_t));
	int measured_rssi_2 = -83; // corresponds to 30 meter
	uint32_t time_now_2 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_2, &addr_2, measured_rssi_2, &adv_data_2);
	printk("Shortest range: %d\n", m_shortest_dist);
	/* We expect the distance to be 4, since this is the shortest registred in the list */
	zassert_equal(m_shortest_dist, 4, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_near_sem, K_SECONDS(5)), 0, "");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  Test that beacons with timestamp older than CONFIG_BEACON_MAX_MEASUREMENT_AGE is ignored
 */
void test_beacon_timeout_dist(void)
{
	/* Clear the beacon list */
	init_beacon_list();
	k_sem_reset(&beacon_far_sem);
	adv_data_t adv_data_1;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_1.rssi = 197;
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
	int measured_rssi_1 = -68; // corresponds to 4 meter
	uint32_t time_now_1 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_1, &addr_1, measured_rssi_1, &adv_data_1);
	printk("Shortest range: %d\n", m_shortest_dist);
	zassert_equal(m_shortest_dist, 4, "Shortest distance calculation is wrong");

	/* Wait for a while */
	k_sleep(K_SECONDS(CONFIG_BEACON_MAX_MEASUREMENT_AGE));
	adv_data_t adv_data_2;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_2.rssi = 197;
	bt_addr_t address_2;
	address_2.val[0] = 0x48;
	address_2.val[1] = 0x01;
	address_2.val[2] = 0x00;
	address_2.val[3] = 0x30;
	address_2.val[4] = 0x02;
	address_2.val[5] = 0xc2;
	bt_addr_le_t addr_2;
	addr_2.type = 1;
	memcpy(addr_2.a.val, address_2.val, sizeof(bt_addr_t));
	int measured_rssi_2 = -83; // corresponds to 30 meter
	uint32_t time_now_2 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_2, &addr_2, measured_rssi_2, &adv_data_2);
	printk("Shortest range: %d\n", m_shortest_dist);
	/* We expect the distance to be 30, since the beacon with shorter measurement is ignored */
	zassert_equal(m_shortest_dist, 30, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_far_sem, K_SECONDS(5)), 0, "");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  Test what happens when list is filled up
 */
void test_beacon_full_list_dist(void)
{
	/* Clear the beacon list */
	init_beacon_list();
	k_sem_reset(&beacon_near_sem);
	k_sem_reset(&beacon_far_sem);
	adv_data_t adv_data_1;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_1.rssi = 197;
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
	int measured_rssi_1 = -68; // corresponds to 4 meter
	uint32_t time_now_1 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_1, &addr_1, measured_rssi_1, &adv_data_1);
	printk("Shortest range: %d\n", m_shortest_dist);
	zassert_equal(m_shortest_dist, 4, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_near_sem, K_SECONDS(5)), 0, "");

	/* Wait for a while */
	k_sleep(K_SECONDS(CONFIG_BEACON_MAX_MEASUREMENT_AGE));

	adv_data_t adv_data_2;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_2.rssi = 197;
	bt_addr_t address_2;
	address_2.val[0] = 0x48;
	address_2.val[1] = 0x01;
	address_2.val[2] = 0x00;
	address_2.val[3] = 0x30;
	address_2.val[4] = 0x02;
	address_2.val[5] = 0xc2;
	bt_addr_le_t addr_2;
	addr_2.type = 1;
	memcpy(addr_2.a.val, address_2.val, sizeof(bt_addr_t));
	int measured_rssi_2 = -81; // corresponds to 23 meter
	uint32_t time_now_2 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_2, &addr_2, measured_rssi_2, &adv_data_2);
	printk("Shortest range: %d\n", m_shortest_dist);
	/* We expect the distance to be 23, since the beacon with shorter measurement is ignored */
	zassert_equal(m_shortest_dist, 23, "Shortest distance calculation is wrong");

	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_far_sem, K_SECONDS(5)), 0, "");

	adv_data_t adv_data_3;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_3.rssi = 197;
	bt_addr_t address_3;
	address_3.val[0] = 0x49;
	address_3.val[1] = 0x01;
	address_3.val[2] = 0x00;
	address_3.val[3] = 0x30;
	address_3.val[4] = 0x02;
	address_3.val[5] = 0xc2;
	bt_addr_le_t addr_3;
	addr_3.type = 1;
	memcpy(addr_3.a.val, address_3.val, sizeof(bt_addr_t));
	int measured_rssi_3 = -77; // corresponds to 14 meter
	uint32_t time_now_3 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_3, &addr_3, measured_rssi_3, &adv_data_3);
	printk("Shortest range: %d\n", m_shortest_dist);
	zassert_equal(m_shortest_dist, 14, "Shortest distance calculation is wrong");

	k_sleep(K_SECONDS(1));
	adv_data_t adv_data_4;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_4.rssi = 197;
	bt_addr_t address_4;
	address_4.val[0] = 0x50;
	address_4.val[1] = 0x01;
	address_4.val[2] = 0x00;
	address_4.val[3] = 0x30;
	address_4.val[4] = 0x02;
	address_4.val[5] = 0xc2;
	bt_addr_le_t addr_4;
	addr_4.type = 1;
	memcpy(addr_4.a.val, address_4.val, sizeof(bt_addr_t));
	int measured_rssi_4 = -83; // corresponds to 30 meter
	uint32_t time_now_4 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_4, &addr_4, measured_rssi_4, &adv_data_4);
	printk("Shortest range: %d\n", m_shortest_dist);
	/* We expect the distance to be 14, since the beacon with shorter measurement is ignored */
	zassert_equal(m_shortest_dist, 14, "Shortest distance calculation is wrong");

	adv_data_t adv_data_5;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data_5.rssi = 197;
	bt_addr_t address_5;
	address_5.val[0] = 0x51;
	address_5.val[1] = 0x01;
	address_5.val[2] = 0x00;
	address_5.val[3] = 0x30;
	address_5.val[4] = 0x02;
	address_5.val[5] = 0xc2;
	bt_addr_le_t addr_5;
	addr_5.type = 1;
	memcpy(addr_5.a.val, address_5.val, sizeof(bt_addr_t));
	int measured_rssi_5 = -60; // corresponds to 1 meter
	uint32_t time_now_5 = k_uptime_get_32();
	m_shortest_dist =
		beacon_process_event(time_now_5, &addr_5, measured_rssi_5, &adv_data_5);

	printk("Shortest range: %d\n", m_shortest_dist);
	zassert_equal(m_shortest_dist, 1, "Shortest distance calculation is wrong");

	k_sem_reset(&beacon_near_sem);
	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_near_sem, K_SECONDS(5)), 0, "");
}

/**
 * @brief This test check the algorithm for calculating the distance based on RSSI.
 * 	  The RSSI values and their corresponding distance is benchmarked with legacy collar.
 * 	  We test for beacon measurement outside valid range
 */
void test_ble_beacon_out_of_range(void)
{
	/* Clear the beacon list */
	init_beacon_list();

	adv_data_t adv_data;
	/* Beacon RSSI advertised. The value is fixed and fetched from adv payload.
	 * Expect the value unsigned. 197 corresponds to -59 in transmitted RSSI.
	 */
	adv_data.rssi = 197;
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
	int measured_rssi = -88; // corresponds to 54 meter
	uint32_t time_now = k_uptime_get_32();
	m_shortest_dist = beacon_process_event(time_now, &addr, measured_rssi, &adv_data);
	/* Since the distance is greater than 50 m we return UINT8_MAX */
	zassert_equal(m_shortest_dist, UINT8_MAX, "We received a wrong return code");

	k_sem_reset(&beacon_not_found);
	/* Evaluate BEACON */
	evaluate_beacon();
	zassert_equal(k_sem_take(&beacon_not_found, K_SECONDS(5)), 0, "");
}

/**
 * @brief Test the Bluetooth connected event
 */
void test_ble_connection(void)
{
	struct ble_conn_event *event = new_ble_conn_event();
	event->conn_state = BLE_STATE_CONNECTED;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_connected_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

/**
 * @brief Test the Bluetooth disconnected event
 */
void test_ble_disconnection(void)
{
	struct ble_conn_event *event = new_ble_conn_event();
	event->conn_state = BLE_STATE_DISCONNECTED;
	EVENT_SUBMIT(event);

	/* Wait for it and see. */
	int err = k_sem_take(&ble_disconnected_sem, K_SECONDS(10));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

/**
 * @brief Test the Bluetooth data event
 */
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

/**
 * @brief Test update of battery. Submit ctrl event and get expected result
 */
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

/**
 * @brief Test ble error event. Submit ctrl event and get expected result
 */
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
	zassert_true(bt_mock_is_error_flag_adv_data_correct(event->param.error_flags),
		     "Adv data not correct");
}

/**
 * @brief Submit collar mode change, and verify ble advertisement update
 */
void test_ble_ctrl_event_collar_mode(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_collar_mode_sem, K_NO_WAIT);

	/* Update collar mode */
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct update_collar_mode *evt = new_update_collar_mode();
	evt->collar_mode = Mode_Fence;
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_collar_mode_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged");

	/* Verify advertisement data */
	zassert_true(bt_mock_is_collar_mode_adv_data_correct(Mode_Fence),
		     "Adv collar mode not correct");
}

/**
 * @brief Submit collar status change, and verify ble advertisement update
 */
void test_ble_ctrl_event_collar_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_collar_status_sem, K_NO_WAIT);

	/* Update collar status */
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct update_collar_status *evt = new_update_collar_status();
	evt->collar_status = CollarStatus_Stuck;
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_collar_status_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged.");

	/* Verify advertisement data */
	zassert_true(bt_mock_is_collar_status_adv_data_correct(CollarStatus_Stuck),
		     "Adv collar status not correct");
}

/**
 * @brief Submit fence status change, and verify ble advertisement update
 */
void test_ble_ctrl_event_fence_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_status_sem, K_NO_WAIT);

	/* Update fence status */
	ztest_returns_value(bt_le_adv_update_data, 0);
	struct update_fence_status *evt = new_update_fence_status();
	evt->fence_status = FenceStatus_NotStarted;
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_fence_status_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged.");

	/* Verify advertisement data */
	zassert_true(bt_mock_is_fence_status_adv_data_correct(FenceStatus_NotStarted),
		     "Adv fence status not correct");
}

/**
 * @brief Submit fence version change, and verify ble advertisement update
 */
void test_ble_ctrl_event_fence_def_status(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_def_ver_sem, K_NO_WAIT);

	/* Update fence def version */
	ztest_returns_value(bt_le_adv_update_data, 0); //fence_def_ver_update()
	ztest_returns_value(bt_le_adv_update_data, 0); //pasture_update()
	struct update_fence_version *evt = new_update_fence_version();
	evt->fence_version = 100; //Fence def version no.
	evt->total_fences = 3; //Fence def Gemoetry
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_fence_def_ver_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged.");

	/* Verify advertisement data (Fence def version) */
	zassert_true(bt_mock_is_fence_def_ver_adv_data_correct(100),
		     "Adv fence def version not correct");

	/* Verify advertisement data (valid pasture) */
	zassert_true(bt_mock_is_pasture_status_adv_data_correct(1 /* valid */),
		     "Adv valid pasture not correct");
}

/**
 * @brief Submit invalid fence defenition version, and verify ble advertisement update
 */
void test_ble_ctrl_event_fence_def_status_invalid(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_def_ver_sem, K_NO_WAIT);

	/* Update fence def version */
	ztest_returns_value(bt_le_adv_update_data, 0); //fence_def_ver_update()
	ztest_returns_value(bt_le_adv_update_data, 0); //pasture_update()
	struct update_fence_version *evt = new_update_fence_version();
	evt->fence_version = 0; //Fence def version no. (0 = No fence)
	evt->total_fences = 3; //Fence def Gemoetry
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_fence_def_ver_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged.");

	/* Verify advertisement data (Fence def version) */
	zassert_true(bt_mock_is_fence_def_ver_adv_data_correct(0),
		     "Adv fence def version not correct");

	/* Verify advertisement data (valid pasture) */
	zassert_true(bt_mock_is_pasture_status_adv_data_correct(0 /* invalid */),
		     "Adv valid pasture not correct");
}

void test_ble_ctrl_event_fence_def_status_no_pasture(void)
{
	/* Safety check, is sempahore already given? */
	k_sem_take(&ble_fence_def_ver_sem, K_NO_WAIT);

	/* Update fence def version */
	ztest_returns_value(bt_le_adv_update_data, 0); //fence_def_ver_update()
	ztest_returns_value(bt_le_adv_update_data, 0); //pasture_update()
	struct update_fence_version *evt = new_update_fence_version();
	evt->fence_version = 100; //Fence def version no.
	evt->total_fences = 0; //Fence def Gemoetry (0 = No pasture)
	EVENT_SUBMIT(evt);

	/* Wait for event to be processed */
	zassert_equal(k_sem_take(&ble_fence_def_ver_sem, K_SECONDS(10)), 0,
		      "Test status event execution hanged.");

	/* Verify advertisement data (Fence def version) */
	zassert_true(bt_mock_is_fence_def_ver_adv_data_correct(100),
		     "Adv fence def version not correct");

	/* Verify advertisement data (valid pasture) */
	zassert_true(bt_mock_is_pasture_status_adv_data_correct(0 /* invalid */),
		     "Adv valid pasture not correct");
}

/* Test case main entry */
void test_main(void)
{
	ztest_test_suite(
		test_bluetooth, ztest_unit_test(test_init_ok),
		ztest_unit_test(test_init_error),
		ztest_unit_test(test_ble_beacon_scanner),
		ztest_unit_test(test_calculation_beacon_scanner),
		ztest_unit_test(test_beacon_shortest_dist),
		ztest_unit_test(test_beacon_timeout_dist),
		ztest_unit_test(test_beacon_full_list_dist),
		ztest_unit_test(test_ble_beacon_out_of_range),
		ztest_unit_test(test_ble_connection),
		ztest_unit_test(test_ble_disconnection),
		ztest_unit_test(test_ble_data_event),
		ztest_unit_test(test_ble_ctrl_event_battery),
		ztest_unit_test(test_ble_ctrl_event_error_flag),
		ztest_unit_test(test_ble_ctrl_event_collar_mode),
		ztest_unit_test(test_ble_ctrl_event_collar_status),
		ztest_unit_test(test_ble_ctrl_event_fence_status),
		ztest_unit_test(test_ble_ctrl_event_fence_def_status),
		ztest_unit_test(test_ble_ctrl_event_fence_def_status_invalid),
		ztest_unit_test(test_ble_ctrl_event_fence_def_status_no_pasture));
	ztest_run_test_suite(test_bluetooth);
}

void evaluate_beacon(void)
{
	struct ble_beacon_event *event = new_ble_beacon_event();
	if (m_shortest_dist == UINT8_MAX) {
		cross_type = CROSS_UNDEFINED;
		event->status = BEACON_STATUS_NOT_FOUND;
		printk("1: Status: BEACON_STATUS_NOT_FOUND, Type: CROSS_UNDEFINED\n");
		goto end;

	} else if (m_shortest_dist > CONFIG_BEACON_HIGH_LIMIT) {
		cross_type = CROSS_UNDEFINED;
		event->status = BEACON_STATUS_REGION_FAR;
		printk("2: Status: BEACON_STATUS_REGION_FAR, Type: CROSS_UNDEFINED\n");
		goto end;

	} else if (m_shortest_dist <= CONFIG_BEACON_LOW_LIMIT) {
		cross_type = CROSS_UNDEFINED;
		event->status = BEACON_STATUS_REGION_NEAR;
		printk("3: Status: BEACON_STATUS_REGION_NEAR, Type: CROSS_UNDEFINED\n");
		goto end;

	} else if (last_distance <= CONFIG_BEACON_LOW_LIMIT &&
		   m_shortest_dist > CONFIG_BEACON_LOW_LIMIT) {
		cross_type = CROSS_LOW_FROM_BELOW;
		event->status = BEACON_STATUS_REGION_NEAR;
		printk("4: Status: BEACON_STATUS_REGION_NEAR, Type: CROSS_LOW_FROM_BELOW\n");
		goto end;

	} else if (last_distance > CONFIG_BEACON_HIGH_LIMIT &&
		   m_shortest_dist <= CONFIG_BEACON_HIGH_LIMIT) {
		cross_type = CROSS_HIGH_FROM_ABOVE;
		event->status = BEACON_STATUS_REGION_FAR;
		printk("5: Status: BEACON_STATUS_REGION_FAR, Type: CROSS_HIGH_FROM_ABOVE\n");
		goto end;

	} else {
		if (cross_type == CROSS_LOW_FROM_BELOW) {
			event->status = BEACON_STATUS_REGION_NEAR;
			printk("6: Status: BEACON_STATUS_REGION_NEAR, Type: CROSS_LOW_FROM_BELOW\n");
			goto end;
		} else if (cross_type == CROSS_HIGH_FROM_ABOVE) {
			event->status = BEACON_STATUS_REGION_FAR;
			printk("7: Status: BEACON_STATUS_REGION_FAR, Type: CROSS_HIGH_FROM_ABOVE\n");
			goto end;
		}
	}
end:
	EVENT_SUBMIT(event);
	last_distance = m_shortest_dist;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_ble_data_event(eh)) {
		printk("BLE data event received!\n");
		const struct ble_data_event *event = cast_ble_data_event(eh);
		uint8_t expected_data[2] = { 0x01, 0x02 };
		zassert_mem_equal(event->buf, expected_data, 2, "Error buffer not equal");
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
		const struct ble_ctrl_event *event = cast_ble_ctrl_event(eh);
		printk("BLE ctrl event received! %d\n", event->cmd);
		switch (event->cmd) {
		case BLE_CTRL_ADV_ENABLE:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_ADV_DISABLE:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_SCAN_START:
			// TODO: A test of this should be added later
			printk("Start scanning for beacon\n");
			break;
		case BLE_CTRL_SCAN_STOP:
			// TODO: A test of this should be added later
			printk("Scanning stopped\n");
			break;
		case BLE_CTRL_DISCONNECT_PEER:
			// TODO: A test of this should be added later
			break;
		case BLE_CTRL_BATTERY_UPDATE:
			k_sem_give(&ble_battery_sem);
			break;
		case BLE_CTRL_ERROR_FLAG_UPDATE:
			k_sem_give(&ble_error_flag_sem);
			break;
		case BLE_CTRL_COLLAR_MODE_UPDATE:
			/* Unused */
		case BLE_CTRL_COLLAR_STATUS_UPDATE:
			/* Unused */
		case BLE_CTRL_FENCE_STATUS_UPDATE:
			/* Unused */
		case BLE_CTRL_PASTURE_UPDATE:
			/* Unused */
		case BLE_CTRL_FENCE_DEF_VER_UPDATE:
			/* Unused */
			break;
		}
		return false;
	}

	if (is_ble_beacon_event(eh)) {
		const struct ble_beacon_event *event = cast_ble_beacon_event(eh);
		switch (event->status) {
		case BEACON_STATUS_REGION_NEAR:
			printk("Beacon status region near detected\n");
			k_sem_give(&beacon_near_sem);
			break;
		case BEACON_STATUS_REGION_FAR:
			printk("Beacon status region far\n");
			k_sem_give(&beacon_far_sem);
			break;
		case BEACON_STATUS_NOT_FOUND:
			printk("Beacon status not found\n");
			k_sem_give(&beacon_not_found);
			break;
		case BEACON_STATUS_OFF:
			printk("Beacon scanner is turned off\n");
			break;
		}
		return false;
	}

	if (is_update_collar_mode(eh)) {
		k_sem_give(&ble_collar_mode_sem);
		return false;
	}

	if (is_update_collar_status(eh)) {
		k_sem_give(&ble_collar_status_sem);
		return false;
	}

	if (is_update_fence_status(eh)) {
		k_sem_give(&ble_fence_status_sem);
		return false;
	}

	if (is_update_fence_version(eh)) {
		k_sem_give(&ble_fence_def_ver_sem);
		return false;
	}

	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, ble_conn_event);
EVENT_SUBSCRIBE(test_main, ble_beacon_event);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);
EVENT_SUBSCRIBE_FINAL(test_main, update_collar_mode);
EVENT_SUBSCRIBE_FINAL(test_main, update_collar_status);
EVENT_SUBSCRIBE_FINAL(test_main, update_fence_status);
EVENT_SUBSCRIBE_FINAL(test_main, update_fence_version);
