
#include <zephyr.h>
#ifndef X3_FW_EEPROM_H
#define X3_FW_EEPROM_H

typedef enum { EEP_UID = 0, EEP_WARN_CNT_TOT } eep_uint32_enum_t;

#define EEP_HOST_PORT_BUF_SIZE 24
#define EEP_BLE_SEC_KEY_LEN 8

int eep_uint32_read(eep_uint32_enum_t field, uint32_t *value);
int eep_uint32_write(eep_uint32_enum_t field, uint32_t value);

int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);
int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);

#endif //X3_FW_EEPROM_H
