/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_handler.h"
#include "sound_event.h"
#include "messaging_module_events.h"
#include "pasture_structure.h"
#include "gnss.h"
#include "request_events.h"
#include "amc_cache.h"
#include "amc_states_cache.h"
#include "amc_dist.h"
#include "error_event.h"
#include "amc_test_common.h"
#include "embedded.pb.h"
#include "gnss_controller_events.h"

static K_SEM_DEFINE(fence_sema, 0, 1);
static K_SEM_DEFINE(error_sema, 0, 1);

static FenceStatus m_current_fence_status = FenceStatus_FenceStatus_UNKNOWN;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init_and_update_pasture(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	/* ..init_states_and_variables, cached variables. 1. tot zap count, 
	 * 2. warn count, 3. zap count day */
	ztest_returns_value(eep_uint16_read, 0);
	ztest_returns_value(eep_uint32_read, 0);
	ztest_returns_value(eep_uint16_read, 0);

	/* ..init_states_and_variables, cached mode. */
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_read, 0);
	ztest_returns_value(eep_uint8_write, 0);

	/* update_pasture_from_stg */
	ztest_returns_value(stg_read_pasture_data, 0);

	/* Cached keep mode. */
	ztest_returns_value(eep_uint8_read, 0);

	zassert_false(amc_module_init(), "Error when initializing AMC module");
}

void test_set_get_pasture(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type =
		FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = -10 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(&pasture.fences[0].coordinates[0], points1, sizeof(points1));

	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");

	pasture_t wrong_pasture;
	memset(&wrong_pasture, 0, sizeof(pasture_t));
	pasture_t *new_pasture = &wrong_pasture;
	zassert_false(get_pasture_cache(&new_pasture), "");
	zassert_mem_equal(&pasture, new_pasture, sizeof(pasture_t), "");
}

void test_fnc_valid_fence_exists(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type =
		FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 1, .s_y_dm = -1 },
		{ .s_x_dm = 1, .s_y_dm = 1 },
		{ .s_x_dm = -1, .s_y_dm = 1 },
		{ .s_x_dm = -1, .s_y_dm = -1 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");
	zassert_true(fnc_valid_fence(), "");
}

void test_empty_fence(void)
{
	pasture_t pasture = {
		.m.ul_total_fences = 0,
	};
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");
	zassert_false(fnc_valid_fence(), "");
}

void test_update_pasture(void)
{
	/* Test: Updating AMC pasture where everything goes as intended.
	 * Reads pasture from storage, sets new cached pasture for future use, sends
	 * new fence definition and fence status (NotStarted) to server.
	 */

	/* Set fence status to "UNKNOWN" before starting the update process */
	ztest_returns_value(eep_uint8_write, 0);
	zassert_equal(force_fence_status(FenceStatus_FenceStatus_UNKNOWN), 0,
		      "");
	k_sem_reset(&fence_sema);
	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0, "");

	/* update_pasture_from_stg() */
	ztest_returns_value(stg_read_pasture_data, 0);

	/* Read keep mode from eeprom */
	ztest_returns_value(eep_uint8_read, 0);

	/* ..force_fence_status() */
	ztest_returns_value(eep_uint8_write, 0);

	zone_set(WARN_ZONE);
	zassert_equal(zone_get(), WARN_ZONE, "Zone not set to WARN_ZONE!");
	/* Submit fence update event */
	struct new_fence_available *event = new_new_fence_available();
	event->new_fence_version = 1337;
	EVENT_SUBMIT(event);

	k_sem_reset(&fence_sema);
	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0,
		      "Failed to receive update fence status event");

	zassert_equal(m_current_fence_status, FenceStatus_NotStarted,
		      "Failed to force correct fence status");
	zassert_equal(
		get_fence_status(), m_current_fence_status,
		"Actual fence status is not equal to reported fence status");
	zassert_equal(zone_get(), NO_ZONE,
		      "Zone not reset to NO_ZONE with "
		      "the new pasture!");
}

