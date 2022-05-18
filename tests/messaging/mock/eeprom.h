
#include <zephyr.h>
#ifndef _EEPROM_H
#define _EEPROM_H

int eep_read_serial(uint32_t *);

int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);

int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize);

#endif /* _EEPROM_H */
