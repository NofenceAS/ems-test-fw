
#ifndef GNSS_H_
#define GNSS_H_

#include <zephyr.h>
#include <device.h>

/**
 * @brief GNSS domain-specific operation modes.
 * @note The order of these enumerations are very important, that is
 * to say that higher performance modes must have higher enumeration values
 * than lower ones.
 */
typedef enum {
	GNSSMODE_NOMODE = 0,
	GNSSMODE_INACTIVE = 1,
	GNSSMODE_PSM = 2,
	GNSSMODE_CAUTION = 3,
	GNSSMODE_MAX = 4,
	GNSSMODE_SIZE = 5
} gnss_mode_t;

/** @brief Struct containing NAV-PL data */
typedef struct {
	/* Protection level tmirCoeff */
	uint8_t tmirCoeff;

	/** Protection level tmirExp */
	uint8_t tmirExp;

	/** Protection level position is valid */
	uint8_t plPosValid;

	/** Protection level frame */
	uint8_t plPosFrame;

	/** Protection level axis 1 */
	uint32_t plPos1;

	/** Protection level axis 2 */
	uint32_t plPos2;

	/** Protection level axis 3 */
	uint32_t plPos3;
} gnss_pl_t;

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

	/** Milliseconds system time when data was updated from GNSS.*/
	uint32_t updated_at;

	/** UBX-NAV-STATUS milliseconds since receiver start or reset.*/
	uint32_t msss;

	/** UBX-NAV-STATUS milliseconds since First Fix.*/
	uint32_t ttff;

	uint8_t cno[4];

	/** Proprietary mode, to verify that any corrections are done when GNSS is in MAX performance */
	gnss_mode_t mode;

	/** Protection Level sub-fields */
	gnss_pl_t pl;

} gnss_struct_t;

/** @brief See gnss_struct_t for descriptions. */
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
	uint32_t msss;
	gnss_mode_t mode;
	uint32_t updated_at;
	gnss_pl_t pl;
} gnss_last_fix_struct_t;

/** @brief Struct containing both GNSS status messages and 
 *         a flag indicating whether the fix was sufficient and the lastfix
 *         has been updated. */
typedef struct {
	gnss_struct_t latest;
	bool fix_ok;

	gnss_last_fix_struct_t lastfix;
	bool has_lastfix;
} gnss_t;

/* Constants */
#define GNSS_RESET_MASK_HOT 0x0000
#define GNSS_RESET_MASK_WARM 0x0001
#define GNSS_RESET_MASK_COLD 0xFFFF

#define GNSS_RESET_MODE_HW_IMMEDIATELY 0x00
#define GNSS_RESET_MODE_SW 0x01
#define GNSS_RESET_MODE_SW_GNSS_ONLY 0x02
#define GNSS_RESET_MODE_HW_SHDN 0x04
#define GNSS_RESET_MODE_GNSS_STOP 0x08
#define GNSS_RESET_MODE_GNSS_START 0x09

/**
 * @typedef gnss_data_cb_t
 * @brief Callback definition used for notifying of 
 *        updated gnss_t
 *
 * @param[in] data Pointer to updated GNSS data
 * 
 * @return 0 if everything was ok, error code otherwise
 */
typedef int (*gnss_data_cb_t)(const gnss_t *data);

/**
 * @typedef gnss_setup_t
 * @brief Callback API for requesting setup to be performed for GNSS receiver
 *
 * See gnss_setup() for argument description
 */
typedef int (*gnss_setup_t)(const struct device *dev, bool try_default_baud_first);

/**
 * @typedef gnss_reset_t
 * @brief Callback API for requesting reset to be performed for GNSS receiver
 *
 * See gnss_reset() for argument description
 */
typedef int (*gnss_reset_t)(const struct device *dev, uint16_t mask, uint8_t mode);

/**
 * @typedef gnss_upload_assist_data_t
 * @brief Callback API for sending assistance data to GNSS receiver
 *
 * See gnss_upload_assist_data() for argument description
 */
typedef int (*gnss_upload_assist_data_t)(const struct device *dev, uint8_t *data, uint32_t size);

/**
 * @typedef gnss_set_rate_t
 * @brief Callback API for setting rate of GNSS receiver
 *
 * See gnss_set_rate() for argument description
 */
typedef int (*gnss_set_rate_t)(const struct device *dev, uint16_t rate);

/**
 * @typedef gnss_get_rate_t
 * @brief Callback API for getting rate of GNSS receiver
 *
 * See gnss_get_rate() for argument description
 */
typedef int (*gnss_get_rate_t)(const struct device *dev, uint16_t *rate);

