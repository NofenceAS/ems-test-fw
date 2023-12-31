/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _AMC_HANDLER_H_
#define _AMC_HANDLER_H_

#include <zephyr.h>

/** @brief Initializes the work calculation thread and puts
 *         request events for fence and GNSS data.
 * 
 * @return 0 on success, otherwise negative errno.
 */
int amc_module_init(void);

/* extern to make available for correction since event handler is in amc_handler */
extern atomic_t sound_max_atomic;

#endif /* _AMC_HANDLER_H_ */