
#include "amc_cache.h"
#include <ztest.h>

bool fnc_valid_fence(void)
{
	return ztest_get_return_value();
}
bool fnc_valid_def(void)
{
	return ztest_get_return_value();
}

Mode get_mode(void)
{
	return ztest_get_return_value();
}

FenceStatus get_fence_status(void)
{
	return ztest_get_return_value();
}

CollarStatus get_collar_status(void)
{
	return ztest_get_return_value();
}

uint16_t get_total_zap_count(void)
{
	return ztest_get_return_value();
}
