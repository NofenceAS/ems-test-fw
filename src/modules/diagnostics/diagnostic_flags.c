/*
 * Copyright (c) 2022 Nofence AS
 */

#include "diagnostic_flags.h"
#include "stg_config.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(diagnostic_flags, 4);

static uint32_t diagnostic_flags = 0;

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

int diagnostic_flags_to_string(char *binary_str)
{
	char buffer[33];
	itoa(diagnostic_flags, buffer, 2);
	memcpy(binary_str, buffer, sizeof(buffer));

	return 0;
}

int diagnostic_flags_init(void)
{
	diagnostic_flags = 0;

	int res = diagnostic_stg_load_flags();
	if (res != 0) {
		diagnostic_flags = FOTA_DISABLED | BUZZER_DISABLED;
	} 
    
    if (diagnostic_has_flag(CLEAR_STG_FLAGS_ON_STARTUP)) {
		diagnostic_flags = 0;
        LOG_INF("CLEAR_STG_FLAGS_ON_STARTUP set, clearing flags");
		res = diagnostic_clear_flags();
		if (res != 0) {
			LOG_ERR("could not clear flags from on startup");
		}
	}

	char binstr[33];
	diagnostic_flags_to_string(&binstr);
	LOG_INF("diagnostic flags loaded: %s", log_strdup(binstr));
	LOG_INF("diagnostic OTA_DISABLED = %d", diagnostic_has_flag(FOTA_DISABLED));
	LOG_INF("diagnostic CELLULAR_THREAD_DISABLED = %d", diagnostic_has_flag(CELLULAR_THREAD_DISABLED));
	LOG_INF("diagnostic GNSS_THREAD_DISABLED = %d", diagnostic_has_flag(GNSS_THREAD_DISABLED));
	LOG_INF("diagnostic BUZZER_DISABLED = %d", diagnostic_has_flag(BUZZER_DISABLED));
	return res;
}

int diagnostic_clear_flags(void)
{
	diagnostic_flags = 0;

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_clear_flag(uint32_t flag)
{
	diagnostic_flags &= ~(flag);

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_set_flag(uint32_t flag)
{
	diagnostic_flags |= (flag);

	int res = diagnostic_stg_save_flags();

	return res;
}

int diagnostic_get_flags(uint32_t *flags)
{
    int res = 0;

    *flags = &diagnostic_flags;

	return res;
}

bool diagnostic_has_flag(uint32_t flag)
{
	return (diagnostic_flags & (flag));
}
