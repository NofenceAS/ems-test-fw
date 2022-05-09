/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"

void init_dummy_pasture(void);

/* Pasture tests. */
void test_pasture(void);
void test_pasture_extended_write_read(void);
void test_reboot_persistent_pasture(void);
void test_request_pasture_multiple(void);
void test_no_pasture_available(void);

/* Log tests. */
void test_log(void);
void test_log_extended(void);
void test_reboot_persistent_log(void);
void test_no_log_available(void);
void test_log_after_reboot(void);
void test_double_clear(void);
void test_rotate_handling(void);

/* Ano tests. */
void test_ano_write_20_days(void);
void test_ano_write_sent(void);
void test_no_ano_available(void);
void test_ano_write_all(void);
void test_reboot_persistent_ano(void);

/* System diagnostic tests. */
void test_sys_diag_log(void);
void test_reboot_persistent_system_diag(void);

#endif /* _STORAGE_HELPER_H_ */