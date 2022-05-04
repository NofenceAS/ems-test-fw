
#include <zephyr.h>
#ifndef X3_FW_EEPROM_H
#define X3_FW_EEPROM_H

typedef enum { EEP_UID = 0, EEP_WARN_CNT_TOT } eep_uint32_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24
#define EEP_BLE_SEC_KEY_LEN 8

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
