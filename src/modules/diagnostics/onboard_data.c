

#include "onboard_data.h"
#include "gnss.h"
#include "modem_nf.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(oboard_data, 4);

static gnss_struct_t ob_gnss_data;
static gsm_info ob_gsm_data;
static onboard_data_struct_t ob_data;
static onboard_all_data_struct_t ob_all_data;


int onboard_data_init(void)
{
    memset(&ob_gnss_data, 0, sizeof(gnss_struct_t));
    memset(&ob_data, 0, sizeof(onboard_data_struct_t));
    memset(&ob_all_data, 0, sizeof(onboard_all_data_struct_t));
	
    return 0;
}


int onboard_set_gnss_data(gnss_struct_t gnss_data_in)
{
    memcpy(&ob_gnss_data, &gnss_data_in, sizeof(gnss_data_in));
    ob_data.num_sv = ob_gnss_data.num_sv;
    ob_data.avg_cno = ob_gnss_data.cno[3];

    return 0;
}

int onboard_set_gsm_data(gsm_info gsm_data_in)
{
    memcpy(&ob_gsm_data, &gsm_data_in, sizeof(gsm_data_in));
    ob_data.rssi = ob_gsm_data.rssi;

    return 0;
}


int onboard_set_power_data(uint8_t pwr_state, uint16_t battery_mv, uint16_t charging_ma)
{
    ob_data.pwr_state = pwr_state;
    ob_data.battery_mv = battery_mv;
    ob_data.charging_ma = charging_ma;

    return 0;
}


int onboard_set_env_sens_data(double temp, double humidity, double pressure)
{
    ob_data.temp = temp;
    ob_data.humidity = humidity;
    ob_data.pressure = pressure;

	return 0;
}


int onboard_get_gnss_data(gnss_struct_t **gnss_data_out)
{
    *gnss_data_out = &ob_gnss_data;

    return 0;
}

int onboard_get_gsm_data(gsm_info **gsm_data_out)
{
    *gsm_data_out = &ob_gsm_data;

    return 0;
}



int onboard_get_data(onboard_data_struct_t **ob_data_out)
{
    *ob_data_out = &ob_data;

    return 0;
}


int onboard_get_all_data(onboard_all_data_struct_t **ob_all_data_out)
{
    ob_all_data.num_sv = ob_gnss_data.num_sv;
    ob_all_data.cno_4[0] = ob_gnss_data.cno[0];
    ob_all_data.cno_4[1] = ob_gnss_data.cno[1];
    ob_all_data.cno_4[2] = ob_gnss_data.cno[2];
    ob_all_data.cno_4[3] = ob_gnss_data.cno[3];
    ob_all_data.pvt_flags = ob_gnss_data.pvt_flags;
    ob_all_data.pvt_valid = ob_gnss_data.pvt_valid;
    ob_all_data.overflow = ob_gnss_data.overflow;
    ob_all_data.height = ob_gnss_data.height;
    ob_all_data.speed = ob_gnss_data.speed;
    ob_all_data.h_dop = ob_gnss_data.h_dop;
    ob_all_data.lat = ob_gnss_data.lat;
    ob_all_data.lon = ob_gnss_data.lon;
    ob_all_data.msss = ob_gnss_data.msss;
    ob_all_data.ttff = ob_gnss_data.ttff;
    ob_all_data.vbatt = ob_data.battery_mv;
    ob_all_data.isolar = ob_data.charging_ma;
    ob_all_data.vint_stat = 0;
    ob_all_data.acc_x = 0;
    ob_all_data.acc_y = 0;
    ob_all_data.acc_z = 0;
    ob_all_data.bme280_temp = ob_data.temp; 
    ob_all_data.bme280_pres = ob_data.pressure;
    ob_all_data.bme280_hum = ob_data.humidity;

    *ob_all_data_out = &ob_all_data;

    return 0;
}

