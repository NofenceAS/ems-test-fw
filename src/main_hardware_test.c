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
#include <autoconf.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gps.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <logging/log.h>
#include <stdio.h>
#include <sys/printk.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main);

//#define TEST_ZOEM8B_GPS 1
// #define TEST_LIS2DH 1
// #define TEST_FLASH 1

#define GSM_DEVICE DT_LABEL(DT_INST(0, ublox_sara_r4))
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#define FLASH_TEST_REGION_OFFSET 0x000
#define FLASH_PAGE_SIZE 256

#ifdef TEST_LIS2DH
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
		rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
	}
	if (rc < 0) {
		printk("ERROR: Update failed: %d\n", rc);
	} else {
		printf("#%u @ %u ms: %sx %f , y %f , z %f\n", count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}
}

#ifdef CONFIG_LIS2DH_TRIGGER
static void trigger_handler(const struct device *dev, struct sensor_trigger *trig)
{
	fetch_and_display(dev);
}
#endif
#endif

#ifdef TEST_ZOEM8B_GPS
static void gps_event_handler(const struct device *dev, struct gps_event *evt)
{
}

/* GPS device. Used to identify the GPS driver in the sensor API. */
static const struct device *gps_dev;

static int setup_gps(void)
{
	int err;

	gps_dev = device_get_binding(DT_LABEL(DT_INST(0, ublox_zoem8bspi)));
	if (gps_dev == NULL) {
		LOG_ERR("Could not get %s device", CONFIG_ZOEM8B_GPS_DEV_NAME);
		return -ENODEV;
	}

	err = gps_init(gps_dev, gps_event_handler);
	if (err) {
		LOG_ERR("Could not initialize GPS, error: %d", err);
		return err;
	}

	return 0;
}
#endif

#include "collar_protocol.h"

static void test_encode_valid_data(void) 
{
	NofenceMessage reqMsg;
	memset(&reqMsg, 0, sizeof(reqMsg));

	reqMsg.header.has_ulVersion = true;
	reqMsg.header.ulVersion = 0;
	reqMsg.header.ulId = 123;
	reqMsg.header.ulUnixTimestamp = 1234;
	reqMsg.which_m = NofenceMessage_firmware_upgrade_req_tag;
	reqMsg.m.firmware_upgrade_req.ulVersion = 999998;
	reqMsg.m.firmware_upgrade_req.has_ucHardwareVersion = true;
	reqMsg.m.firmware_upgrade_req.ucHardwareVersion = 13;
	reqMsg.m.firmware_upgrade_req.has_versionInfoHW = true;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.ucPCB_RF_Version = 13;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.usPCB_Product_Type = 1;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.ucPCB_HV_Version = 0;
	reqMsg.m.firmware_upgrade_req.usFrameNumber = 0;
	
	uint8_t raw_buf[NofenceMessage_size];
	size_t raw_size = 0;
	if (collar_protocol_encode(&reqMsg, raw_buf, sizeof(raw_buf), &raw_size)) {
		// Test failed
		LOG_ERR("collar_protocol_encode failed for valid data\n");
		return;
	}

	NofenceMessage message;
	memset(&message, 0, sizeof(message));

	if (collar_protocol_decode(raw_buf, raw_size, &message)) {
		// Test failed
		LOG_ERR("collar_protocol_decode failed for valid data\n");
		return;
	}

	if (memcmp(&reqMsg, &message, sizeof(NofenceMessage)) != 0) {
		// Test failed
		LOG_ERR("collar_protocol_decode resulted in unexpected data\n");
		return;
	}
	LOG_INF("test_encode_valid_data PASSED!\n");
}

static void test_decode_valid_data(void)
{
	NofenceMessage message;
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, 0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, 0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	if (collar_protocol_decode(raw_buf, sizeof(raw_buf), &message)) {
		// Test failed
		LOG_ERR("collar_protocol_decode failed for valid data\n");
		return;
	}

	if ((!message.header.has_ulVersion) ||
	    (message.header.ulVersion != 327) || 
	    (message.header.ulId != 1337) ||
	    (message.header.ulUnixTimestamp != 1638999508) ||
	    (message.which_m != NofenceMessage_firmware_upgrade_req_tag) ||
	    (message.m.firmware_upgrade_req.ulVersion != 929598) ||
	    (!message.m.firmware_upgrade_req.has_ucHardwareVersion) ||
	    (message.m.firmware_upgrade_req.ucHardwareVersion != 14) ||
	    (!message.m.firmware_upgrade_req.has_versionInfoHW) ||
	    (message.m.firmware_upgrade_req.versionInfoHW.ucPCB_RF_Version != 12) ||
	    (message.m.firmware_upgrade_req.versionInfoHW.usPCB_Product_Type != 1) ||
	    (message.m.firmware_upgrade_req.versionInfoHW.ucPCB_HV_Version != 0) ||
	    (message.m.firmware_upgrade_req.usFrameNumber != 16)) {
		// Test failed
		LOG_ERR("collar_protocol_decode resulted in unexpected data\n");
		return;
	}
	LOG_INF("test_decode_valid_data PASSED!\n");
}

