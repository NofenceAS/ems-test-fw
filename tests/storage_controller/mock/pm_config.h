/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _DFU_TARGET_MOCK_H_
#define _DFU_TARGET_MOCK_H_

#include <zephyr.h>
#include <ztest.h>
#include <drivers/flash.h>
#include <devicetree.h>
#include <storage/flash_map.h>

#define PM_LOG_PARTITION_SIZE 0x80000
#define PM_ANO_PARTITION_SIZE 0x20000
#define PM_PASTURE_PARTITION_SIZE 0x20000
#define PM_SYSTEM_DIAGNOSTIC_SIZE 0x20000
#define PM_CONFIG_PARTITION_SIZE 0x2000

/* Max flash page size for writing operations. */
#define FLASH_PAGE_MAX_CNT 256

#define nordic_qspi_nor DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL

#endif