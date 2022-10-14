/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage_helper.h"
#include "stg_config.h"
#include "pm_config.h"

void test_stg_init()
{
    zassert_equal(stg_config_init(), 0, "");
}
