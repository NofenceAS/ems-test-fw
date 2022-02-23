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

/* Number of entries available from the partition we have emulated if no 
 * headers/paddings are added, which it is. We use this number to ensure
 * we exceed the partition size when we test to append too many entries.
 */
#define LOG_ENTRY_COUNT PM_LOG_PARTITION_SIZE / sizeof(log_rec_t)

void test_log_write(void)
{
}

void test_log_read(void)
{
}

void test_write_log_exceed(void)
{
}

/** @brief Checks what happens if we try to read after there's a long time
*          since we've read previously.
*/
void test_read_log_exceed(void)
{
}

void test_empty_walk_log(void)
{
}

void test_reboot_persistent_log(void)
{
}