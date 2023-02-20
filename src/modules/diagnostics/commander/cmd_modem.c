#include "cmd_modem.h"
#include "modem_nf.h"
#include <stddef.h>
#include <string.h>
#include "cellular_helpers_header.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(diagnostics_cmd_modem, 4);

int commander_modem_handler(enum diagnostics_interface interface, uint8_t cmd, uint8_t *data,
			    uint32_t size)
{
	int err = 0;

	uint8_t resp = ACK;

	switch (cmd) {
	case GET_CCID: {
		char *ccid = NULL;

		err = get_ccid(&ccid);
		if (err == 0) {
			commander_send_resp(interface, MODEM, cmd, DATA, (uint8_t *)ccid,
					    strlen(ccid));
		} else {
			commander_send_resp(interface, MODEM, cmd, ERROR, NULL, 0);
		}
		break;
	}
	case GET_IP: {
		/* previous attemt using get_ip() from cellular_helpers 
		seems to block CPU and break connection to host */
		commander_send_resp(interface, MODEM, cmd, NOT_IMPLEMENTED, NULL, 0);

		break;
	}
	case TEST_MODEM_TX: {
		int ret = -1;

		if (size >= 8) {
			uint32_t tx_ch =
				(data[0] << 0) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
			int16_t dbm_level = (data[4] << 0) + (data[5] << 8);
			uint16_t test_dur = (data[6] << 0) + (data[7] << 8);
			LOG_DBG("TEST_MODEM_TX params: ch: %d, dbm: %d, dur: %d", tx_ch, dbm_level,
				test_dur);
			ret = modem_test_tx_run_test(tx_ch, dbm_level, test_dur);
		} else {
			LOG_DBG("TEST_MODEM_TX using default params");
			ret = modem_test_tx_run_test_default();
		}

		modem_test_tx_result_t *tx_test_res;
		modem_test_tx_get_result(&tx_test_res);

		if (ret == 0) {
			resp = DATA;
			commander_send_resp(interface, MODEM, cmd, resp, (void *)tx_test_res,
					    sizeof(modem_test_tx_result_t));
		} else {
			resp = ERROR;
			commander_send_resp(interface, MODEM, cmd, resp, NULL, 0);
		}

		LOG_DBG("TEST_MODEM_TX resetting modem");
		modem_nf_reset();

		break;
	}
	default:
		resp = UNKNOWN_CMD;
		commander_send_resp(interface, MODEM, cmd, resp, NULL, 0);
		break;
	}

	return err;
}
