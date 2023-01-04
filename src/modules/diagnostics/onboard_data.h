/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _ONBOARD_DATA_H_
#define _ONBOARD_DATA_H_

#include <zephyr.h>
#include "gnss.h"
#include "modem_nf.h"
#include "pwr_event.h"

/** @brief on nboard data struct */
typedef struct {
	/** num satelites from NAV-SAT */
	uint8_t num_sv;
	/** avarage CNO from NAV-SAT */
	uint8_t avg_cno;
	/** rssi */
	int32_t rssi;
	/** power status */
	uint8_t pwr_state;
	/** battery mv */
	uint16_t battery_mv;
	/** charging ma */
	uint16_t charging_ma;
	/** BME280 temperature */
	double temp;
	/** BME280 humidity */
	double humidity;
	/** BME280 pressure */
	double pressure;
} onboard_data_struct_t;

/** @brief onboard data for 902 backwards compatibility
 */
typedef struct {
	/** Number of SVs used in Nav Solution.*/
	uint8_t num_sv;
	/** CNO for 4 sat id.*/
	uint8_t cno_4[4];
	/** UBX-NAV-PVT flags as copied.*/
	uint8_t pvt_flags;
	/** UBX-NAV-PVT valid flags as copies.*/
	uint8_t pvt_valid;
	/** Set if overflow because of too far away from origin position.*/
	uint8_t overflow;
	/** Height above ellipsoid [dm].*/
	int16_t height;
	/** 2-D speed [cm/s]*/
	uint16_t speed;
	/** Horizontal dilution of precision.*/
	uint16_t h_dop;
	/** Latitude.*/
	int32_t lat;
	/** Longitude.*/
	int32_t lon;
	/** UBX-NAV-STATUS milliseconds since receiver start or reset.*/
	uint32_t msss;
	/** UBX-NAV-STATUS milliseconds since First Fix.*/
	uint32_t ttff;
	/** contain vbatt value*/
	uint32_t vbatt; //4byte
	/** Contain solar charging value*/
	uint32_t isolar; //4byte
	/** Contain solar charging value*/
	uint32_t vint_stat; //1byte
	/** Contain Accelerometerdata X value*/
	int32_t acc_x; //4byte
	/** Contain Accelerometerdata Y value*/
	int32_t acc_y; //4byte
	/** Contain Accelerometerdata Z value*/
	int32_t acc_z; //4byte
	/** Contain BME280 Temp value*/
	int32_t bme280_temp; //4byte
	/** Contain BME280 Press value*/
	int32_t bme280_pres; //4byte
	/** Contain BME280 hum value*/
	int32_t bme280_hum; //4byte
} onboard_all_data_struct_t;

int onboard_data_init(void);

int onboard_set_gnss_data(gnss_struct_t gnss_data_in);
int onboard_set_gsm_data(gsm_info gsm_data_in);
int onboard_set_power_data(uint8_t pwr_state, uint16_t battery_mv, uint16_t charging_ma);
int onboard_set_env_sens_data(double temp, double humidity, double pressure);

int onboard_get_data(onboard_data_struct_t **ob_data_out);
int onboard_get_gnss_data(gnss_struct_t **gnss_data_out);
int onboard_get_gsm_data(gsm_info **gsm_data_out);

int onboard_get_all_data(onboard_all_data_struct_t **ob_all_data_out);

#endif