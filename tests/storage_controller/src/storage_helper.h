/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"

void fill_dummy_fence(void);
/* Pasture tests. */
void test_pasture(void);
void test_pasture_extended_write_read(void);
void test_reboot_persistent_pasture(void);
void test_request_pasture_multiple(void);
void test_no_pasture_available(void);

/* Log tests. */
//void test_log_write(void);
//void test_log_read(void);
//void test_reboot_persistent_log(void);
//void test_write_log_exceed(void);
//void test_read_log_exceed(void);
//void test_empty_walk_log(void);
//
///* Ano tests. */
//void test_ano_write(void);
//void test_ano_read(void);
//void test_reboot_persistent_ano(void);

#endif /* _STORAGE_HELPER_H_ */