/**
 *
 * Name         :  fifo.h
 *
 * Description  :  This is header file for NoFence fifo functions
 *
 * Author       :  Oscar Hovde Berntsen <oscar@nofence.no>
 * Website      :  http://www.nofence.no
 *
 * Credit       :  Oscar Hovde Berntsen (2014) 
 *
 *
 * License      :  Copyright 2015 Nofence AS
 *
 *                 Licensed under the Apache License, Version 2.0 (the "License");
 *                 you may not use this file except in compliance with the License.
 *                 You may obtain a copy of the License at
 *              
 *                     http://www.apache.org/licenses/LICENSE-2.0
 *              
 *                 Unless required by applicable law or agreed to in writing, software
 *                 distributed under the License is distributed on an "AS IS" BASIS,
 *                 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *                 See the License for the specific language governing permissions and
 *                 limitations under the License.
 *
 * Compiler     :  WinAVR, GCC for AVR platform 
 *                 Tested version :
 *                 - 20100110
 * Compiler note:  Please be aware of using older/newer version since WinAVR 
 *                 is in extensive development. Please compile with parameter -O1 
 *
 * History      :  
 * Please refer to nf_fifo.c
 * 
 * @note Migrated to Zephyr 25.03.2022
 */

#ifndef _NF_FIFO_H_
#define _NF_FIFO_H_

#include <zephyr.h>
#include <stdint.h>

/** @brief Counts decreasing elements in fifo.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Number of decreasing elements in fifo.
 */
uint8_t fifo_dec_cnt(int16_t *fx_array, uint8_t size);

/** @brief Counts increasing elements in fifo.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Number of increasing elements in fifo.
 */
uint8_t fifo_inc_cnt(int16_t *fx_array, uint8_t size);

/** @brief Calculates the slope rate of fifo register.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Slope rate of fifo register.
 */
int16_t fifo_slope(int16_t *fx_array, uint8_t size);

/** @brief Calculate average of fifo register.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Average value of fifo register.
 */
int16_t fifo_avg(int16_t *fx_array, uint8_t size);

/** @brief Min value of fifo register.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Min value of fifo register.
 */
int16_t fifo_min(int16_t *fx_array, uint8_t size);

/** @brief Max value of fifo register.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Max value of fifo register.
 */
int16_t fifo_max(int16_t *fx_array, uint8_t size);

/** @brief Subtracts fifo_min from fifo_max.
 * 
 * @param fx_array array of values.
 * @param size of array.
 * 
 * @returns Delta value of fifo register.
 */
int16_t fifo_delta(int16_t *fx_array, uint8_t size);

/** @brief Makes all element in a fifo equal to the last input.
 * 
 * @param fx_array array of values.
 * @param size of array.
 */
void fifo_fill(int16_t *fx_array, uint8_t size);

/** @brief Puts an element into fifo register.
 * 
 * @param[in] in value to store into array.
 * @param[in] fx_array array to store to.
 * @param[in] size of array.
 */
void fifo_put(int16_t in, int16_t *fx_array, uint8_t size);

#endif