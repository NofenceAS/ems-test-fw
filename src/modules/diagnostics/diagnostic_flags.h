/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _DIAGNOSTIC_FLAGS_H_
#define _DIAGNOSTIC_FLAGS_H_

#include <zephyr.h>

typedef enum {
	FOTA_DISABLED = (1 << 0),
	CELLULAR_THREAD_DISABLED = (1 << 1),
	//GNSS_THREAD_DISABLED = (1 << 2),
	//BUZZER_DISABLED = (1 << 3),

	INTERNAL_CLEAR_FLAGS_ON_STARTUP = (1 << 30),
	INTERNAL_DIAG_FLAGS_IS_ACTIVE = (1 << 31),
} diagnostic_flags_t;

int diagnostic_flags_init(void);

int diagnostic_clear_flags(void);

int diagnostic_get_flags(uint32_t *flags);

int diagnostic_clear_flag(uint32_t flag);

int diagnostic_set_flag(uint32_t flag);

bool diagnostic_has_flag(uint32_t flag);

#endif