#include <ztest.h>
#include <dfu_target.h>

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