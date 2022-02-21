#include <ztest.h>
#include "gnss.h"

static void test_data(void)
{
	zassert_equal(1, 1, "Expected");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_data));
	ztest_run_test_suite(common);
}