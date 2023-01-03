#include "amc_test_common.h"
#include "amc_zone.h"
#include <ztest.h>

void test_zone_calc(void)
{
	zone_set(NO_ZONE);

	gnss_t gnss_data = {
		.latest = { .pvt_flags = 1, .msss = 25000, .updated_at = k_uptime_get_32() },
		.fix_ok = true,
		.lastfix = { .pvt_flags = 1 },
		.has_lastfix = true
	};

	amc_zone_t cur_zone = NO_ZONE;

	zassert_equal(zone_get(), NO_ZONE, "Zone not initialized correctly");

	/* Check updated since timer */
	zassert_true(zone_get_time_since_update() < 5, "Time since update is wrong");

	k_sleep(K_MSEC(100));

	zassert_true((zone_get_time_since_update() > 100) && (zone_get_time_since_update() < 150),
		     "Time since update is wrong");

	/* Position on border */
	zassert_equal(zone_update(0, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, WARN_ZONE, "Position on border, but not in warn zone");

	/* Check updated since timer reset */
	zassert_true(zone_get_time_since_update() < 5, "Time since update is wrong");

	/* Get position after change */
	zassert_equal(zone_get(), WARN_ZONE, "Zone not updated correctly");

	/* Check updated since timer */
	k_sleep(K_MSEC(500));
	zassert_true((zone_get_time_since_update() > 500) && (zone_get_time_since_update() < 550),
		     "Time since update is wrong");

	printk("Since updated: %d", (uint32_t)zone_get_time_since_update());
	/* Even further outside border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(i, &gnss_data, &cur_zone), 0, "Failed to update zone");

		zassert_equal(cur_zone, WARN_ZONE, "Not in warn zone as expected");

		k_sleep(K_MSEC(100));
		printk("Since updated: %d", (uint32_t)zone_get_time_since_update());
	}
	/* No change, check timer, wide range since simulator is not good with time */
	printk("Since updated: %d", (uint32_t)zone_get_time_since_update());
	zassert_true((zone_get_time_since_update() > 1500) && (zone_get_time_since_update() < 1750),
		     "Time since update is wrong");

	/* Moving back to border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(10 - 1 - i, &gnss_data, &cur_zone), 0,
			      "Failed to update zone");

		zassert_equal(cur_zone, WARN_ZONE, "Not in warn zone as expected");
	}

	/* Going within border */
	zassert_equal(zone_update(-1, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, PREWARN_ZONE, "Not in prewarn zone as expected");

	/* Going to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, PREWARN_ZONE, "Not in prewarn zone as expected");

	/* Going within caution, from above, must go one beyond to be outside [start, end> */
	zassert_equal(zone_update(-71, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, CAUTION_ZONE, "Not in caution zone as expected");

	/* Going back to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, CAUTION_ZONE, "Not in caution zone as expected");

	/* Going to prewarn, from below, can be on hysteresis border and be within [start, end> */
	zassert_equal(zone_update(-50, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, PREWARN_ZONE, "Not in prewarn zone as expected");

	/* Going well within warn zone */
	zassert_equal(zone_update(250, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, WARN_ZONE, "Not in warn zone as expected");

	/* Going well within psm zone */
	zassert_equal(zone_update(-250, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, PSM_ZONE, "Not in psm zone as expected");

	/* Failing position */
	zassert_equal(zone_set(NO_ZONE), 0, "Unable to set NO_ZONE");
	zassert_equal(zone_get(), NO_ZONE, "Not in no zone as expected");

	/* Extremes minimum */
	zassert_equal(zone_update(INT16_MIN, &gnss_data, &cur_zone), 0, "Failed to update zone");
	zassert_equal(cur_zone, PSM_ZONE, "Not in psm zone as expected");

	/* Extremes maximum, but should fail due to low time since PSM start */
	zassert_equal(zone_update(INT16_MAX, &gnss_data, &cur_zone), -ETIME,
		      "Not enough time since PSM");

	/* Extremes maximum, With wait since PSM */
	k_sleep(K_MSEC(16000));
	zassert_equal(zone_update(INT16_MAX, &gnss_data, &cur_zone), 0, "Failed to update zone");

	zassert_equal(cur_zone, WARN_ZONE, "Not in warn zone as expected");

	/* Set erroneous zone */
	zassert_equal(zone_set(94), -EINVAL, "Wrong result code when setting invalid zone");
}