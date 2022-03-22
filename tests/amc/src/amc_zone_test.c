#include "amc_test_common.h"
#include "amc_zone.h"
#include <ztest.h>

void test_zone_calc(void)
{
	zone_reset();

	zassert_equal(zone_get(), 
		      NO_ZONE, 
		      "Zone not reset correctly");

	/* On border, but position is faulty */
	zassert_equal(zone_update(0, false), 
		      NO_ZONE, 
		      "Position not OK, but not in no zone");
	
	/* Position recovered, still on border */
	zassert_equal(zone_update(0, true), 
		      WARN_ZONE, 
		      "Position OK on border, but not in warn zone");
	
	/* Even further outside border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(i, true), 
			      WARN_ZONE, 
			      "Not in warn zone as expected");
	}

	/* Moving back to border */
	for (int i = 0; i < 10; i++) {
		zassert_equal(zone_update(10-1-i, true), 
			      WARN_ZONE, 
			      "Not in warn zone as expected");
	}

	/* Going within border */
	zassert_equal(zone_update(-1, true), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");

	/* Going to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60, true), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");

	/* Going within caution, from above, must go one beyond to be outside [start, end> */
	zassert_equal(
		zone_update(-71, 
			    true), 
		CAUTION_ZONE, 
		"Not in caution zone as expected");

	/* Going back to prewarn/caution border, but not over hysteresis */
	zassert_equal(zone_update(-60, true), 
		      CAUTION_ZONE, 
		      "Not in caution zone as expected");

	/* Going to prewarn, from below, can be on hysteresis border and be within [start, end> */
	zassert_equal(zone_update(-50, true), 
		      PREWARN_ZONE, 
		      "Not in prewarn zone as expected");
	
	/* Going well within warn zone */
	zassert_equal(zone_update(250, true), 
		      WARN_ZONE, 
		      "Not in warn zone as expected");
	
	/* Going well within psm zone */
	zassert_equal(zone_update(-250, true), 
		      PSM_ZONE, 
		      "Not in psm zone as expected");
	
	/* Failing position */
	zassert_equal(zone_update(-250, false), 
		      NO_ZONE, 
		      "Not in no zone as expected");
}