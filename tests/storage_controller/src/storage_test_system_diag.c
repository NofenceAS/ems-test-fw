/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_helper.h"
#include "system_diagnostic_structure.h"

#include "pm_config.h"
#include <stdlib.h>

system_diagnostic_t dummy_sys_diag = {
	.error_code = -ENOMEM,
};

size_t dummy_sys_diag_len = sizeof(system_diagnostic_t);

int read_callback_sys_log(uint8_t *data, size_t len)
{
	zassert_equal(len, dummy_sys_diag_len, "");
	zassert_mem_equal((system_diagnostic_t *)data, &dummy_sys_diag, len, "");
	return 0;
}

void test_sys_diag_log(void)
{
	zassert_equal(stg_write_system_diagnostic_log((uint8_t *)&dummy_sys_diag,
						      dummy_sys_diag_len),
		      0, "Write system diagnostic error.");
	zassert_equal(stg_read_system_diagnostic_log(read_callback_sys_log, 0), 0,
		      "Read system diagnostic error.");
}

void test_reboot_persistent_system_diag(void)
{
	zassert_equal(stg_write_system_diagnostic_log((uint8_t *)&dummy_sys_diag,
						      dummy_sys_diag_len),
		      0, "Write system diagnostic error.");

	/* Clear ANO partition so that we do not call date_time. */
	zassert_false(stg_clear_partition(STG_PARTITION_ANO), "");

	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");
	zassert_equal(stg_read_system_diagnostic_log(read_callback_sys_log, 0), 0,
		      "Read system diagnostic error.");
}