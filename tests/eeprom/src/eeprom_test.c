#include <ztest.h>
#include <nf_settings.h>
#include <drivers/eeprom.h>

static int mock_eeprom_read(const struct device *dev, off_t offset, void *data,
			    size_t len)
{
	ztest_check_expected_value(offset);
	ztest_check_expected_value(len);
	ztest_copy_return_data(data, len);

	return ztest_get_return_value();
}

static int mock_eeprom_write(const struct device *dev, off_t offset,
			     const void *data, size_t len)
{
	ztest_check_expected_value(offset);
	ztest_check_expected_value(len);
	ztest_check_expected_data(data, len);
	return ztest_get_return_value();
}

static size_t mock_eeprom_size(const struct device *dev)
{
	return ztest_get_return_value();
}

struct eeprom_driver_api mock_api = { .read = mock_eeprom_read,
				      .write = mock_eeprom_write,
				      .size = mock_eeprom_size };

const struct device mock_eeprom_device = { .api = &mock_api };

static void test_serial_number(void)
{
	int ret;
	eep_init(&mock_eeprom_device);
	uint32_t expected_data = UINT32_MAX;
	/* Write: Happy scenario */
	ztest_expect_value(mock_eeprom_write, offset, 0);
	ztest_expect_value(mock_eeprom_write, len, 4);
	ztest_expect_data(mock_eeprom_write, data, &expected_data);
	ztest_returns_value(mock_eeprom_write, 0);
	ret = eep_uint32_write(EEP_UID, UINT32_MAX);
	zassert_equal(ret, 0, "eep_uint32_write should return 0");

	/* Read: Happy scenario */
	ztest_expect_value(mock_eeprom_read, offset, 0);
	ztest_expect_value(mock_eeprom_read, len, 4);
	expected_data = 1234;
	ztest_return_data(mock_eeprom_read, data, &expected_data);
	ztest_returns_value(mock_eeprom_read, 0);
	uint32_t serial;
	ret = eep_uint32_read(EEP_UID, &serial);
	zassert_equal(ret, 0, "eep_uint32_read should return 0");
	zassert_equal(serial, 1234, "Expected serial number");
}

static void test_host_port(void)
{
	int ret;
	eep_init(&mock_eeprom_device);
	char host_port[EEP_HOST_PORT_BUF_SIZE];
	char host_port_ret[EEP_HOST_PORT_BUF_SIZE];
	char host_port_too_large[EEP_HOST_PORT_BUF_SIZE + 1];

	memset(host_port, 'a', EEP_HOST_PORT_BUF_SIZE);
	host_port[EEP_HOST_PORT_BUF_SIZE - 1] = '\0';
	memset(host_port_too_large, 'a', EEP_HOST_PORT_BUF_SIZE + 1);
	host_port_too_large[EEP_HOST_PORT_BUF_SIZE] = '\0';

	/* Write : Happy scenario */
	ztest_expect_value(mock_eeprom_write, offset, 4);
	ztest_expect_value(mock_eeprom_write, len, EEP_HOST_PORT_BUF_SIZE);
	ztest_expect_data(mock_eeprom_write, data, host_port);
	ztest_returns_value(mock_eeprom_write, 0);
	ret = eep_write_host_port(host_port);
	zassert_equal(ret, 0, "eep_write_host_port should return 0");

	/* Write: Too large string */
	ret = eep_write_host_port(host_port_too_large);
	zassert_equal(ret, -EOVERFLOW,
		      "eep_write_host_port should return error");

	/* Read: Happy scenario */
	ztest_expect_value(mock_eeprom_read, offset, 4);
	ztest_expect_value(mock_eeprom_read, len, EEP_HOST_PORT_BUF_SIZE);
	ztest_return_data(mock_eeprom_read, data, host_port_ret);
	strcpy(host_port_ret, "193.333.555.777:123456");
	ztest_returns_value(mock_eeprom_read, 0);
	ret = eep_read_host_port(host_port, EEP_HOST_PORT_BUF_SIZE);
	zassert_equal(ret, 0, "should return 0");
	zassert_mem_equal(host_port, "193.333.555.777:123456", 23,
			  "Expected host port");
	/* Read, too small buffer */
	ret = eep_read_host_port(host_port, EEP_HOST_PORT_BUF_SIZE - 1);
	zassert_equal(ret, -EOVERFLOW, "unexpected return");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_serial_number),
			 ztest_unit_test(test_host_port));
	ztest_run_test_suite(common);
}