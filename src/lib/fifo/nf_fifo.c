/**
 *
 * Name         :  nf_fifo.c
 *
 * Description  :  This is the functions for Nofence fifo handling.
 *
 * Author       :  Oscar Hovde Berntsen <oscar@nofence.no>
 * Website      :  http://www.nofence.no
 *
 * Credit       :  Oscar Hovde Berntsen (2014) 
 *
 * License      :  Copyright 2015 Nofence AS
 *                 All rights reserved
 *
 * Compiler     :  WinAVR, GCC for AVR platform 
 *                 Tested version :
 *                 - 20100110 (avr-libc 1.x)
 * Compiler note:  Please be aware of using older/newer version since WinAVR 
 *                 is in extensive development. Please compile with parameter -O1 
 *
 * History 
 * Oscar: (May, 2014)
 * 
 * @note Migrated to Zephyr 25.03.2022
 */

#include <zephyr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "nf_fifo.h"

void fifo_put(int16_t in, int16_t *fx_array, uint8_t size)
{
	for (uint8_t i = (size - 1); i > 0; i--) {
		fx_array[i] = fx_array[i - 1];
	}
	fx_array[0] = in;
}

void fifo_fill(int16_t *fx_array, uint8_t size)
{
	for (uint8_t i = 1; i < size; i++) {
		fx_array[i] = fx_array[0];
	}
}

int16_t fifo_max(int16_t *fx_array, uint8_t size)
{
	int16_t tmp_max = INT16_MIN;

	for (uint8_t i = 0; i < size; i++) {
		tmp_max = MAX(tmp_max, fx_array[i]);
	}
	return tmp_max;
}

int16_t fifo_min(int16_t *fx_array, uint8_t size)
{
	int16_t tmp_min = INT16_MAX;

	for (uint8_t i = 0; i < size; i++) {
		tmp_min = MIN(tmp_min, fx_array[i]);
	}
	return tmp_min;
}

int16_t fifo_delta(int16_t *fx_array, uint8_t size)
{
	return fifo_max(fx_array, size) - fifo_min(fx_array, size);
}

int16_t fifo_avg(int16_t *fx_array, uint8_t size)
{
	int32_t my_avg = 0;

	if (size == 0) {
		return fx_array[0];
	}

	for (uint8_t i = 0; i < size; i++) {
		my_avg += fx_array[i];
	}

	my_avg = my_avg / size;
	return (int16_t)my_avg;
}

int16_t fifo_slope(int16_t *fx_array, uint8_t size)
{
	int32_t my_slope_rate;

	/* Slope is calculated as double of difference 
         * between newest value and average value.
         */
	my_slope_rate = ((fx_array[0] - fifo_avg(fx_array, size)) << 1);

	if (my_slope_rate < INT16_MIN) {
		return INT16_MIN;
	}

	if (my_slope_rate > INT16_MAX) {
		return INT16_MAX;
	}

	return (int16_t)my_slope_rate;
}

uint8_t fifo_inc_cnt(int16_t *fx_array, uint8_t size)
{
	uint8_t my_counter = 0;
	for (uint8_t i = (size - 1); i > 0; i--) {
		if (fx_array[i] < fx_array[i - 1]) {
			my_counter++;
		}
	}
	return my_counter;
}

uint8_t fifo_dec_cnt(int16_t *fx_array, uint8_t size)
{
	uint8_t my_counter = 0;
	for (uint8_t i = (size - 1); i > 0; i--) {
		if (fx_array[i] > fx_array[i - 1]) {
			my_counter++;
		}
	}
	return my_counter;
}