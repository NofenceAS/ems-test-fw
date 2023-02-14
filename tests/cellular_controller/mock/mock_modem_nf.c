#include <modem_nf.h>
#include <ztest.h>

int modem_nf_reset(void)
{
	return 0;
}

int modem_nf_wakeup(void)
{
	return 0;
}

int get_gsm_info(struct gsm_info *gsm_session_info)
{
	return 0;
}

void stop_rssi(void)
{
	return;
}

void set_modem_status_cb(int (*read_status)(uint8_t *), int (*write_status)(uint8_t))
{
	return;
}

int modem_nf_pwr_off(void)
{
	return ztest_get_return_value();
}

int modem_nf_ftp_fw_download(const struct modem_nf_uftp_params *ftp_params, const char *filename)
{
	ztest_check_expected_value(filename);
	int err = strcmp(ftp_params->ftp_server, "172.31.45.52");
	zassert_equal(err, 0, "Wrong FTP IP address!");
	err = strcmp(ftp_params->ftp_user, "sarar412");
	zassert_equal(err, 0, "Wrong FTP credentials!");
	err = strcmp(ftp_params->ftp_password, "Klave12005");
	zassert_equal(err, 0, "Wrong FTP credentials!");
	zassert_equal(ftp_params->download_timeout_sec, 3600, "");
	return ztest_get_return_value();
}

int modem_nf_ftp_fw_install(bool start_install)
{
	return ztest_get_return_value();
}
