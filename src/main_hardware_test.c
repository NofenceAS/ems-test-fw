/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM.
 */

#include "ble/nf_ble.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <sys/printk.h>
#include <zephyr.h>

static void fetch_and_display(const struct device *sensor)
{
  static unsigned int count;
  struct sensor_value accel[3];
  const char *overrun = "";
  int rc = sensor_sample_fetch(sensor);

  ++count;
  if (rc == -EBADMSG) {
    /* Sample overrun.  Ignore in polled mode. */
    if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
      overrun = "[OVERRUN] ";
    }
    rc = 0;
  }
  if (rc == 0) {
    rc = sensor_channel_get(sensor,
                            SENSOR_CHAN_ACCEL_XYZ,
                            accel);
  }
  if (rc < 0) {
    printk("ERROR: Update failed: %d\n", rc);
  } else {
    printf("#%u @ %u ms: %sx %f , y %f , z %f\n",
           count, k_uptime_get_32(), overrun,
           sensor_value_to_double(&accel[0]),
           sensor_value_to_double(&accel[1]),
           sensor_value_to_double(&accel[2]));
  }
}

void main(void) {
  // sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
  printk("main %p\n", k_current_get());
  nf_ble_init();
#if defined(TEST_SCAN)
  nf_ble_start_scan();
#endif

  //const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dh);
  //const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dh);
  const struct device *sensor = device_get_binding(DT_LABEL(DT_INST(0, st_lis2dh)));

  if (sensor == NULL) {
    printk("No device found\n");
  }
  if (!device_is_ready(sensor)) {
    printk("Device %s is not ready\n", sensor->name);
  } else {
    printk("Device %s is ready :-)\n", sensor->name);
  }
  printk("Polling at 0.5 Hz\n");
  while (true) {
    fetch_and_display(sensor);
    k_sleep(K_MSEC(2000));
  }
}