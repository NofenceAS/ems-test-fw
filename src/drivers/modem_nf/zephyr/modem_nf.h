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

int modem_nf_reset(void);
int get_pdp_addr(char **);
int modem_nf_wakeup(void);
int modem_nf_sleep(void);
int get_ccid(char **);
int get_gsm_info(struct gsm_info *);
void stop_rssi(void);
int modem_nf_pwr_off(void);

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

#endif /* MODEM_NF_H_ */