

#include "onboard_data.h"
#include "gnss.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(oboard_data, 4);

static gnss_struct_t ob_gnss_data;
static onboard_data_struct_t ob_data;
static onboard_sens_data_struct_t ob_sens_data;


int onboard_data_init(void)
{
    memset(&ob_gnss_data, 0, sizeof(gnss_struct_t));
    memset(&ob_data, 0, sizeof(onboard_data_struct_t));
    memset(&ob_sens_data, 0, sizeof(onboard_sens_data_struct_t));
	
    return 0;
}


int onboard_set_gnss_data(gnss_struct_t gnss_data_in)
{
    memcpy(&ob_gnss_data, &gnss_data_in, sizeof(gnss_data_in));
    ob_sens_data.num_sv = ob_gnss_data.num_sv;
    ob_sens_data.avg_cno = ob_gnss_data.cno[3];

    return 0;
}

int onboard_get_gnss_data(gnss_struct_t **gnss_data_out)
{
    *gnss_data_out = &ob_gnss_data;

    return 0;
}

int onboard_get_sens_data(onboard_sens_data_struct_t **sens_data_out)
{
    *sens_data_out = &ob_sens_data;

    return 0;
}

int onboard_get_data(onboard_data_struct_t **onboard_data_out)
{
    *onboard_data_out = &ob_sens_data;

    return 0;
}

int onboard_set_battery_data(uint16_t battery_mv)
{
    ob_sens_data.battery_mv = battery_mv;

    return 0;
}

int onboard_set_charging_data(uint16_t charging_ma)
{
    ob_sens_data.charging_ma = charging_ma;

    return 0;
}

int onboard_set_env_sens_data(double temp, double humidity, double pressure)
{
    ob_sens_data.temp = temp;
    ob_sens_data.humidity = humidity;
    ob_sens_data.pressure = pressure;

	return 0;
}