void test_update_pasture_teach_mode(void)
{
	/* Test: Updating AMC pasture where reading keep_mode for eeprom fails.
	 * Reads pasture from storage, as read from eeprom fails the default
	 * behaviour should be to force teach mode, sets new cached pasture for 
	 * future use, sends new fence definition and fence status (NotStarted) to 
	 * server. 
	 */

	/* Set fence status to "UNKNOWN" before starting the update process */
	ztest_returns_value(eep_uint8_write, 0);
	zassert_equal(force_fence_status(FenceStatus_FenceStatus_UNKNOWN), 0,
		      "");
	k_sem_reset(&fence_sema);
	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0, "");

	/* update_pasture_from_stg() */
	ztest_returns_value(stg_read_pasture_data, 0);

	/* Fails to read keep_mode from eeprom */
	ztest_returns_value(eep_uint8_read, -1);

	/* ..force_teach_mode() */
	ztest_returns_value(eep_uint8_write, 0);
	ztest_returns_value(eep_uint32_read, 0);
	ztest_returns_value(eep_uint16_read, 0);
	ztest_returns_value(eep_uint8_read, 0);

	/* ..force_fence_status() */
	ztest_returns_value(eep_uint8_write, 0);

	/* handle_states_fn()/calc_fence_status() */
	ztest_returns_value(eep_uint8_write, 0);

	/* Submit fence update event */
	struct new_fence_available *event = new_new_fence_available();
	event->new_fence_version = 1337;
	EVENT_SUBMIT(event);

	k_sem_reset(&fence_sema);
	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0,
		      "Failed to receive update fence status event");

	zassert_equal(m_current_fence_status, FenceStatus_NotStarted,
		      "Failed to force correct fence status");
	zassert_equal(
		get_fence_status(), m_current_fence_status,
		"Actual fence status is not equal to reported fence status");

	zassert_equal(get_mode(), Mode_Teach, "Failed to force teach mode");
}

void test_update_pasture_stg_fail(void)
{
	/* Test: Updating AMC pasture with read from storage error.
	 * If read from storage fails, send error event and return immediately.
	 */
	/* Set fence status to "UNKNOWN" before starting the update process */
	ztest_returns_value(eep_uint8_write, 0);
	zassert_equal(force_fence_status(FenceStatus_FenceStatus_UNKNOWN), 0,
		      "");
	k_sem_reset(&fence_sema);
	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0, "");

	/* update_pasture_from_stg() */
	ztest_returns_value(stg_read_pasture_data, -1); //Fails to read

	/* handle_states_fn()/calc_fence_status() */
	ztest_returns_value(eep_uint8_write, 0);

	zassert_equal(zone_get(), NO_ZONE,
		      "Zone not reset to NO_ZONE with "
		      "failing to read a new pasture "
		      "from storage!");
	zone_set(WARN_ZONE);
	zassert_equal(zone_get(), WARN_ZONE, "Zone not set to WARN_ZONE!");
	/* Submit fence update event */
	struct new_fence_available *event = new_new_fence_available();
	EVENT_SUBMIT(event);

	k_sem_reset(&error_sema);
	zassert_equal(k_sem_take(&error_sema, K_SECONDS(10)), 0,
		      "Failed to receive update fence error event");
}

static void update_position(int delta_lat, int delta_lon)
{
	static int32_t my_lat = 1000;
	static int32_t my_lon = 0;

	my_lat += delta_lat;
	my_lon += delta_lon;

	struct gnss_data *new_position = new_gnss_data();
	new_position->gnss_data.fix_ok = true;
	new_position->gnss_data.has_lastfix = true;
	new_position->gnss_data.latest.pvt_flags = 1;
	new_position->gnss_data.latest.num_sv = 7;
	new_position->gnss_data.latest.h_dop = 80;
	new_position->gnss_data.latest.h_acc_dm = 65;
	new_position->gnss_data.latest.height = 100;
	new_position->gnss_data.latest.lat = my_lat;
	new_position->gnss_data.latest.lon = my_lon;
	EVENT_SUBMIT(new_position);
	return;
}

