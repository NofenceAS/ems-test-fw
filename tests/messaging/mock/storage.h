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

int stg_init_storage_controller(void);
int stg_clear_partition(flash_partition_t partition);
int stg_fcb_reset_and_init();

typedef int (*stg_read_log_cb)(uint8_t *data, size_t len);
int stg_read_log_data(stg_read_log_cb cb);
int stg_read_ano_data(stg_read_log_cb cb);
int stg_read_pasture_data(stg_read_log_cb cb);
int stg_write_log_data(uint8_t *data, size_t len);
int stg_write_ano_data(uint8_t *data, size_t len, bool first_frame);
int stg_write_pasture_data(uint8_t *data, size_t len);

#endif /* _STORAGE_H_ */