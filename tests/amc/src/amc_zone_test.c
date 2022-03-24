#include "amc_test_common.h"
#include "amc_zone.h"
#include <ztest.h>

void test_zone_calc(void)
{
	zassert_equal(zone_get(), 
		      NO_ZONE, 
		      "Zone not initialized correctly");
	
	/* Position on border */
	zassert_equal(zone_update(0), 
		      WARN_ZONE, 
		      "Position on border, but not in warn zone");
	
	/* Get position after change */
	zassert_equal(zone_get(), 
		      WARN_ZONE, 
		      "Zone not updated correctly");

	/* Even further outside border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(i), 
			      WARN_ZONE, 
			      "Not in warn zone as expected");
	}

	/* Moving back to border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(10-1-i), 
			      WARN_ZONE, 
			      "Not in warn zone as expected");
	}

	/* Going within border */
	zassert_equal(zone_update(-1), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");

	/* Going to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");

	/* Going within caution, from above, must go one beyond to be outside [start, end> */
	zassert_equal(
		zone_update(-71), 
		CAUTION_ZONE, 
		"Not in caution zone as expected");

	/* Going back to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60), 
		      CAUTION_ZONE, 
		      "Not in caution zone as expected");

	/* Going to prewarn, from below, can be on hysteresis border and be within [start, end> */
	zassert_equal(zone_update(-50), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");
	
	/* Going well within warn zone */
	zassert_equal(zone_update(250), 
		      WARN_ZONE, 
		      "Not in warn zone as expected");
	
	/* Going well within psm zone */
	zassert_equal(zone_update(-250), 
		      PSM_ZONE, 
		      "Not in psm zone as expected");
	
	/* Failing position */
	zassert_equal(zone_set(NO_ZONE), 0, "Unable to set NO_ZONE");
	zassert_equal(zone_get(), 
		      NO_ZONE, 
		      "Not in no zone as expected");
	
	/* Extremes minimum */
	zassert_equal(zone_update(INT16_MIN), 
		      PSM_ZONE, 
		      "Not in psm zone as expected");
	
	/* Extremes maximum */
	zassert_equal(zone_update(INT16_MAX), 
		      WARN_ZONE, 
		      "Not in warn zone as expected");
	
	/* Set erroneous zone */
	zassert_equal(zone_set(94), -EINVAL, "Wrong result code when setting invalid zone");
}