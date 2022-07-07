
#include <zephyr.h>
#ifndef X3_FW_EEPROM_H
#define X3_FW_EEPROM_H

typedef enum { EEP_UID = 0, EEP_WARN_CNT_TOT } eep_uint32_enum_t;

typedef enum {
	EEP_ACC_SIGMA_NOACTIVITY_LIMIT = 0,
	EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
	EEP_ACC_SIGMA_SLEEP_LIMIT,
	EEP_ZAP_CNT_TOT,
	EEP_ZAP_CNT_DAY,
	EEP_PRODUCT_TYPE,
} eep_uint16_enum_t;

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
} eep_uint8_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24
#define EEP_BLE_SEC_KEY_LEN 8

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value);
int eep_uint8_write(eep_uint8_enum_t field, uint8_t value);

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value);
int eep_uint16_write(eep_uint16_enum_t field, uint16_t value);

int eep_uint32_read(eep_uint32_enum_t field, uint32_t *value);
int eep_uint32_write(eep_uint32_enum_t field, uint32_t value);

/**
 * @brief writes the ble_security key to persisted storage
 * @param[in] ble_sec_key pointer to security key array.
 * @param[in] bufsize length of data to be written
 *
 * @return 0 on success, otherwise negative errror code
 */
int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);

/**
 * @brief reads the ble_security key from persisted storage
 * @param[in] ble_sec_key pointer to security key array.
 * @param[in] bufsize length of data to be written
 *
 * @return 0 on success, otherwise negative errror code
 */
int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);

#endif //X3_FW_EEPROM_H
