/*
 * Copyright (c) 2022 Nofence AS
 */

//#include <drivers/pwm.h>
#include <ztest.h>
#include "pwm.h"

int pwm_pin_set_usec(const struct device *dev, uint32_t pwm, uint32_t period,
		     uint32_t pulse, pwm_flags_t flags)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pwm);
	ARG_UNUSED(period);
	ARG_UNUSED(pulse);
	ARG_UNUSED(flags);

	return ztest_get_return_value();
}
