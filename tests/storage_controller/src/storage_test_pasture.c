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

fence_t *dummy_fence;
size_t dummy_fence_len;

void fill_dummy_fence(void)
{
	dummy_fence_len = sizeof(fence_t) + (sizeof(fence_coordinate_t) * 4);
	dummy_fence = (fence_t *)k_malloc(dummy_fence_len);

	dummy_fence->header.n_points = 4;
	dummy_fence->header.e_fence_type = 1;
	dummy_fence->header.us_id = 128;

	dummy_fence->p_c[0].s_x_dm = 0xDE;
	dummy_fence->p_c[0].s_y_dm = 0xDE;

	dummy_fence->p_c[1].s_x_dm = 0xAD;
	dummy_fence->p_c[1].s_y_dm = 0xAD;

	dummy_fence->p_c[2].s_x_dm = 0xBE;
	dummy_fence->p_c[2].s_y_dm = 0xBE;

	dummy_fence->p_c[3].s_x_dm = 0xEF;
	dummy_fence->p_c[3].s_y_dm = 0xEF;
}

int read_callback_pasture(uint8_t *data, size_t len)
{
	zassert_equal(len, dummy_fence_len, "");
	zassert_mem_equal((fence_t *)data, dummy_fence, len, "");
	return 0;
}

void test_pasture(void)
{
	zassert_equal(stg_write_to_partition(STG_PARTITION_PASTURE,
					     (uint8_t *)dummy_fence,
					     dummy_fence_len),
		      0, "Write pasture error.");
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

void test_pasture_extended_write_read(void)
{
	for (int i = 0; i < 5; i++) {
		zassert_equal(stg_write_to_partition(STG_PARTITION_PASTURE,
						     (uint8_t *)dummy_fence,
						     dummy_fence_len),
			      0, "Write pasture error.");
	}
	zassert_equal(stg_read_pasture_data(read_callback_pasture), 0,
		      "Read pasture error.");
}

void test_reboot_persistent_pasture(void)
{
	zassert_equal(stg_write_to_partition(STG_PARTITION_PASTURE,
					     (uint8_t *)dummy_fence,
					     dummy_fence_len),
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
	zassert_equal(stg_write_to_partition(STG_PARTITION_PASTURE,
					     (uint8_t *)dummy_fence,
					     dummy_fence_len),
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