static void test_invalid_input(void) {
	// Valid protobuf data of firmware_upgrade_req
	size_t raw_size = 0;
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, 0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, 0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	NofenceMessage message;
	
	if ((collar_protocol_decode(raw_buf, sizeof(raw_buf), NULL) != -EINVAL) ||
	    (collar_protocol_decode(raw_buf, 0, &message) != -EINVAL) || 
	    (collar_protocol_decode(raw_buf, 0, NULL) != -EINVAL) || 
	    (collar_protocol_decode(NULL, sizeof(raw_buf), &message) != -EINVAL) || 
	    (collar_protocol_decode(NULL, sizeof(raw_buf), NULL) != -EINVAL) || 
	    (collar_protocol_decode(NULL, 0, &message) != -EINVAL) || 
	    (collar_protocol_decode(NULL, 0, NULL) != -EINVAL)) {
		// Test failed, decode with NULL/0 values
		LOG_ERR("collar_protocol_decode returned wrong error for invalid arguments\n");
		return;
	}

	// Decode raw_buf, should work since it contains valid data and was tested previously
	if (collar_protocol_decode(raw_buf, sizeof(raw_buf), &message)) {
		// Test failed, unexpectedly
		LOG_ERR("collar_protocol_decode returned unexpected error\n");
		return;
	}

	if ((collar_protocol_encode(&message, raw_buf, sizeof(raw_buf), NULL) != -EINVAL) ||
	    (collar_protocol_encode(&message, raw_buf, 0,               &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(&message, raw_buf, 0,               NULL) != -EINVAL) ||
	    (collar_protocol_encode(&message, NULL,    sizeof(raw_buf), &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(&message, NULL,    sizeof(raw_buf), NULL) != -EINVAL) ||
	    (collar_protocol_encode(&message, NULL,    0,               &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(&message, NULL,    0,               NULL) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     raw_buf, sizeof(raw_buf), &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     raw_buf, sizeof(raw_buf), NULL) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     raw_buf, 0,               &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     raw_buf, 0,               NULL) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     NULL,    sizeof(raw_buf), &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     NULL,    sizeof(raw_buf), NULL) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     NULL,    0,               &raw_size) != -EINVAL) ||
	    (collar_protocol_encode(NULL,     NULL,    0,               NULL) != -EINVAL)) {
		// Test failed, encode with NULL/0 values returned wrong code
		LOG_ERR("collar_protocol_encode returned wrong error for invalid arguments\n");
		return;
	}
	LOG_INF("test_invalid_input PASSED!\n");
}

static void test_buffer_overflow(void) {
	// Valid protobuf data of firmware_upgrade_req
	size_t raw_size = 0;
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, 0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, 0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	NofenceMessage message;

	// Decode raw_buf, should work since it contains valid data and was tested previously
	if (collar_protocol_decode(raw_buf, sizeof(raw_buf), &message)) {
		// Test failed, unexpectedly
		LOG_ERR("collar_protocol_decode failed for valid data\n");
		return;
	}

	// Attempt encode with buffer size one less than required
	if (collar_protocol_encode(&message, raw_buf, sizeof(raw_buf)-1, &raw_size) != -EMSGSIZE) {
		// Test failed, encode with too small buffer returned wrong code
		LOG_ERR("collar_protocol_encode returned wrong result for insufficient buffer size (-1)\n");
		return;
	}

	// Attempt encode with buffer size half of required
	if (collar_protocol_encode(&message, raw_buf, sizeof(raw_buf)/2, &raw_size) != -EMSGSIZE) {
		// Test failed, encode with too small buffer returned wrong code
		LOG_ERR("collar_protocol_encode returned wrong result for insufficient buffer size (/2)\n");
		return;
	}

	// Attempt encode with buffer size of 1
	if (collar_protocol_encode(&message, raw_buf, 1, &raw_size) != -EMSGSIZE) {
		// Test failed, encode with too small buffer returned wrong code
		LOG_ERR("collar_protocol_encode returned wrong result for insufficient buffer size (=1)\n");
		return;
	}
	LOG_INF("test_buffer_overflow PASSED!\n");
}