/**
 * @typedef gnss_set_data_cb_t
 * @brief Callback API for setting callback to call on updated data
 *
 * See gnss_set_data_cb() for argument description
 */
typedef int (*gnss_set_data_cb_t)(const struct device *dev, gnss_data_cb_t gnss_data_cb);

/**
 * @typedef gnss_data_fetch_t
 * @brief Callback API for getting latest GNSS data
 *
 * See gnss_data_fetch() for argument description
 */
typedef int (*gnss_data_fetch_t)(const struct device *dev, gnss_t *data);

/**
 * @typedef gnss_set_backup_mode_t
 * @brief Callback API for setting the receiver in backup mode
 *
 * See gnss_set_backup_mode() for argument description
 */
typedef int (*gnss_set_backup_mode_t)(const struct device *dev);

/**
 * @typedef gnss_wakeup_t
 * @brief API for waking up the receicer from backup mode
 *
 * See gnss_wakeup() for argument description
 */
typedef int (*gnss_wakeup_t)(const struct device *dev);

/**
 * @typedef gnss_set_power_mode_t
 * @brief API for setting the receiver Power mode (PSM, Continous
 *
 * See gnss_set_power_mode() for argument description
 */
typedef int (*gnss_set_power_mode_t)(const struct device *dev, gnss_mode_t mode);

__subsystem struct gnss_driver_api {
	gnss_setup_t gnss_setup;
	gnss_reset_t gnss_reset;
	gnss_upload_assist_data_t gnss_upload_assist_data;
	gnss_set_rate_t gnss_set_rate;
	gnss_get_rate_t gnss_get_rate;
	gnss_set_data_cb_t gnss_set_data_cb;
	gnss_data_fetch_t gnss_data_fetch;
	gnss_set_backup_mode_t gnss_set_backup_mode;
	gnss_wakeup_t gnss_wakeup;
	gnss_set_power_mode_t gnss_set_power_mode;
};

/**
 * @brief Requesting setup to be performed for GNSS receiver
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[in] try_default_baud_first In case of UART communication, this 
 *                                   controls whether setup should reset 
 *                                   UART baud before performing setup.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_setup(const struct device *dev, bool try_default_baud_first)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_setup(dev, try_default_baud_first);
}

/**
 * @brief Request reset to be performed for GNSS receiver
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[in] mask Bit mask deciding what is being reset. 
 *                 Use constant definitions or see GNSS datasheet for detailed 
 *                 usage.
 * @param[in] mode Mode of reset. Use constant definitions. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_reset(const struct device *dev, uint16_t mask, uint8_t mode)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_reset(dev, mask, mode);
}

/**
 * @brief Send assistance data to GNSS receiver
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[in] data Raw buffer of data to be sent as assistance data
 * @param[in] size Size of raw buffer.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_upload_assist_data(const struct device *dev, uint8_t *data, uint32_t size)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_upload_assist_data(dev, data, size);
}

/**
 * @brief Get rate of GNSS receiver
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[out] rate Rate in milliseconds. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_set_rate(const struct device *dev, uint16_t rate)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_set_rate(dev, rate);
}

/**
 * @brief Set rate of GNSS receiver
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[in] rate Rate in milliseconds. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_get_rate(const struct device *dev, uint16_t *rate)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_get_rate(dev, rate);
}

/**
 * @brief Set callback to call on updated data
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[in] gnss_data_cb Callback function. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_set_data_cb(const struct device *dev, gnss_data_cb_t gnss_data_cb)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_set_data_cb(dev, gnss_data_cb);
}

/**
 * @brief Get latest GNSS data
 *
 * @param[in] dev Pointer to the GNSS device
 * @param[out] data Pointer to destination of data. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static inline int gnss_data_fetch(const struct device *dev, gnss_t *data)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_data_fetch(dev, data);
}

/**
 * @brief Set the receiver in backup-mode (aka RXM-PMREQ)
 * @param[in] dev Pointer to the GNSS device
 * @return 0 if OK, negative error code otherwise
 */
static inline int gnss_set_backup_mode(const struct device *dev)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_set_backup_mode(dev);
}

/**
* @brief Wakes up the receiver after backup
* @param[in] dev Pointer to the GNSS device
* @return 0 if OK, negative error code otherwise
*/
static inline int gnss_wakeup(const struct device *dev)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_wakeup(dev);
}

/**
 * @brief sets the domain-specific power-mode of the receiver
 * @param dev
 * @param mode
 * @return
 */
static inline int gnss_set_power_mode(const struct device *dev, gnss_mode_t mode)
{
	const struct gnss_driver_api *api = (const struct gnss_driver_api *)dev->api;

	return api->gnss_set_power_mode(dev, mode);
}

#endif /* GNSS_H_ */
