/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM.
 */

#include <zephyr.h>
#include <sys/printk.h>
#include "ble/nf_ble.h"


void main(void)
{
    //sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
    nf_ble_init();
    nf_ble_start_scan();

}