static void test_corrupted_decode(void) {
	// Attempt decode with corrupted buffer data
	NofenceMessage message;
	uint8_t corrupted_raw_buf[] = {0xFF, 0xFE, 0x99, 0x78, 0x0A, 0x10, 0xD4, 0xFF, 0xC4, 0x8D, 0x00, 0x18, 0xC7, 0x02, 0xAA, 0x10, 0x08, 0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	if (collar_protocol_decode(corrupted_raw_buf, sizeof(corrupted_raw_buf), &message) != -EILSEQ) {
		// Test failed, decode with corrupted buffer returned wrong code
		LOG_ERR("collar_protocol_decode returned wrong result for corrupted data\n");
		return;
	}
	LOG_INF("test_corrupted_decode PASSED!\n");
}

void main(void)
{
	LOG_INF("Running collar protocol tests: \n");
	test_encode_valid_data();
	test_decode_valid_data();
	test_invalid_input();
	test_buffer_overflow();
	test_corrupted_decode();
	// sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
	printk("main %p\n", k_current_get());
	LOG_ERR("main starting!\n");
#if defined(TEST_SCAN)
	nf_ble_init();
#endif
	const struct device *gsm_dev = device_get_binding(GSM_DEVICE);
	if (!gsm_dev) {
		LOG_ERR("GSM driver %s was not found!\n", GSM_DEVICE);
		return;
	}
#if defined(TEST_FLASH)
	const struct device *flash_dev;
	const uint8_t expected[] = { 0x55, 0xaa, 0x66, 0x99 };
	const size_t len = sizeof(expected);
	uint8_t buf[sizeof(expected)];
	int rc;

	LOG_INF("\n" FLASH_NAME " SPI flash testing\n");
	LOG_INF("==========================\n");

	flash_dev = device_get_binding(FLASH_DEVICE);

	if (!flash_dev) {
		LOG_ERR("SPI flash driver %s was not found!\n", FLASH_DEVICE);
		return;
	}

	LOG_INF("\nTest 1: Flash erase\n");

	rc = flash_erase(flash_dev, FLASH_TEST_REGION_OFFSET, FLASH_PAGE_SIZE);
	if (rc != 0) {
		LOG_ERR("Flash erase failed! %d\n", rc);
	} else {
		LOG_INF("Flash erase succeeded!\n");
	}

	LOG_INF("\nTest 2: Flash write\n");

	LOG_INF("Attempting to write %u bytes\n", len);
	rc = flash_write(flash_dev, FLASH_TEST_REGION_OFFSET, expected, len);
	if (rc != 0) {
		LOG_ERR("Flash write failed! %d\n", rc);
		return;
	}

	memset(buf, 0, len);
	rc = flash_read(flash_dev, FLASH_TEST_REGION_OFFSET, buf, len);
	if (rc != 0) {
		LOG_ERR("Flash read failed! %d\n", rc);
		return;
	}

	if (memcmp(expected, buf, len) == 0) {
		LOG_INF("Data read matches data written. Good!!\n");
	} else {
		const uint8_t *wp = expected;
		const uint8_t *rp = buf;
		const uint8_t *rpe = rp + len;

		printf("Data read does not match data written!!\n");
		while (rp < rpe) {
			LOG_ERR("%08x wrote %02x read %02x %s\n",
				(uint32_t)(FLASH_TEST_REGION_OFFSET + (rp - buf)), *wp, *rp,
				(*rp == *wp) ? "match" : "MISMATCH");
			++rp;
			++wp;
		}
	}

#endif

#ifdef TEST_ZOEM8B_GPS
	setup_gps();
#endif

#if defined(TEST_SCAN)
	nf_ble_start_scan();
#endif
#ifdef TEST_LIS2DH
	const struct device *sensor = device_get_binding(DT_LABEL(DT_INST(0, st_lis2dh)));

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
#else /* CONFIG_LIS2DH_TRIGGER */
	printf("Polling at 0.5 Hz\n");
	while (true) {
		fetch_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DH_TRIGGER */
#endif /* TEST_LIS2DH */

#ifdef TEST_ZOEM8B_GPS
	struct gps_config gps_cfg = {
		.nav_mode = GPS_NAV_MODE_PERIODIC,
		.power_mode = GPS_POWER_MODE_DISABLED,
		.timeout = 360,
		.interval = 360,
		.priority = true,
	};
	gps_start(gps_dev, &gps_cfg);
#endif
}