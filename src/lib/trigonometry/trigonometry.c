/**
 *
 * Name         :  trigonometry.c
 *
 * Description  :  This is trigonometry functions using only 16 bit integers. Is used by accelerometer and compass aritmetics
 *                 it is based upon a freescale document with doc number AN4248.
 *
 * Author       :  Oscar Hovde Berntsen <oscar@nofence.no>
 * Website      :  http://www.nofence.no
 *
 * Credit       :  Oscar Hovde Berntsen (2012) 
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
 * Oscar: (Nov, 2012)
 * 
 * @note Migrated to Zephyr 3/17/2022
 */

#include <zephyr.h>
#include "trigonometry.h"

// MODULE VARIABLES

/*
The function iDivide is an accurate integer division function where it is given that both the
numerator and denominator are non-negative, non-zero and where the denominator is greater than
the numerator. The result is in the range 0 decimal to 32767 decimal which is interpreted in
Q15 fractional arithmetic as the range 0.0 to 0.9999695.
*/

/*The accuracy is determined by the threshold MINDELTADIV. The setting for maximum accuracy is
MINDELTADIV = 1.*/
const uint16_t MINDELTADIV = 1; /* final step size for iDivide */

/* function to calculate ir = iy / ix with iy <= ix, and ix, iy both > 0 */
int16_t g_i16_Divide(int16_t iy, int16_t ix)
{
	int32_t itmp; /* scratch */
	int16_t ir; /* result = iy / ix range 0., 1. returned in range 0 to 32767 */
	int16_t idelta; /* delta on candidate result dividing each stage by factor of 2 */

	/* set result r to zero and binary search step to 16384 = 0.5 */
	ir = 0;
	idelta = 16384; /* set as 2^14 = 0.5 */

	/* to reduce quantization effects, boost ix and iy to the maximum signed 16 bit value */
	while ((ix < 16384) && (iy < 16384)) {
		ix += ix;
		iy += iy;
	}

	/* loop over binary sub-division algorithm solving for ir*ix = iy */
	do {
		/* generate new candidate solution for ir and test if we are too high or too low */
		itmp = ir + idelta; /* itmp=ir+delta, the candidate solution */
		itmp *= ix;
		itmp = itmp >> 15;
		if (itmp <= iy)
			ir += idelta;
		idelta =
			idelta >> 1; /* divide by 2 using right shift one bit */
	} while (
		idelta >=
		MINDELTADIV); /* last loop is performed for idelta=MINDELTADIV */

	return ir;
}

/*The accuracy is determined by the threshold MINDELTATRIG. The setting for maximum accuracy is
MINDELTATRIG = 1.*/
const uint16_t MINDELTATRIG = 1; /* final step size for iTrig */

/* function to calculate ir = ix / sqrt(ix*ix+iy*iy) using binary division */
int16_t g_i16_Trig(int16_t ix, int16_t iy)
{
	uint32_t itmp; /* scratch */
	uint32_t ixsq; /* ix * ix */
	int16_t isignx; /* storage for sign of x. algorithm assumes x >= 0 then corrects later */
	uint32_t ihypsq; /* (ix * ix) + (iy * iy) */
	int16_t ir; /* result = ix / sqrt(ix*ix+iy*iy) range -1, 1 returned as signed int16_t */
	int16_t idelta; /* delta on candidate result dividing each stage by factor of 2 */

	/* stack variables */
	/* ix, iy: signed 16 bit integers representing sensor reading in range -32768 to 32767 */
	/* function returns signed int16_t as signed fraction (ie +32767=0.99997, -32768=-1.0000) */
	/* algorithm solves for ir*ir*(ix*ix+iy*iy)=ix*ix */

	/* correct for pathological case: ix==iy==0 */
	if ((ix == 0) && (iy == 0))
		ix = iy = 1;

	/* check for -32768 which is not handled correctly */
	if (ix == -32768)
		ix = -32767;
	if (iy == -32768)
		iy = -32767;

	/* store the sign for later use. algorithm assumes x is positive for convenience */
	isignx = 1;
	if (ix < 0) {
		ix = -ix;
		isignx = -1;
	}

	/* for convenience in the boosting set iy to be positive as well as ix */
	iy = abs(iy);

	/* to reduce quantization effects, boost ix and iy but keep below maximum signed 16 bit */
	while ((ix < 16384) && (iy < 16384)) {
		ix += ix;
		iy += iy;
	}

	/* calculate ix*ix and the hypotenuse squared */
	ixsq = (uint32_t)ix * ix; /* ixsq=ix*ix: 0 to 32767^2 = 1073676289 */
	ihypsq = ixsq +
		 ((uint32_t)iy *
		  iy); /* ihypsq=(ix*ix+iy*iy) 0 to 2*32767*32767=2147352578 */

	/* set result r to zero and binary search step to 16384 = 0.5 */
	ir = 0;
	idelta = 16384; /* set as 2^14 = 0.5 */

	/* loop over binary sub-division algorithm */
	do {
		/* generate new candidate solution for ir and test if we are too high or too low */
		/* itmp=(ir+delta)^2, range 0 to 32767*32767 = 2^30 = 1073676289 */
		itmp = (uint32_t)ir + idelta;
		itmp *= itmp;
		/* itmp=(ir+delta)^2*(ix*ix+iy*iy), range 0 to 2^31 = 2147221516 */
		itmp = (itmp >> 15) * (ihypsq >> 15);
		if (itmp <= ixsq)
			ir += idelta;
		idelta =
			idelta >> 1; /* divide by 2 using right shift one bit */
	} while (
		idelta >=
		MINDELTATRIG); /* last loop is performed for idelta=MINDELTATRIG */

	/* correct the sign before returning */
	return ir * isignx;
}

