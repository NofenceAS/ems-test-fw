#include <ztest.h>
#include "storage.h"
#include "pasture_structure.h"

pasture_t pasture;

int stg_read_pasture_data(fcb_read_cb cb)
{
	int retval = ztest_get_return_value();
	if (retval) {
		return retval;
	}

	pasture.m.ul_total_fences = 2;

	pasture.fences[0].m.fence_no = 0;
	pasture.fences[0].m.n_points = 2;
	pasture.fences[0].coordinates[0].s_x_dm = 1;
	pasture.fences[0].coordinates[0].s_y_dm = 2;
	pasture.fences[0].coordinates[1].s_x_dm = 3;
	pasture.fences[0].coordinates[1].s_y_dm = 4;

	pasture.fences[1].m.fence_no = 1;
	pasture.fences[1].m.n_points = 3;
	pasture.fences[1].coordinates[0].s_x_dm = 1;
	pasture.fences[1].coordinates[0].s_y_dm = 2;
	pasture.fences[1].coordinates[1].s_x_dm = 3;
	pasture.fences[1].coordinates[1].s_y_dm = 4;
	pasture.fences[1].coordinates[2].s_x_dm = 5;
	pasture.fences[1].coordinates[2].s_y_dm = 6;

	zassert_false(cb((uint8_t *)&pasture, sizeof(pasture)), "");

	return retval;
}