
#include <zephyr.h>

#ifndef X3_FW_EEPROM_H
#define X3_FW_EEPROM_H

typedef enum {
	EEP_WARN_MAX_DURATION = 0,
	EEP_WARN_MIN_DURATION,
	EEP_PAIN_CNT_DEF_ESCAPED,
	EEP_COLLAR_MODE,
	EEP_FENCE_STATUS,
	EEP_COLLAR_STATUS,
	EEP_TEACH_MODE_FINISHED,
	EEP_EMS_PROVIDER,
	EEP_PRODUCT_RECORD_REV,
	EEP_BOM_MEC_REV,
	EEP_BOM_PCB_REV,
	EEP_HW_VERSION,
	EEP_KEEP_MODE,
	EEP_RESET_REASON
} eep_uint8_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value);
int eep_uint8_write(eep_uint8_enum_t field, uint8_t value);

#endif //X3_FW_EEPROM_H
