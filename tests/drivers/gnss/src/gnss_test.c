#include <ztest.h>
#include <device.h>
#include "gnss.h"

static void test_data(void)
{
	int ret = 0;
	
	const struct device *gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));
	(void)gnss_dev;
	ret = gnss_setup(gnss_dev, false);

	zassert_equal(ret, 0, "Expected OK, but failed...");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_data));
	ztest_run_test_suite(common);
}