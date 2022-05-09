#include <ztest.h>
#include "watchdog_app.h"

int watchdog_init_and_start(void)
{
	return ztest_get_return_value();
}