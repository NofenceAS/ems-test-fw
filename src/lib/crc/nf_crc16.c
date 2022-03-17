/**
 *  Created by janni on 3/19/2018.
 *  Migrated to zephyr 3/17/2022.
 */

#include <zephyr.h>
#include "nf_crc16.h"

uint16_t _crc16_update(uint16_t crc, uint8_t a)
{
	int i;
	crc ^= a;
	for (i = 0; i < 8; ++i) {
		if (crc & 1)
			crc = (crc >> 1) ^ 0xA001;
		else
			crc = (crc >> 1);
	}
	return crc;
}

uint16_t nf_crc16_uint32(uint32_t data, uint16_t const *p_crc)
{
	uint16_t crc = (p_crc == NULL) ? 0xFFFF : *p_crc;

	crc = _crc16_update(crc, (uint8_t)(data));
	crc = _crc16_update(crc, (uint8_t)(data >> 8));
	crc = _crc16_update(crc, (uint8_t)(data >> 16));
	crc = _crc16_update(crc, (uint8_t)(data >> 24));

	return crc;
}

uint16_t nf_crc16_uint16(uint16_t data, uint16_t const *p_crc)
{
	uint16_t crc = (p_crc == NULL) ? 0xFFFF : *p_crc;

	crc = _crc16_update(crc, (uint8_t)(data));
	crc = _crc16_update(crc, (uint8_t)(data >> 8));

	return crc;
}

uint16_t nf_crc16_buf(uint8_t *p_buf, size_t buf_size)
{
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < buf_size; i++) {
		crc = _crc16_update(crc, p_buf[i]);
	}
	// 0xFFFF means unititialized in this context.
	if (crc == 0xFFFF) {
		crc--;
	}
	return crc;
}