/* The function iHundredAtanDeg computes the function for X and Y in the range 0 to 32767
 * (interpreted as 0.0 to 0.9999695 in Q15 fractional arithmetic) outputting the angle in 
 * degrees * 100 in the range 0 to 9000 (0.0° to 90.0°).
 * For Y? X the output angle is in the range 0° to 45° 
 */

/* fifth order of polynomial approximation giving 0.05 deg max error */
const int16_t K1 = 5701;
const int16_t K2 = -1645;
const int16_t K3 = 446;

/* calculates 100*atan(iy/ix) range 0 to 9000 for all ix, iy positive in range 0 to 32767 */
int16_t g_i16_HundredAtanDeg(int16_t iy, int16_t ix)
{
	int32_t iAngle; /* angle in degrees times 100 */
	int16_t iRatio; /* ratio of iy / ix or vice versa */
	int32_t iTmp; /* temporary variable */

	/* check for pathological cases */
	if ((ix == 0) && (iy == 0))
		return (0);
	if ((ix == 0) && (iy != 0))
		return (Deg90);

	/* check for non-pathological cases */
	if (iy <= ix)
		iRatio = g_i16_Divide(
			iy,
			ix); /* return a fraction in range 0. to 32767 = 0. to 1. */
	else
		iRatio = g_i16_Divide(
			ix,
			iy); /* return a fraction in range 0. to 32767 = 0. to 1. */

	/* first, third and fifth order polynomial approximation */
	iAngle = (int32_t)K1 * (int32_t)iRatio;
	iTmp = ((int32_t)iRatio >> 5) * ((int32_t)iRatio >> 5) *
	       ((int32_t)iRatio >> 5);
	iAngle += (iTmp >> 15) * (int32_t)K2;
	iTmp = (iTmp >> 20) * ((int32_t)iRatio >> 5) * ((int32_t)iRatio >> 5);
	iAngle += (iTmp >> 15) * (int32_t)K3;
	iAngle = iAngle >> 15;

	/* check if above 45 degrees */
	if (iy > ix)
		iAngle = (int16_t)(Deg90 - iAngle);

	/* for tidiness, limit result to range 0 to 9000 equals 0.0 to 90.0 degrees */
	if (iAngle < 0)
		iAngle = 0;
	if (iAngle > Deg90)
		iAngle = Deg90;

	return ((int16_t)iAngle);
}

/* The function iHundredAtan2Deg is a wrapper function 
 * which implements the ATAN2 function by
 * assigning the results of an ATAN function to the correct quadrant. 
 * The result is the angle 
 * in degrees times 100.
 */
/* calculates 100*atan2(iy/ix)=100*atan2(iy,ix) in deg for ix, iy in range -32768 to 32767 */
int16_t g_i16_HundredAtan2Deg(int16_t iy, int16_t ix)
{
	int16_t iResult; /* angle in degrees times 100 */

	/* check for -32768 which is not handled correctly */
	if (ix == -32768)
		ix = -32767;
	if (iy == -32768)
		iy = -32767;

	/* check for quadrants */
	if ((ix >= 0) && (iy >= 0)) /* range 0 to 90 degrees */
		iResult = g_i16_HundredAtanDeg(iy, ix);
	else if ((ix <= 0) && (iy >= 0)) /* range 90 to 180 degrees */
		iResult = Deg180 - g_i16_HundredAtanDeg(iy, -ix);
	else if ((ix <= 0) && (iy <= 0)) /* range -180 to -90 degrees */
		iResult = g_i16_HundredAtanDeg(-iy, -ix) - Deg180;
	else /* ix >=0 and iy <= 0 giving range -90 to 0 degrees */
		iResult = -g_i16_HundredAtanDeg(-iy, ix);

	return (iResult);
}

/* Calculates smallest sector between two angles. In the range +-18000*/
int16_t g_i16_AngleBetween(int16_t Alpha, int16_t Betha)
{
	uint16_t myDeltaAngle = abs(Alpha - Betha);
	if (myDeltaAngle > Deg180)
		myDeltaAngle = Deg360 - myDeltaAngle;
	return myDeltaAngle;
}

/** Implemented at v3.21-17
 * @brief    Fast Square root algorithm, with rounding
 *
 * This does arithmetic rounding of the result. That is, if the real answer
 * would have a fractional part of 0.5 or greater, the result is rounded up to
 * the next integer.
 *      - SquareRootRounded(2) --> 1
 *      - SquareRootRounded(3) --> 2
 *      - SquareRootRounded(4) --> 2
 *      - SquareRootRounded(6) --> 2
 *      - SquareRootRounded(7) --> 3
 *      - SquareRootRounded(8) --> 3
 *      - SquareRootRounded(9) --> 3
 *
 * @param[in] a_nInput - unsigned integer for which to find the square root
 *
 * @return Integer square root of the input value.
 */
uint32_t g_u32_SquareRootRounded(uint32_t a_nInput)
{
	uint32_t op = a_nInput;
	uint32_t res = 0;

	/* The second-to-top bit is set: use 1u << 14 
     * for uint16_t type; use 1uL<<30 for uint32_t type.
     */
	uint32_t one = 1uL << 30;

	/* "one" starts at the highest power of four <= than the argument. */
	while (one > op) {
		one >>= 2;
	}

	while (one != 0) {
		if (op >= res + one) {
			op = op - (res + one);
			res = res + 2 * one;
		}
		res >>= 1;
		one >>= 2;
	}

	/* Do arithmetic rounding to nearest integer */
	if (op > res) {
		res++;
	}

	return res;
}