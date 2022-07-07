/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef TEST_MOCK_EEPROM_H
#define TEST_MOCK_EEPROM_H

#include <zephyr.h>

typedef enum {
	EEP_ACC_SIGMA_NOACTIVITY_LIMIT = 0,
	EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
	EEP_ACC_SIGMA_SLEEP_LIMIT,
	EEP_ZAP_CNT_TOT,
	EEP_ZAP_CNT_DAY,
	EEP_PRODUCT_TYPE,
} eep_uint16_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value);

#endif /* TEST_MOCK_EEPROM_H_ */
