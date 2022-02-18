#include <drivers/pwm.h>
#include <ztest.h>

int pwm_pin_set_usec(const struct device *dev, uint32_t pwm, uint32_t period,
		     uint32_t pulse, pwm_flags_t flags)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pwm);
	ARG_UNUSED(flags);

	ztest_check_expected_value(period);
	ztest_check_expected_value(pulse);

	return ztest_get_return_value();
}

int pwm_get_cycles_per_sec(const struct device *dev, uint32_t pwm,
			   uint64_t *cycles)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pwm);
	ARG_UNUSED(cycles);
	return ztest_get_return_value();
}