
#include <zephyr.h>
#ifndef X3_FW_EEPROM_H
#define X3_FW_EEPROM_H

typedef enum { EEP_UID = 0, EEP_WARN_CNT_TOT } eep_uint32_enum_t;

typedef enum {
	EEP_ACC_SIGMA_NOACTIVITY_LIMIT = 0,
	EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
	EEP_ACC_SIGMA_SLEEP_LIMIT,
	EEP_ZAP_CNT_TOT,
	EEP_ZAP_CNT_DAY
} eep_uint16_enum_t;

typedef enum {
	EEP_WARN_MAX_DURATION = 0,
	EEP_WARN_MIN_DURATION,
	EEP_PAIN_CNT_DEF_ESCAPED,
	EEP_COLLAR_MODE,
	EEP_FENCE_STATUS,
	EEP_COLLAR_STATUS,
	EEP_TEACH_MODE_FINISHED
} eep_uint8_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24

int eep_uint32_read(eep_uint32_enum_t field, uint32_t *value);
int eep_uint32_write(eep_uint32_enum_t field, uint32_t value);

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value);
int eep_uint16_write(eep_uint16_enum_t field, uint16_t value);

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value);
int eep_uint8_write(eep_uint8_enum_t field, uint8_t value);

#endif //X3_FW_EEPROM_H
