// ! Commented out since we need to write the unit tests mentioned in
// the readme file, using event_manager modules and twister !
// Difficulties by unit testing event-driven architecture is how to expect a return value
// from the given ztest assertion with passed function return value
//
//
#include <ztest.h>
#include "publisher.h"
#include "listener.h"

static void test_assert(void)
{
	zassert_true(1, "1 was false");
	zassert_false(0, "0 was true");
	zassert_is_null(NULL, "NULL was not NULL");
	zassert_not_null("foo", "\"foo\" was NULL");
	zassert_equal(1, 1, "1 was not equal to 1");
	zassert_equal_ptr(NULL, NULL, "NULL was not equal to NULL");
}

void test_main(void)
{
	ztest_test_suite(framework_tests, ztest_unit_test(test_assert));

	ztest_run_test_suite(framework_tests);
}