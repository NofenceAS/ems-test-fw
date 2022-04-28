#include <ztest.h>
#include "nf_crc16.h"

uint16_t nf_crc16_uint32(uint32_t data, uint16_t const *p_crc)
{
	return ztest_get_return_value();
}

uint16_t nf_crc16_uint16(uint16_t data, uint16_t const *p_crc)
{
	return ztest_get_return_value();
}

uint16_t nf_crc16_buf(uint8_t *p_buf, size_t buf_size)
{
	return ztest_get_return_value();
}