/**
 *
 * Name         :  trigonometry.h
 *
 * Description  :  
 *
 * Author       :  Oscar Hovde Berntsen <oscar@nofence.no>
 * Website      :  http://nofence.no
 *
 * Credit       :  Oscar Hovde Berntsen (2012)
 *
 * License      :  Copyright 2015 Nofence AS
 *                 All rights reserved
 *
 * History      :  
 * Please refer to trigonometry.c
 * 
 * @note Migrated to Zephyr 3/17/2022
 */

#ifndef _TRIGONOMETRY_H_
#define _TRIGONOMETRY_H_

#include <zephyr.h>

#define Deg90 9000
#define Deg180 (Deg90 << 1)
#define Deg360 (Deg180 << 1)

/* Function to calculate ir = iy / ix with iy <= ix, and ix, iy both > 0. */
int16_t g_i16_Divide(int16_t iy, int16_t ix);

/* Function to calculate ir = ix / sqrt(ix*ix+iy*iy) using binary division. */
int16_t g_i16_Trig(int16_t ix, int16_t iy);

/* The function iHundredAtan2Deg is a wrapper function which implements the ATAN2 function by
 * assigning the results of an ATAN function to the correct quadrant. The result is the angle
 * in degrees times 100.
 */
int16_t g_i16_HundredAtan2Deg(int16_t iy, int16_t ix);

/* Calculates smallest sector between two angles. In the range +-18000*/
int16_t g_i16_AngleBetween(int16_t Alpha, int16_t Betha);

/*. Fast Square root algorithm, with rounding. */
uint32_t g_u32_SquareRootRounded(uint32_t a_nInput);

#endif /* END OF trigonometry.h. */
