/*
* Copyright (c) 2022 Nofence AS
*/

#include <modem_nf.h>
#include <ztest_mock.h>

static char *dummy_model = "01234567890123456789012345678901";
static char *dummy_version = "0123456789012345678901234567890123456789012345678901234567890123";

int modem_nf_get_model_and_fw_version(const char **model, const char **version)
{
	/* @todo, I have not found a way of using ztest_copy_return_data on a (char **) */
	*model = dummy_model;
	*version = dummy_version;
	return ztest_get_return_value();
}