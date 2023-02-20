#ifndef MODEM_NF_H_
#define MODEM_NF_H_

#include <zephyr.h>

#define CCID_BUF_SIZE 20

typedef struct gsm_info {
	int rat;
	int mnc;
	int rssi;
	int min_rssi;
	int max_rssi;
	uint8_t ccid[CCID_BUF_SIZE];
} gsm_info;

/**
 * @brief Contains the necessary parameters to download a modem FW file
 * over FTP using the u-blox Sara R4 AT commands.
 */
struct modem_nf_uftp_params {
	/** @brief the IP address of the server, standard FTP port 21 assumed, must be numerical address*/
	const char *ftp_server;
	/** @brief the FTP username, max 32 characters long*/
	const char *ftp_user;
	/** @brief the FTP password max 32 characters long */
	const char *ftp_password;
	/** @brief timeout for the FTP download in seconds, 0 means forever*/
	int download_timeout_sec;
};

int modem_nf_reset(void);
int get_pdp_addr(char **);
int modem_nf_wakeup(void);
int modem_nf_sleep(void);
int get_ccid(char **);
int get_gsm_info(struct gsm_info *);
void stop_rssi(void);
void enable_rssi(void);
int modem_nf_pwr_off(void);

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
bool modem_has_power(void);
#endif
typedef struct {
	uint8_t success;
	uint32_t ch;
	int16_t dbm;
	int32_t seq;
	int32_t mod;
	int16_t dur;
} modem_test_tx_result_t;
int modem_test_tx_get_result(modem_test_tx_result_t **);
int modem_test_tx_run_test_default(void);
int modem_test_tx_run_test(uint32_t, int16_t, uint16_t);

/**
 * @brief Gets the SARA-R4 model information and firmware version for the modem
 *
 * @note The modem must have initialized before this function returns data.
 *
 * @param [out] model. On success return this points to the read-only modem model information string. Maximum
 * size of the string (including delimiter) is MDM_INFORMATION_LENGTH
 * @param [out] version. On success return this points to the read-only modem FW version string.
 * Maximum size of the string (including delimiter) is MDM_REVISION_LENGTH
 *
 * @retval  0      If success.
 * @retval -EINVAL If any parameter is NULL.
 * @retval -ENODATA If the modem model or version fields are empty. This could happen if the this
 * function is called before the modem has been reset.

 */
int modem_nf_get_model_and_fw_version(const char **model, const char **version);

/**
 * @brief Downloads a u-blox FW update over FTP and makes it available for install.
 * The modem must have a network connection and a PDP context established if this function
 * will succeed.
 * @param [in] ftp_params the FTP connection params
 * @param [in]  filename the filename to download, size is limited to 24 characters
 * @retval 0  If success
 * @retval -EINVAL if ftp_params or filename is NULL
 * @retval -ETIMEDOUT if connection establishment or download timed-out
 * @retva -ECONNREFUSED if not able to login to the FTP server
 * @retval -EIO if low-level AT commands fail
 */
int modem_nf_ftp_fw_download(const struct modem_nf_uftp_params *ftp_params, const char *filename);

int modem_nf_ftp_fw_install(bool start_install);

void set_modem_status_cb(int (*read_status)(uint8_t *), int (*write_status)(uint8_t));
#endif /* MODEM_NF_H_ */
