/**
 *  Created by janni on 3/19/2018.
 *  Migrated to zephyr 3/17/2022.
 */

#include <zephyr.h>

#ifndef NOFENCE_CRC16_H
#define NOFENCE_CRC16_H

uint16_t nf_crc16_uint32(uint32_t data, uint16_t const *p_crc);
uint16_t nf_crc16_uint16(uint16_t data, uint16_t const *p_crc);

uint16_t nf_crc16_buf(uint8_t *p_buf, size_t buf_size);

#endif //NOFENCE_CRC16_H
