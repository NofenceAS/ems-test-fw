
#ifndef GNSS_H_
#define GNSS_H_

/** @brief Struct containing GNSS data. */
typedef struct {
	int32_t lat;
	int32_t lon;

	/** Corrected position.*/
	int16_t x;
	/** Corrected position.*/
	int16_t y;

	/** Set if overflow because of too far away from origin position.*/
	uint8_t overflow;

	/** Height above ellipsoid [dm].*/
	int16_t height;

	/** 2-D speed [cm/s]*/
	uint16_t speed;

	/** Movement direction (-18000 to 18000 Hundred-deg).*/
	int16_t head_veh;

	/** Horizontal dilution of precision.*/
	uint16_t h_dop;

	/** Horizontal Accuracy Estimate [DM].*/
	uint16_t h_acc_dm;

	/** Vertical Accuracy Estimate [DM].*/
	uint16_t v_acc_dm;

	/** Heading accuracy estimate [Hundred-deg].*/
	uint16_t head_acc;

	/** Number of SVs used in Nav Solution.*/
	uint8_t num_sv;

	/** UBX-NAV-PVT flags as copied.*/
	uint8_t pvt_flags;

	/** UBX-NAV-PVT valid flags as copies.*/
	uint8_t pvt_valid;

	/** Milliseconds since position report from UBX.*/
	uint16_t age;

	/** UBX-NAV-SOL milliseconds since receiver start or reset.*/
	uint32_t msss;

	/** UBX-NAV-SOL milliseconds since First Fix.*/
	uint32_t ttff;
} gnss_struct_t;

/** @brief See gps_struct_t for descriptions. */
typedef struct {
	int32_t lat;
	int32_t lon;
	uint64_t unix_timestamp;
	uint16_t h_acc_dm;
	int16_t head_veh;
	uint16_t head_acc;
	uint8_t pvt_flags;
	uint8_t num_sv;
	uint16_t h_dop;
	int16_t height;
	int16_t baro_height;
	uint32_t msss;
	uint8_t gps_mode;
} gnss_last_fix_struct_t;

#define GNSS_RESET_MASK_HOT	0x0000
#define GNSS_RESET_MASK_WARM	0x0001
#define GNSS_RESET_MASK_COLD	0xFFFF

/**
 * @typedef gnss_position_fetch_t
 * @brief Callback API for getting a reading from GNSS
 *
 * See gnss_position_fetch() for argument description
 */
typedef int (*gnss_data_cb_t)(gnss_struct_t* data);
typedef int (*gnss_lastfix_cb_t)(gnss_last_fix_struct_t* lastfix);

typedef int (*gnss_setup_t)(const struct device *dev);
typedef int (*gnss_reset_t)(const struct device *dev, uint8_t mask, uint8_t mode);

//typedef int (*gnss_set_data_cb_t)(const struct device *dev, );

typedef int (*gnss_set_data_cb_t)(const struct device *dev, gnss_data_cb_t gnss_data_cb);
typedef int (*gnss_set_lastfix_cb_t)(const struct device *dev, gnss_lastfix_cb_t gnss_lastfix_cb);

typedef int (*gnss_data_fetch_t)(const struct device *dev, gnss_struct_t* data);
typedef int (*gnss_lastfix_fetch_t)(const struct device *dev, gnss_last_fix_struct_t* lastfix);

__subsystem struct gnss_driver_api {
	gnss_setup_t gnss_setup;
	gnss_reset_t gnss_reset;

	gnss_set_data_cb_t gnss_set_data_cb;
	gnss_set_lastfix_cb_t gnss_set_lastfix_cb;
	
	gnss_data_fetch_t gnss_data_fetch;
	gnss_lastfix_fetch_t gnss_lastfix_fetch;

	/* TODO - ANO Data */
};

/**
 * @brief Get a position from GNSS
 *
 *
 * @param dev Pointer to GNSS device
 *
 * @return 0 if successful, negative errno code if failure.
 */

static inline int gnss_setup(const struct device *dev)
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_setup(dev);
}

static inline int gnss_reset(const struct device *dev, uint8_t mask, uint8_t mode)
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_reset(dev, mask, mode);
}

static inline int gnss_set_data_cb(const struct device *dev, int (*gnss_data_cb)(gnss_struct_t* data))
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_set_data_cb(dev, gnss_data_cb);
}

static inline int gnss_set_lastfix_cb(const struct device *dev, int (*gnss_lastfix_cb)(gnss_last_fix_struct_t* lastfix))
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_set_lastfix_cb(dev, gnss_lastfix_cb);
}

static inline int gnss_data_fetch(const struct device *dev, gnss_struct_t* data)
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_data_fetch(dev, data);
}

static inline int gnss_lastfix_fetch(const struct device *dev, gnss_last_fix_struct_t* lastfix)
{
	const struct gnss_driver_api *api =
		(const struct gnss_driver_api *)dev->api;

	return api->gnss_lastfix_fetch(dev, lastfix);
}

#endif /* GNSS_H_ */