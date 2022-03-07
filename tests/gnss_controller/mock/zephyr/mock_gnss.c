#include <ztest.h>
#include <mock_gnss.h>

#define DT_DRV_COMPAT	nofence_mock_gnss

static gnss_data_cb_t data_cb = NULL;
static gnss_lastfix_cb_t lastfix_cb = NULL;

static int mock_gnss_set_data_cb(const struct device *dev, gnss_data_cb_t
							       gnss_data_cb)
{
	ARG_UNUSED(dev);
	data_cb = gnss_data_cb;
	return 0;
}

static int mock_gnss_set_lastfix_cb(const struct device *dev, gnss_lastfix_cb_t
								  gnss_lastfix_cb)
{
	ARG_UNUSED(dev);
	lastfix_cb = gnss_lastfix_cb;
	return 0;
}

void simulate_new_gnss_last_fix(const gnss_last_fix_struct_t gnss_fix){
	lastfix_cb(&gnss_fix);
}

void simulate_new_gnss_data(const gnss_struct_t gnss_data){
	data_cb(&gnss_data);
}

static int mock_gnss_setup(const struct device *dev, bool dummy)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(dummy);
	return ztest_get_return_value();
}

static int mock_gnss_reset(const struct device *dev, uint16_t mask, uint8_t mode)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(dummy);
	return ztest_get_return_value();
}

static int mock_gnss_upload_assist_data(const struct device *dev, uint8_t*
									  data, uint32_t size)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(data);
	ARG_UNUSED(size);
	return ztest_get_return_value();
}

static int mock_gnss_set_rate(const struct device *dev, uint16_t rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(rate);
	return ztest_get_return_value();
}

static int mock_gnss_get_rate(const struct device *dev, uint16_t rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(rate);
	return ztest_get_return_value();
}

static int mock_gnss_data_fetch(const struct device *dev, gnss_struct_t* data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(data);
	return ztest_get_return_value();
}

static int mock_gnss_lastfix_fetch(const struct device *dev,
				gnss_last_fix_struct_t* lastfix)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(lastfix);
	return ztest_get_return_value();
}

static const struct gnss_driver_api mock_gnss_driver_funcs = {
	.gnss_setup = mock_gnss_setup,
	.gnss_reset = mock_gnss_reset,

	.gnss_upload_assist_data = mock_gnss_upload_assist_data,

	.gnss_set_rate = mock_gnss_set_rate,
	.gnss_get_rate = mock_gnss_get_rate,

	.gnss_set_data_cb = mock_gnss_set_data_cb,
	.gnss_set_lastfix_cb = mock_gnss_set_lastfix_cb,

	.gnss_data_fetch = mock_gnss_data_fetch,
	.gnss_lastfix_fetch = mock_gnss_lastfix_fetch
};

static int mock_gnss_init(const struct device *dev)
{
	return 0;
}

DEVICE_DT_INST_DEFINE(0, mock_gnss_init, NULL,
NULL, NULL, POST_KERNEL,
90, &mock_gnss_driver_funcs);