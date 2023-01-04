/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ONBOARD_DATA_H_
#define _ONBOARD_DATA_H_

#include <zephyr.h>
#include "gnss.h"

/** @brief Struct containing onboard data. */
typedef struct {
	/** Number of SVs used in Nav Solution */
	uint8_t num_sv;
	/** avarage CNO  */
	uint8_t avg_cno;
	/** contain vbatt value*/
	uint16_t battery_mv;
	/** Contain solar charging value*/
	uint16_t charging_ma;
	/** Contain BME280 Temp value*/
	double temp;	
	/** Contain BME280 hum value*/
	double humidity;			
	/** Contain BME280 Press value*/
	double pressure;	
} onboard_data_struct_t;



int onboard_data_init(void);

int onboard_set_gnss_data(gnss_struct_t gnss_data_in);
int onboard_get_gnss_data(gnss_struct_t **gnss_data_out);

int onboard_get_data(onboard_data_struct_t **ob_data_out);

int onboard_set_battery_data(uint16_t battery_mv);
int onboard_set_charging_data(uint16_t charging_ma);
int onboard_set_env_sens_data(double temp, double humidity, double pressure);

#endif