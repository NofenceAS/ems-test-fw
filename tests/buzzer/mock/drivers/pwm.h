/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef PWM_MOCK_H_
#define PWM_MOCK_H_

#include <zephyr.h>
#include <device.h>

/**
 * @brief Provides a type to hold PWM configuration flags.
 */
typedef uint8_t pwm_flags_t;

int pwm_pin_set_usec(const struct device *dev, uint32_t pwm, uint32_t period, uint32_t pulse,
		     pwm_flags_t flags);

int pwm_get_cycles_per_sec(const struct device *dev, uint32_t pwm, uint64_t *cycles);

#define PWM_BUZZER_NODE NULL
#define PWM_BUZZER_LABEL DT_LABEL(DT_ALIAS(dummy_buzzer))
#define BUZZER_PWM_CHANNEL 0

#endif /* PWM_MOCK_H_ */
