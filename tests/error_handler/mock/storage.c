#include <ztest.h>
#include "storage.h"

int stg_write_system_diagnostic_log(uint8_t *data, size_t len)
{
	return ztest_get_return_value();
}