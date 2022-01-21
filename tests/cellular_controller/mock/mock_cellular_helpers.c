#include <ztest.h>
#include <mock_cellular_helpers.h>
#include "cellular_helpers_header.h"

uint8_t receive_tcp(struct data *data)
{
    return ztest_get_return_value();
}

//uint8_t recv(int sock, void *buf, size_t, int flags)
//{
//    return ztest_get_return_value();
//}

int dfu_target_reset(void)
{
	return ztest_get_return_value();
}

int dfu_target_mcuboot_set_buf(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

int dfu_target_img_type(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

int dfu_target_init(int result, size_t file_size, dfu_target_callback_t cb)
{
	return ztest_get_return_value();
}

int dfu_target_write(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

int dfu_target_done(bool status)
{
	return ztest_get_return_value();
}