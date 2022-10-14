
/*
* Copyright (c) 2022 Nofence AS
*/

#include "stg_config.h"
#include <ztest.h>

int stg_config_u8_read(stg_config_param_id_t id, uint8_t *value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_u8_write(stg_config_param_id_t id, const uint8_t value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_u16_read(stg_config_param_id_t id, uint16_t *value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_u16_write(stg_config_param_id_t id, const uint16_t value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_u32_read(stg_config_param_id_t id, uint32_t *value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_u32_write(stg_config_param_id_t id, const uint32_t value)
{
	ARG_UNUSED(id);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int stg_config_str_read(stg_config_param_id_t id, char *str, uint8_t *len)
{
	ARG_UNUSED(id);
	ARG_UNUSED(str);
    ARG_UNUSED(len);
	return ztest_get_return_value();
}

int stg_config_str_write(stg_config_param_id_t id, const char *str, const uint8_t len)
{
 	ARG_UNUSED(id);
	ARG_UNUSED(str);
    ARG_UNUSED(len);
	return ztest_get_return_value();
}
