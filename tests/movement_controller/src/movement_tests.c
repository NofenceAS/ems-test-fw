/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include <zephyr.h>
#include <drivers/sensor.h>
#include <stdlib.h>
#include "movement_events.h"
#include "movement_controller.h"
#include "event_manager.h"

K_SEM_DEFINE(cur_activity_sem, 0, 1);

test_id_t curr_test = TEST_ACTIVITY_LOW;
uint8_t row = 0;
static acc_activity_t cur_activity = ACTIVITY_NO;
static const struct device *sensor;

/** @todo, these functions are actually private */
void process_acc_data(raw_acc_data_t *acc);
void fetch_and_display(const struct device *sensor);

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
	ztest_returns_value(sensor_attr_set, 0);
	ztest_returns_value(sensor_attr_set, 0);
	//ztest_returns_value(sensor_trigger_set, 0);

	/* For the sigma values. */
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);

	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	zassert_false(init_movement_controller(),
		      "Error when initializing movement controller");
}

void test_activity_no(void)
{
	_movement_controller_reset_for_test();
	curr_test = TEST_ACTIVITY_NO;
	/* Disregard first batch read */
	for (int i = 0; i < 32; i++ ){
		row = i;
		ztest_returns_value(sensor_sample_fetch, 0);
		ztest_returns_value(sensor_channel_get, 0);
		extern raw_acc_data_t raw_data;
		fetch_and_display(sensor);
		process_acc_data(&raw_data);
	}

	for (int i = 0; i < 32; i++ ){
		row = i;
		ztest_returns_value(sensor_sample_fetch, 0);
		ztest_returns_value(sensor_channel_get, 0);
		extern raw_acc_data_t raw_data;
		fetch_and_display(sensor);
		process_acc_data(&raw_data);
	}
	zassert_equal(k_sem_take(&cur_activity_sem, K_SECONDS(30)), 0, "");
	zassert_equal(cur_activity, ACTIVITY_NO, "");

}

void test_activity_low(void)
{
	curr_test = TEST_ACTIVITY_LOW;
	for (int i = 0; i < 32; i++ ){
		row = i;
		ztest_returns_value(sensor_sample_fetch, 0);
		ztest_returns_value(sensor_channel_get, 0);
		extern raw_acc_data_t raw_data;
		fetch_and_display(sensor);
		process_acc_data(&raw_data);
	}
	zassert_equal(k_sem_take(&cur_activity_sem, K_SECONDS(30)), 0, "");
	zassert_equal(cur_activity, ACTIVITY_LOW, "");

}

void test_activity_med(void)
{
	curr_test = TEST_ACTIVITY_MED;
	for (int i = 0; i < 32; i++ ){
		row = i;
		ztest_returns_value(sensor_sample_fetch, 0);
		ztest_returns_value(sensor_channel_get, 0);
		extern raw_acc_data_t raw_data;
		fetch_and_display(sensor);
		process_acc_data(&raw_data);
	}
	zassert_equal(k_sem_take(&cur_activity_sem, K_SECONDS(30)), 0, "");
	zassert_equal(cur_activity, ACTIVITY_MED, "");

}

void test_activity_high(void)
{
	curr_test = TEST_ACTIVITY_HIGH;
	for (int i = 0; i < 32; i++ ){
		row = i;
		ztest_returns_value(sensor_sample_fetch, 0);
		ztest_returns_value(sensor_channel_get, 0);
		extern raw_acc_data_t raw_data;
		fetch_and_display(sensor);
		process_acc_data(&raw_data);
	}
	zassert_equal(k_sem_take(&cur_activity_sem, K_SECONDS(30)), 0, "");
	zassert_equal(cur_activity, ACTIVITY_HIGH, "");
}

void test_process_acc_data_correct_acc_std_final(void)
{
	_movement_controller_reset_for_test();
	raw_acc_data_t raw;
	/* The first batch of data should be disregarded */
	raw.x = 1;
	raw.y = 1;
	raw.z = 1;
	for (int i=0; i < 32; i ++) {
		process_acc_data(&raw);
		raw.x ++;
		raw.y ++;
		raw.z ++;
	}
	zassert_equal(_movement_controler_get_acc_std_final(),0,"Running average standard deviation is wrong");
	/* The second batch should be ok */
	raw.x = 1;
	raw.y = 1;
	raw.z = 1;
	for (int i=0; i < 32; i ++) {
		process_acc_data(&raw);
		raw.x ++;
		raw.y ++;
		raw.z ++;
	}
	zassert_equal(_movement_controler_get_acc_std_final(),16,"Running average standard deviation is wrong");

	/* Feed the same values again, variance should increase even more */
	raw.x = 1;
	raw.y = 1;
	raw.z = 1;
	for (int i=0; i < 32; i ++) {
		process_acc_data(&raw);
		raw.x +=100;
		raw.y +=100;
		raw.z +=100;
	}
	zassert_equal(_movement_controler_get_acc_std_final(),68,"Running average standard deviation is wrong");
}


void test_main(void)
{
	ztest_test_suite(movement_tests, 
			ztest_unit_test(test_init),
			ztest_unit_test(test_activity_no),
			ztest_unit_test(test_activity_low),
			ztest_unit_test(test_activity_med),
			ztest_unit_test(test_activity_high),
			 ztest_unit_test(test_process_acc_data_correct_acc_std_final)
			);

	ztest_run_test_suite(movement_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_movement_out_event(eh)) {
		return false;
	}
	if (is_activity_level(eh)) {
		struct activity_level *ev = cast_activity_level(eh);
		cur_activity = ev->level;
		k_sem_give(&cur_activity_sem);
		return false;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, movement_out_event);
EVENT_SUBSCRIBE(test_main, activity_level);
