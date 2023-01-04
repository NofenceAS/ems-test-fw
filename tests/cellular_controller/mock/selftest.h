/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _SELFTEST_H_
#define _SELFTEST_H_

#include <stdbool.h>
#include <stdint.h>

/** @brief Bit position for tested modules */
#define SELFTEST_FLASH_POS 0
#define SELFTEST_EEPROM_POS 1
#define SELFTEST_ACCELEROMETER_POS 2
#define SELFTEST_GNSS_POS 3
#define SELFTEST_MODEM_POS 4

int selftest_mark_state(uint8_t selftest_pos, bool passed);

#endif /* _SELFTEST_H_ */