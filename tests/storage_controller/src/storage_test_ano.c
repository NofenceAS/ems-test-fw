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

ano_rec_t dummy_ano = { .header.len = 4,
			.header.ID = 1,
			.header.tag = 128,
			.buf = { 0xDE, 0xAD, 0xBE, 0xEF } };
size_t dummy_ano_len = sizeof(ano_rec_t);

int read_callback_ano(uint8_t *data, size_t len)
{
	zassert_equal(len, dummy_ano_len, "");
	zassert_mem_equal((log_rec_t *)data, &dummy_ano, len, "");
	return 0;
}

void test_ano(void)
{
	zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano, dummy_ano_len,
					 true),
		      0, "Write ano error.");
	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
}

void test_ano_extended_write_read(void)
{
	zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano, dummy_ano_len,
					 true),
		      0, "Write ano error.");
	for (int i = 0; i < 5; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len, false),
			      0, "Write ano error.");
	}
	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
}

void test_reboot_persistent_ano(void)
{
	zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano, dummy_ano_len,
					 true),
		      0, "Write ano error.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
}

/** @brief Test to check that the fcb_offset_last_n function works
 *         if there have been no writes between the requests.
 */
void test_request_ano_multiple(void)
{
	zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano, dummy_ano_len,
					 true),
		      0, "Write ano error.");
	for (int i = 0; i < 5; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len, false),
			      0, "Write ano error.");
	}

	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
	zassert_equal(stg_read_ano_data(read_callback_ano), 0,
		      "Read ano error.");
}

/** @brief Test to check that the fcb_offset_last_n function works
 *         if there have been no writes between the requests.
 */
void test_no_ano_available(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");

	/* Read. */
	zassert_equal(stg_read_ano_data(read_callback_ano), -ENODATA,
		      "Read ano should return -ENODATA.");
}