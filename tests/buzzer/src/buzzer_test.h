/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _BUZZER_TEST_H_
#define _BUZZER_TEST_H_

#include <zephyr.h>
#include "sound_event.h"

void test_warn_zone_no_freq_event(void);
void test_warn_zone_timeout(void);

extern struct k_sem sound_idle_sem;
extern struct k_sem sound_playing_sem;
extern struct k_sem sound_playing_max_sem;
extern struct k_sem error_timeout_sem;

#endif /* _BUZZER_TEST_H_ */