#include <ztest.h>
#include "gnss.h"
#include "mock_uart.h"
#include <device.h>
#include "ublox-mia-m10.h"

uint32_t mock_api = 0x133755AA;

struct device_state dev_state = { .init_res = 0,
				.initialized = true};

struct device unit_test_uart_dev = { .api = &mock_api,
				     .state = &dev_state
};

static void test_data(void)
{
	int ret;
	
	ztest_returns_value(uart_fifo_read, 0);

	struct device_state gnss_state;
	struct device gnss_dev = {
		.api = NULL,
		.state = &gnss_state,
	};
	ret = mia_m10_unit_test_init(&gnss_dev);

	//ret = gnss_setup(gnss_dev, true);

	zassert_equal(ret, 0, "Expected OK, but failed...");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_data));
	ztest_run_test_suite(common);
}