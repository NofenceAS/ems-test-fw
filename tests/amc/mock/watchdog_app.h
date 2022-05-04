/*
 * Copyright (c) 2021 Nofence AS
 */

/**@file
 *
 * @brief   Watchdog module
 */

#ifndef WATCHDOG_APP_MOCK_H__
#define WATCHDOG_APP_MOCK_H__

#include <zephyr.h>

int watchdog_init_and_start(void);
void external_watchdog_feed(void);

#endif /* WATCHDOG_APP_MOCK_H__ */
