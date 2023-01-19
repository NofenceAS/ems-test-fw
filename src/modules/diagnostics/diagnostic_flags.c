/*
 * Copyright (c) 2022 Nofence AS
 */

#include "diagnostic_flags.h"
#include "stg_config.h"
#include "onboard_data.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(diagnostic_flags, 4);

static uint32_t diagnostic_flags = 0;
//static uint16_t battery_mv = 0;

int diagnostic_stg_load_flags(void)
{
	int res = stg_config_u32_read(STG_U32_DIAGNOSTIC_FLAGS, &diagnostic_flags);
	if (res != 0) {
		LOG_ERR("error loading diagnostic flags from storage: %d", res);
	}

	return res;
}

int diagnostic_stg_save_flags(void)
{
	int res = stg_config_u32_write(STG_U32_DIAGNOSTIC_FLAGS, diagnostic_flags);
	if (res != 0) {
		LOG_ERR("error saving diagnostic flags to storage: %d", res);
	}

	return res;
}

bool diagnostic_flags_battery_override(void)
{
	onboard_data_struct_t *ob_data;
	int res = onboard_get_data(&ob_data);
	if (res != 0) {
		LOG_ERR("error reading battery override");
	}

	bool override = (ob_data->battery_mv >= 4000 && ob_data->battery_mv <= 4400);

	LOG_WRN("BATTERY OVERRIDE VALUE = %d, OVERRIDE = %d", ob_data->battery_mv, override);

	return override;
}

int diagnostic_flags_init(void)
{
	diagnostic_flags = 0;

	int res = 0;
	uint32_t default_flags =
		INTERNAL_DIAG_FLAGS_IS_ACTIVE | FOTA_DISABLED | CELLULAR_THREAD_DISABLED;

	res = diagnostic_stg_load_flags();
	if (res != 0) {
		diagnostic_flags = default_flags;
	}

	if (diagnostic_flags == 0xFFFFFFFF) {
		diagnostic_flags = default_flags;
		LOG_WRN("loaded flags = 0xFFFFFFFF, setting to default: %u", default_flags);
		res = diagnostic_set_flag(diagnostic_flags);
		if (res != 0) {
			LOG_ERR("could not reset flags on startup");
		}
	}

	if (!diagnostic_has_flag(INTERNAL_DIAG_FLAGS_IS_ACTIVE)) {
		LOG_WRN("flags not active, setting to default: %u", default_flags);
		res = diagnostic_clear_flags();
		if (res != 0) {
			LOG_ERR("flags not active, could not reset on startup");
		}
	}

	if (diagnostic_has_flag(INTERNAL_CLEAR_FLAGS_ON_STARTUP)) {
		diagnostic_flags = 0;
		LOG_WRN("INTERNAL_CLEAR_FLAGS_ON_STARTUP set, clearing flags");
		res = diagnostic_clear_flags();
		if (res != 0) {
			LOG_ERR("could not clear flags from on startup");
		}
	}

	LOG_WRN("diagnostic FOTA_DISABLED = %d", diagnostic_has_flag(FOTA_DISABLED));
	LOG_WRN("diagnostic CELLULAR_THREAD_DISABLED = %d",
		diagnostic_has_flag(CELLULAR_THREAD_DISABLED));
	//LOG_WRN("diagnostic GNSS_THREAD_DISABLED = %d", diagnostic_has_flag(GNSS_THREAD_DISABLED));
	//LOG_WRN("diagnostic BUZZER_DISABLED = %d", diagnostic_has_flag(BUZZER_DISABLED));
	return res;
}

int diagnostic_clear_flags(void)
{
	diagnostic_flags = INTERNAL_DIAG_FLAGS_IS_ACTIVE;

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_clear_flag(uint32_t flag)
{
	diagnostic_flags &= ~(flag) | INTERNAL_DIAG_FLAGS_IS_ACTIVE;

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_set_flag(uint32_t flag)
{
	diagnostic_flags |= (flag) | INTERNAL_DIAG_FLAGS_IS_ACTIVE;

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_get_flags(uint32_t *flags)
{
	int res = 0;

	if (diagnostic_flags_battery_override()) {
		LOG_WRN("DIAGNOSTIC FLAGS BATTERY OVERRIDE");
	}

	/* remove internal flags for readability */
	*flags = (diagnostic_flags &
		  ~(INTERNAL_DIAG_FLAGS_IS_ACTIVE | INTERNAL_CLEAR_FLAGS_ON_STARTUP));

	return res;
}

bool diagnostic_has_flag(uint32_t flag)
{
	if (diagnostic_flags_battery_override()) {
		return false;
	}

	return (diagnostic_flags & (flag));
}
