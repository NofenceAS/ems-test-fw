
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

static int gnss_set_data_cb(const struct device *, gnss_data_cb_t);

void simulate_new_gnss_data(const gnss_struct_t );

static int gnss_set_lastfix_cb(const struct device *, gnss_lastfix_cb_t);

void simulate_new_gnss_last_fix(const gnss_last_fix_struct_t);
