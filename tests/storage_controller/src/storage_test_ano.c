/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

#include "pm_config.h"
#include <stdlib.h>

void test_ano_write(void)
{
	write_ano_data();
	int err = k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write ano ack.");
}

void test_ano_read(void)
{
	request_ano_data();
	int err = k_sem_take(&read_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read ano ack.");
	err = k_sem_take(&consumed_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed ano ack.");
}

void test_reboot_persistent_ano(void)
{
	/* Write something. */
	write_ano_data();
	int err = k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write ano ack.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */

	err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	/* Read something. */
	request_ano_data();
	err = k_sem_take(&read_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read ano ack.");
	err = k_sem_take(&consumed_ano_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed ano ack.");
}