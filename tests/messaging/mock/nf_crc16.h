/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _NF_CRC16_MOCK_H_
#define _NF_CRC16_MOCK_H_

#include <zephyr.h>

uint16_t nf_crc16_uint32(uint32_t data, uint16_t *crc);

uint16_t nf_crc16_uint16(uint16_t data, uint16_t *crc);

#endif /* _NF_CRC16_MOCK_H_ */