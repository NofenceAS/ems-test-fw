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

#include "UBX.h"

UBX_MGA_ANO_RAW_t dummy_ano = { .mga_ano.year = 0,
				.mga_ano.month = 3,
				.mga_ano.day = 9 };
size_t dummy_ano_len = sizeof(UBX_MGA_ANO_RAW_t);

uint16_t expected_valid_year = 22;

int read_callback_ano(uint8_t *data, size_t len)
{
	UBX_MGA_ANO_RAW_t *ano_frame = (UBX_MGA_ANO_RAW_t *)data;
	zassert_equal(len, dummy_ano_len, "");
	zassert_mem_equal(ano_frame, &dummy_ano, len, "");
	return 0;
}

int read_callback_inc_years(uint8_t *data, size_t len)
{
	int err = read_callback_ano(data, len);
	dummy_ano.mga_ano.year++;
	return err;
}

/* Valid years are from 22 to 29. */
int read_callback_valid_years(uint8_t *data, size_t len)
{
	dummy_ano.mga_ano.year = expected_valid_year;
	UBX_MGA_ANO_RAW_t *ano_frame = (UBX_MGA_ANO_RAW_t *)data;
	zassert_equal(len, dummy_ano_len, "");
	zassert_mem_equal(ano_frame, &dummy_ano, len, "");
	expected_valid_year++;
	return 0;
}

/** @brief Writes ano data, where each frame is one year.
 *         So it will write from year 2000 to 2020, and checks if we read all
 *         entries if read_from_boot_entry == false.
 */
void test_ano_write_20_years(void)
{
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");

	dummy_ano.mga_ano.year = 0;
	for (int i = 0; i < 20; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len),
			      0, "Write ano error.");
		dummy_ano.mga_ano.year++;
	}

	dummy_ano.mga_ano.year = 0;

	zassert_equal(stg_read_ano_data(read_callback_inc_years, false, 0), 0,
		      "Read ano error.");
}

/** @brief Writes ano data, reads, write and then read again to see if it continues
 *         where it left off. Tests if we can continue where we left of with
 *         read_from_boot_entry == false.
 */
void test_ano_write_sent(void)
{
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");

	dummy_ano.mga_ano.year = 0;
	for (int i = 0; i < 10; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len),
			      0, "Write ano error.");
		dummy_ano.mga_ano.year++;
	}

	dummy_ano.mga_ano.year = 0;

	zassert_equal(stg_read_ano_data(read_callback_inc_years, false, 0), 0,
		      "Read ano error.");

	/* Continue to write where it left off. */
	dummy_ano.mga_ano.year = 10;
	for (int i = 0; i < 10; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len),
			      0, "Write ano error.");
		dummy_ano.mga_ano.year++;
	}

	/* We now expect us to read from 10 to 20, not 0 to 20. */
	dummy_ano.mga_ano.year = 10;
	zassert_equal(stg_read_ano_data(read_callback_inc_years, false, 0), 0,
		      "Read ano error.");
}

/** @brief Writes ano data, updates the boot pointer 
 *         by simulating a reboot, and then read again from
 *         that pointer. We write 0 to 29 years, so we expect
 *         everything from 22-29 to be the callbacked results.
 */
void test_ano_write_all(void)
{
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");

	dummy_ano.mga_ano.year = 0;
	for (int i = 0; i < 30; i++) {
		zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano,
						 dummy_ano_len),
			      0, "Write ano error.");
		dummy_ano.mga_ano.year++;
	}

	/* Simualte reboot. */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	dummy_ano.mga_ano.year = 0;

	zassert_equal(stg_read_ano_data(read_callback_valid_years, true, 0), 0,
		      "Read ano error.");
}

//void test_reboot_persistent_ano(void)
//{
//	zassert_equal(stg_write_ano_data((uint8_t *)&dummy_ano, dummy_ano_len),
//		      0, "Write ano error.");
//
//	/* Reset FCB, pretending to reboot. Checks if persistent storage
//	 * works. Should get the expected read value from the written one above.
//	 */
//	int err = stg_fcb_reset_and_init();
//	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");
//
//	zassert_equal(stg_read_ano_data(read_callback_ano, ANO_READ_ALL), 0,
//		      "Read ano error.");
//}
//
///** @brief Test to check that the fcb_offset_last_n function works
// *         if there have been no writes between the requests.
// */
void test_no_ano_available(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");

	/* Read. */
	zassert_equal(stg_read_ano_data(read_callback_ano, false, 0), -ENODATA,
		      "Read ano should return -ENODATA.");
}