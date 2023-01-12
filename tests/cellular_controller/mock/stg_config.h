/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef MOCK_STG_CONFIG_H
#define MOCK_STG_CONFIG_H

#include <zephyr.h>

#define STG_CONFIG_HOST_PORT_BUF_LEN 24
#define STG_CONFIG_BLE_SEC_KEY_LEN 8

typedef enum {
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
	STG_PARAM_ID_CNT
} stg_config_param_id_t;

int stg_config_u8_read(stg_config_param_id_t id, uint8_t *value);
int stg_config_u8_write(stg_config_param_id_t id, const uint8_t value);

int stg_config_u16_read(stg_config_param_id_t id, uint16_t *value);
int stg_config_u16_write(stg_config_param_id_t id, const uint16_t value);

int stg_config_u32_read(stg_config_param_id_t id, uint32_t *value);
int stg_config_u32_write(stg_config_param_id_t id, const uint32_t value);

int stg_config_str_read(stg_config_param_id_t id, char *str, uint8_t *len);
int stg_config_str_write(stg_config_param_id_t id, const char *str, const uint8_t len);

int stg_config_blob_read(stg_config_param_id_t id, uint8_t *arr, uint8_t *len);
int stg_config_blob_write(stg_config_param_id_t id, const uint8_t *arr, const uint8_t len);

int stg_config_erase_all(void);

#endif /* MOCK_STG_CONFIG_H */
