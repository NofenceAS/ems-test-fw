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
#include "amc_dist.h"

#include "error_event.h"
#include "amc_test_common.h"

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

	/* Cached pasture. */
	ztest_returns_value(stg_read_pasture_data, 0);

	/* Cached variables. 1. tot zap count, 2. warn count, 3. zap count day */
	ztest_returns_value(eep_uint16_read, 0);
	ztest_returns_value(eep_uint32_read, 0);
	ztest_returns_value(eep_uint16_read, 0);

	/* Cached mode. */
	ztest_returns_value(eep_uint8_read, 0);

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
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 0,
	};

	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");

	zassert_false(fnc_valid_fence(), "");
}

static bool event_handler(const struct event_header *eh)
{
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests,
			 ztest_unit_test(test_init_and_update_pasture),
			 ztest_unit_test(test_set_get_pasture),
			 ztest_unit_test(test_fnc_valid_fence_exists),
			 ztest_unit_test(test_empty_fence));
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
}

EVENT_LISTENER(test_main, event_handler);
