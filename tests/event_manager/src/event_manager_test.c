/* Include example module that contain events and run tests on them */
#include <ztest.h>

static void test_equal(void)
{
	zassert_equal(1, 1, "equal");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_equal), );
	ztest_run_test_suite(common);
}