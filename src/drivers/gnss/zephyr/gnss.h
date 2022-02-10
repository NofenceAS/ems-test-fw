
#ifndef GNSS_H_
#define GNSS_H_
/**
 * @typedef gnss_position_fetch_t
 * @brief Callback API for getting a reading from GNSS
 *
 * See gnss_position_fetch() for argument description
 */
typedef int (*gnss_position_fetch_t)(const struct device *dev);

__subsystem struct gnss_driver_api {
	gnss_position_fetch_t position_fetch;
};

/**
 * @brief Get a position from GNSS
 *
 *
 * @param dev Pointer to GNSS device
 *
 * @return 0 if successful, negative errno code if failure.
 */
static inline int gnss_position_fetch(const struct device *dev)
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->position_fetch(dev);
}

#endif /* GNSS_H_ */