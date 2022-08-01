/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef TEST_MOCK_PWM_H
#define TEST_MOCK_PWM_H

#include <zephyr.h>
#include <device.h>

#define PWM_EP_NODE NULL
#define PWM_EP_LABEL DT_LABEL(DT_ALIAS(dummy_ep))
#define PWM_EP_CHANNEL 0

/**
 * \brief Type to hold PWM configuration flags.
 */
typedef uint8_t pwm_flags_t;

/**
 * \brief Mock PWM function (See pwm_pin_set_usec in documentation).
 */
int pwm_pin_set_usec(const struct device *dev, uint32_t pwm, uint32_t period,
		     uint32_t pulse, pwm_flags_t flags);

#endif /* TEST_MOCK_PWM_H */
