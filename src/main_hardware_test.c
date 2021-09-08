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

static void fetch_and_display(const struct device *sensor) {
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
    rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
  }
  if (rc < 0) {
    printk("ERROR: Update failed: %d\n", rc);
  } else {
    printf("#%u @ %u ms: %sx %f , y %f , z %f\n", count, k_uptime_get_32(),
           overrun, sensor_value_to_double(&accel[0]),
           sensor_value_to_double(&accel[1]),
           sensor_value_to_double(&accel[2]));
  }
}

#ifdef CONFIG_LIS2DH_TRIGGER
static void trigger_handler(const struct device *dev,
                            struct sensor_trigger *trig) {
  fetch_and_display(dev);
}
#endif

void main(void) {
  // sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
  printk("main %p\n", k_current_get());
  nf_ble_init();
#if defined(TEST_SCAN)
  nf_ble_start_scan();
#endif

  // const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dh);
  // const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dh);
  const struct device *sensor =
      device_get_binding(DT_LABEL(DT_INST(0, st_lis2dh)));

  if (sensor == NULL) {
    printk("No device found\n");
  }
  if (!device_is_ready(sensor)) {
    printk("Device %s is not ready\n", sensor->name);
  } else {
    printk("Device %s is ready :-)\n", sensor->name);
  }


#if CONFIG_LIS2DH_TRIGGER
  {
    struct sensor_trigger trig;
    int rc;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_XYZ;

    if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME)) {
      struct sensor_value odr = {
          .val1 = 1,
      };

      rc = sensor_attr_set(sensor, trig.chan, SENSOR_ATTR_SAMPLING_FREQUENCY,
                           &odr);
      if (rc != 0) {
        printf("Failed to set odr: %d\n", rc);
        return;
      }
      printf("Sampling at %u Hz\n", odr.val1);
    }

    rc = sensor_trigger_set(sensor, &trig, trigger_handler);
    if (rc != 0) {
      printf("Failed to set trigger: %d\n", rc);
      return;
    }

    printf("Waiting for triggers\n");
    while (true) {
      k_sleep(K_MSEC(2000));
    }
  }
#else  /* CONFIG_LIS2DH_TRIGGER */
  printf("Polling at 0.5 Hz\n");
  while (true) {
    fetch_and_display(sensor);
    k_sleep(K_MSEC(2000));
  }
#endif /* CONFIG_LIS2DH_TRIGGER */
}