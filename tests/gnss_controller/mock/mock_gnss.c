#include <ztest.h>
#include <mock_gnss.h>
#include "gnss.h"

static gnss_data_cb_t data_cb = NULL;
static gnss_lastfix_cb_t lastfix_cb = NULL;

static int gnss_set_data_cb(const struct device *dev, gnss_data_cb_t gnss_data_cb)
{
	ARG_UNUSED(dev);
	data_cb = gnss_data_cb;
	return 0;
}

void simulate_new_gnss_data(const gnss_struct_t gnss_data){
	data_cb(&gnss_data);
}

static int gnss_set_lastfix_cb(const struct device *dev, gnss_lastfix_cb_t gnss_lastfix_cb)
{
	ARG_UNUSED(dev);
	lastfix_cb = gnss_lastfix_cb;
	return 0;
}

void simulate_new_gnss_last_fix(const gnss_last_fix_struct_t gnss_fix){
	lastfix_cb(&gnss_fix);
}