void test_update_pasture_integration(void)
{
	/* set pasture cache to a 20x20 offset by 1000m to the right from the
	 * origin. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
		.m.ul_fence_def_version = 1337, //todo: set_pasture_cache is
		// not updating this value in the unit tests!
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type =
		FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;
	int offset = 1000;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 10 + offset, .s_y_dm = -10 },
		{ .s_x_dm = 10 + offset, .s_y_dm = 10 },
		{ .s_x_dm = -10 + offset, .s_y_dm = 10 },
		{ .s_x_dm = -10 + offset, .s_y_dm = -10 },
		{ .s_x_dm = 10 + offset, .s_y_dm = -10 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(&pasture.fences[0].coordinates[0], points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");
	pasture_t *new_pasture = NULL;
	zassert_false(get_pasture_cache(&new_pasture), "");
	zassert_mem_equal(&pasture, new_pasture, sizeof(pasture_t), "");
	k_sem_reset(&fence_sema);
	//	zassert_equal(k_sem_take(&fence_sema, K_SECONDS(10)), 0, "");

	/* update_pasture_from_stg() */
	ztest_returns_value(stg_read_pasture_data, 0);

	/* Read keep mode from eeprom */
	ztest_returns_value(eep_uint8_read, 0);

	/* ..force_fence_status() */
	ztest_returns_value(eep_uint8_write, 0);

	/* Submit fence update event */
	struct new_fence_available *event = new_new_fence_available();
	event->new_fence_version = pasture.m.ul_fence_def_version;
	EVENT_SUBMIT(event);
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");

	for (int i = 0; i < 100; i++) {
		update_position(1, 1);
		k_sleep(K_MSEC(250));
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_update_fence_status(eh)) {
		struct update_fence_status *event =
			cast_update_fence_status(eh);
		m_current_fence_status = event->fence_status;
		k_sem_give(&fence_sema);
		return false;
	}
	if (is_error_event(eh)) {
		struct error_event *event = cast_error_event(eh);
		if (event->sender == ERR_AMC) {
			k_sem_give(&error_sema);
		}
		return false;
	}
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests,
			 ztest_unit_test(test_init_and_update_pasture),
			 ztest_unit_test(test_set_get_pasture),
			 ztest_unit_test(test_fnc_valid_fence_exists),
			 ztest_unit_test(test_empty_fence),
			 ztest_unit_test(test_update_pasture_teach_mode),
			 ztest_unit_test(test_update_pasture_stg_fail),
			 ztest_unit_test(test_update_pasture));
	ztest_run_test_suite(amc_tests);

	ztest_test_suite(amc_dist_tests,
			 ztest_unit_test(test_fnc_calc_dist_quadratic),
			 ztest_unit_test(test_fnc_calc_dist_quadratic_max),
			 ztest_unit_test(test_fnc_calc_dist_rect),
			 ztest_unit_test(test_fnc_calc_dist_2_fences_hole),
			 ztest_unit_test(test_fnc_calc_dist_2_fences_hole2),
			 ztest_unit_test(test_fnc_calc_dist_2_fences_max_size));
	ztest_run_test_suite(amc_dist_tests);

	ztest_test_suite(amc_zone_tests, ztest_unit_test(test_zone_calc));
	ztest_run_test_suite(amc_zone_tests);

	ztest_test_suite(amc_gnss_tests, ztest_unit_test(test_gnss_fix),
			 ztest_unit_test(test_gnss_mode));
	ztest_run_test_suite(amc_gnss_tests);

	ztest_test_suite(amc_collar_fence_tests,
			 ztest_unit_test(test_init_and_update_pasture),
			 ztest_unit_test(test_collar_status),
			 ztest_unit_test(test_collar_mode),
			 ztest_unit_test(test_fence_status));
	ztest_run_test_suite(amc_collar_fence_tests);

	//	ztest_test_suite(amc_integration_tests,
	//			 ztest_unit_test(test_update_pasture_integration));
	/*TODO: We need to find a way to simulate new pastures, to be able to
	 * run full simulation tests.*/
	//	ztest_run_test_suite(amc_integration_tests);
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, update_fence_status);
EVENT_SUBSCRIBE(test_main, error_event);
