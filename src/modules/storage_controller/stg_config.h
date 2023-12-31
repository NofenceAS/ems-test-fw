/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef STG_CONFIG_H
#define STG_CONFIG_H

#include <zephyr.h>

/* Length of a null-terminated IPv4 host/port string */
#define STG_CONFIG_HOST_PORT_BUF_LEN 24
/* Length of a Bluetooth Security Key */
#define STG_CONFIG_BLE_SEC_KEY_LEN 8

/**
 * @brief Identifiers for configuration parameters.
 */
typedef enum {
	/* NB! Do NOT change or reorder the identifiers of NVS Id-data pairs without 
	 * a sector erase of all NVS sectors */
	STG_U8_WARN_MAX_DURATION = 0,
	STG_U8_WARN_MIN_DURATION,
	STG_U8_PAIN_CNT_DEF_ESCAPED,
	STG_U8_COLLAR_MODE,
	STG_U8_FENCE_STATUS,
	STG_U8_COLLAR_STATUS,
	STG_U8_TEACH_MODE_FINISHED,
	STG_U8_EMS_PROVIDER,
	STG_U8_PRODUCT_GENERATION,
	STG_U8_PRODUCT_MODEL,
	STG_U8_PRODUCT_REVISION,
	STG_U8_HW_VERSION,
	STG_U8_KEEP_MODE,
	STG_U8_RESET_REASON,
	STG_U16_ACC_SIGMA_NOACTIVITY_LIMIT,
	STG_U16_OFF_ANIMAL_TIME_LIMIT_SEC,
	STG_U16_ACC_SIGMA_SLEEP_LIMIT,
	STG_U16_ZAP_CNT_TOT,
	STG_U16_ZAP_CNT_DAY,
	STG_U16_PRODUCT_TYPE,
	STG_U32_UID,
	STG_U32_WARN_CNT_TOT,
	STG_STR_HOST_PORT,
	STG_BLOB_BLE_KEY,
	STG_U8_MODEM_INSTALLING,
	STG_U32_DIAGNOSTIC_FLAGS,
	STG_PARAM_ID_CNT,
} stg_config_param_id_t;

/**
 * @brief Initialization of the flash configuration storage. 
 * @return 0 if successful, otherwise a negative error code.
 */
int stg_config_init(void);

/**
 * @brief Reads the u8 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value Pointer to the u8 data value.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u8_read(stg_config_param_id_t id, uint8_t *value);

/**
 * @brief Writes to the u8 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value The u8 data value to write.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u8_write(stg_config_param_id_t id, const uint8_t value);

/**
 * @brief Reads the u16 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value Pointer to the u16 data value.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u16_read(stg_config_param_id_t id, uint16_t *value);

/**
 * @brief Writes to the u16 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value The u16 data value to write.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u16_write(stg_config_param_id_t id, const uint16_t value);

/**
 * @brief Reads the u32 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value Pointer to the u32 data value.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u32_read(stg_config_param_id_t id, uint32_t *value);

/**
 * @brief Writes to the u32 config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param value The u32 data value to write.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_u32_write(stg_config_param_id_t id, const uint32_t value);

/**
 * @brief Reads the string config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param str Pointer to the data buffer.
 * @param len The length of the data.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_str_read(stg_config_param_id_t id, char *str, uint8_t *len);

/**
 * @brief Writes to the string config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param str Pointer to the data buffer.
 * @param len The length of the data.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_str_write(stg_config_param_id_t id, const char *str, const uint8_t len);

/**
 * @brief Reads the binary blob config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param arr Pointer to the data array.
 * @param len The length of the data.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_blob_read(stg_config_param_id_t id, uint8_t *arr, uint8_t *len);

/**
 * @brief Writes to binary blob config parameter as specified by the identifier.
 * @param id The identifier of the config parameter (See stg_config_param_id_t).
 * @param arr Pointer to the data array.
 * @param len The length of the data.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_blob_write(stg_config_param_id_t id, const uint8_t *arr, const uint8_t len);

/**
 * @brief Erase all flash sectors associated with STG config.
 * @note All STG config data will be lost and is NOT recoverable.
 * @return 0 if successful, otherwise a negative error code. 
 */
int stg_config_erase_all(void);

#endif /* STG_CONFIG_H */
