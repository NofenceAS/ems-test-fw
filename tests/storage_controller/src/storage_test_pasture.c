/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

#include "pm_config.h"
#include <stdlib.h>

#include "storage.h"

pasture_t pasture;

void init_dummy_pasture(void)
{
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
}

int read_callback_pasture(uint8_t *data, size_t len)
{
	zassert_equal(len, sizeof(pasture_t), "");
	zassert_mem_equal((pasture_t *)data, &pasture, len, "");
	return 0;
}

void test_pasture(void)
{
	zassert_equal(stg_write_pasture_data((uint8_t *)&pasture,
					     sizeof(pasture)),
		      0, "Write pasture error.");
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

void test_pasture_extended_write_read(void)
{
	for (int i = 0; i < 5; i++) {
		zassert_equal(stg_write_pasture_data((uint8_t *)&pasture,
						     sizeof(pasture)),
			      0, "Write pasture error.");
	}
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

void test_reboot_persistent_pasture(void)
{
	/* We tested with ANO data first, meaning it uses date time to validate */
	ztest_returns_value(date_time_now, 0);

	zassert_equal(stg_write_pasture_data((uint8_t *)&pasture,
					     sizeof(pasture)),
		      0, "Write pasture error.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

/** @brief Test to check that the fcb_offset_last_n function works
 *         if there have been no writes between the requests.
 */
void test_request_pasture_multiple(void)
{
	zassert_equal(stg_write_pasture_data((uint8_t *)&pasture,
					     sizeof(pasture)),
		      0, "Write pasture error.");

	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

/** @brief Test to check that the fcb_offset_last_n function works
 *         if there have been no writes between the requests.
 */
void test_no_pasture_available(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_PASTURE), 0, "");

	/* Read. */
	zassert_equal(stg_read_pasture_data(read_callback_pasture), -ENODATA,
		      "Read pasture should return -ENODATA.");
}