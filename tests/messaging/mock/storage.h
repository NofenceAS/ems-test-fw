/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <zephyr.h>

/** Used to tell storage controller which region to read/write to. */
typedef enum {
	STG_PARTITION_LOG = 0,
	STG_PARTITION_ANO = 1,
	STG_PARTITION_PASTURE = 2
} flash_partition_t;

/** 
 * @brief Callback function containing the read entry data. Created
 *        by the caller, which also means we do not 
 *        link in a partition since the caller should have a unique
 *        callback function of what they want to do with the data. We just 
 *        give them some data and a length for every entry.
 * 
 * @param[in] data the raw data pointer.
 * @param[in] len size of the data.
 * 
 * @return 0 on success, otherwise negative errno.
 */
typedef int (*fcb_read_cb)(uint8_t *data, size_t len);

int stg_init_storage_controller(void);
int stg_clear_partition(flash_partition_t partition);
int stg_fcb_reset_and_init();

typedef int (*fcb_read_cb)(uint8_t *data, size_t len);
int stg_read_log_data(fcb_read_cb cb, uint16_t num_entries);
int stg_read_ano_data(fcb_read_cb cb, uint16_t num_entries);
int stg_read_pasture_data(fcb_read_cb cb);
int stg_write_log_data(uint8_t *data, size_t len);
int stg_write_ano_data(uint8_t *data, size_t len);
int stg_write_pasture_data(uint8_t *data, size_t len);
uint32_t get_num_entries(flash_partition_t partition);
bool stg_log_pointing_to_last();

#endif /* _STORAGE_H_ */