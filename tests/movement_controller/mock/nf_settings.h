
#include <zephyr.h>
#ifndef X3_FW_EEPROM_MOCK_H
#define X3_FW_EEPROM_MOCK_H

typedef enum {
	EEP_ACC_SIGMA_NOACTIVITY_LIMIT = 0,
	EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
	EEP_ACC_SIGMA_SLEEP_LIMIT
} eep_uint16_enum_t;

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value);
int eep_uint16_write(eep_uint16_enum_t field, uint16_t value);

#endif //X3_FW_EEPROM_MOCK_H
