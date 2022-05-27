#include <ztest.h>

uint32_t get_active_delta(void)
{
	return ztest_get_return_value();
}