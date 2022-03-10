#include <ztest.h>
#include "storage.h"

int stg_init_storage_controller(void)
{
	return ztest_get_return_value();
}

int stg_clear_partition(flash_partition_t partition)
{
	return ztest_get_return_value();
}

int stg_fcb_reset_and_init()
{
	return ztest_get_return_value();
}

int stg_read_log_data(fcb_read_cb cb, uint16_t num_entries)
{
	return ztest_get_return_value();
}

int stg_read_ano_data(fcb_read_cb cb, uint16_t num_entries)
{
	return ztest_get_return_value();
}

int stg_read_pasture_data(fcb_read_cb cb)
{
	return ztest_get_return_value();
}

int stg_write_log_data(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

int stg_write_ano_data(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

int stg_write_pasture_data(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}

uint32_t get_num_entries(flash_partition_t partition)
{
	return ztest_get_return_value();
}

bool stg_log_pointing_to_last()
{
	return ztest_get_return_value();
}