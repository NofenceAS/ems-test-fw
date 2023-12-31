/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _AMC_CACHE_H_

#include <zephyr.h>
#include "pasture_structure.h"
#include "gnss.h"

extern struct k_sem fence_data_sem;

/**
 * @brief Fetches the cached fence data and outputs the length and 
 *        pointer location to the cached fence.
 * 
 * @note Semaphore locking when using the cached area must be handled outside
 *       this function, since we give the caller a pointer to the area.
 * 
 * @param[out] pasture pointer to where the cached pasture is stored.
 * @param[in] len length of the pasture struct.
 * 
 * @return 0 on success.
 * @return -ENODATA if pasture has 0 fences.
 */
int get_pasture_cache(pasture_t **pasture);

/**
 * @brief Free's previous fence cache and stores a new one based on
 *        input data. It is of type uint8_t* and not fence_t* since this
 *        function is fed into the storage controller which reads out
 *        raw binary data.
 * 
 * @param[in] pasture pointer to where the pasture is buffered.
 * @param[in] len length of the pasture struct.
 * 
 * @return 0 on success, otherwise negative errno.
 */
int set_pasture_cache(uint8_t *pasture, size_t len);

/** @brief Checks if a fence is valid with number of points and type..
 * 
 * @param fence pointer to fence(polygon) to chec.
 * 
 * @return true if valid, false if not valid.
*/
bool fnc_valid(fence_t *fence);

/** @brief Checks if there's any valid fence in the pasture. 
 * 
 * @note Mutex must be taken outside of this function.
 * 
 * @return true if valid fence exists.
*/
bool fnc_valid_fence(void);

/** @brief Checks if the cached pasture has valid definition.
 * 
 * @note Mutex must be taken outside of this function.
 * 
 * @return true if valid definition. (def != 0)
*/
bool fnc_valid_def(void);

/**
 * @brief Fetches the cached gnss data and outputs the
 *        pointer location to the cached gnss area.
 * 
 * @note No need to lock semaphores outside, since we use two gnss cached
 *       areas. Whenever we consume gnss_cache, we check if we have new
 *       data written to the other cached area, and swap them if its true.
 * 
 * @param[out] gnss pointer to where the cached gnss is stored.
 * 
 * @return 0 on success.
 * @return -ENODATA if gnss cache has not been set.
 */
int get_gnss_cache(gnss_t **gnss);

/**
 * @brief Updates the gnss cache area that is not being read from. Locks
 *        If consumer has not yet processed the previous GNSS entry.
 * 
 * @param[in] gnss gnss data to cache.
 * @param[in] timed_out Flag indicating whether or not the GNSS timed out.
 * 
 * @return 0 on success, otherwise negative errno on semaphore error.
 */
int set_gnss_cache(gnss_t *gnss, const bool timed_out);

#endif /* _AMC_CACHE_H_ */