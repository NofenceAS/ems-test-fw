
#ifndef UBLOX_MIA_M10_H_
#define UBLOX_MIA_M10_H_

#include <stdint.h>
#include <device.h>
#include "gnss.h"

#if CONFIG_GNSS_MIA_M10_UNIT_TESTING
int mia_m10_unit_test_init(struct device *dev);
#endif

/**
 * @brief Get configuration data (uint8_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to get.
 * @param[out] value Value in configuration parameter key. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_get_u8(uint32_t key, uint8_t* value);

/**
 * @brief Get configuration data (uint16_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to get.
 * @param[out] value Value in configuration parameter key. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_get_u16(uint32_t key, uint16_t* value);

/**
 * @brief Get configuration data (uint32_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to get.
 * @param[out] value Value in configuration parameter key. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_get_u32(uint32_t key, uint32_t* value);

/**
 * @brief Get configuration data (double) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to get.
 * @param[out] value Value in configuration parameter key. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_get_f64(uint32_t key, double* value);

/**
 * @brief Get configuration data (raw) for specified key. 
 *        The raw data value must be bit-wise converted to correct data type,
 *        depending on key. E.g. IEEE 754 format for float. 
 *
 * @param[in] key Configuration parameter key of data to get.
 * @param[out] value Value in configuration parameter key. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t* raw_value);

/**
 * @brief Set configuration data (uint8_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to set.
 * @param[in] value Value to set configuration parameter to. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_set_u8(uint32_t key, uint8_t value);

/**
 * @brief Set configuration data (uint16_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to set.
 * @param[in] value Value to set configuration parameter to. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_set_u16(uint32_t key, uint16_t value);

/**
 * @brief Set configuration data (uint32_t) for specified key. 
 *
 * @param[in] key Configuration parameter key of data to set.
 * @param[in] value Value to set configuration parameter to. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_set_u32(uint32_t key, uint32_t value);

/**
 * @brief Set configuration data (raw) for specified key. 
 *        Raw value must be of correct format depending on key data type. 
 *
 * @param[in] key Configuration parameter key of data to set.
 * @param[in] value Value to set configuration parameter to. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_config_set(uint32_t key, uint64_t raw_value);

/**
 * @brief Send reset to device. See device protocol documentation.
 *
 * @param[in] mask Mask of what to reset in the system. 
 * @param[in] mode Mode of reset, e.g. HW or SW. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_send_reset(uint16_t mask, uint8_t mode);

/**
 * @brief Send assistance data to GNSS. 
 *
 * @param[in] data Data buffer to upload. 
 * @param[in] size Size of data buffer.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mia_m10_send_assist_data(uint8_t* data, uint32_t size);

#endif /* UBLOX_MIA_M10_H_ */