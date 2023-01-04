#include "cmd_stimulator.h"

#include "buzzer.h"
#include "sound_event.h"
#include "ep_event.h"
#include "event_manager.h"
#include "messaging_module_events.h"
#include "gnss_hub.h"
#include "gnss.h"
#include "onboard_data.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(diagnostics_stimulator, 4);


static void commander_gnss_data_received(void)
{
}

int commander_stimulator_handler(enum diagnostics_interface interface, uint8_t cmd, uint8_t *data,
				 uint32_t size)
{
	int err = 0;

	uint8_t resp = ACK;

	switch (cmd) {
	case GNSS_HUB: {
		if (size != 1) {
			resp = ERROR;
			break;
		}

		uint8_t hub_mode = data[0];
		if ((hub_mode == GNSS_HUB_MODE_SNIFFER) || (hub_mode == GNSS_HUB_MODE_SIMULATOR)) {
			gnss_hub_set_diagnostics_callback(commander_gnss_data_received);
		} else {
			gnss_hub_set_diagnostics_callback(NULL);
		}
		if (gnss_hub_configure(hub_mode) != 0) {
			resp = ERROR;
		}

		break;
	}
	case GNSS_SEND: {
		if (gnss_hub_send(GNSS_HUB_ID_DIAGNOSTICS, data, size) != 0) {
			resp = ERROR;
		}
		break;
	}
	case GNSS_RECEIVE: {
		uint8_t *buffer;
		uint32_t size = 0;
		if (gnss_hub_rx_get_data(GNSS_HUB_ID_DIAGNOSTICS, &buffer, &size) == 0) {
			/* Create and send data response */
			resp = DATA;
			size = MIN(size, 100);

			commander_send_resp(interface, STIMULATOR, cmd, resp, buffer, size);
			gnss_hub_rx_consume(GNSS_HUB_ID_DIAGNOSTICS, size);
		} else {
			resp = ERROR;
		}
		break;
	}
	case GET_ONBOARD_DATA:
	{
		onboard_all_data_struct_t *ob_all_data;
		onboard_get_all_data(&ob_all_data);

		resp = DATA;
		commander_send_resp(interface, STIMULATOR, cmd, resp, 
				(void*)ob_all_data, sizeof(onboard_all_data_struct_t));
		break;
	}
	case GET_OB_DATA:
	{
		onboard_data_struct_t *ob_data;
		onboard_get_data(&ob_data);

		resp = DATA;
		commander_send_resp(interface, STIMULATOR, cmd, resp, 
				(void*)ob_data, sizeof(onboard_data_struct_t));
		break;
	}
	case GET_GNSS_DATA:
	{
		gnss_struct_t *gnss_data;
		onboard_get_gnss_data(&gnss_data);

		resp = DATA;
		commander_send_resp(interface, STIMULATOR, cmd, resp, 
				(void*)gnss_data, sizeof(gnss_struct_t));
		break;
	}
	case GET_GSM_DATA:
	{
		gsm_info *ob_gsm_data;
		onboard_get_gsm_data(&ob_gsm_data);

		resp = DATA;
		commander_send_resp(interface, STIMULATOR, cmd, resp, 
				(void*)ob_gsm_data, sizeof(gsm_info));
		break;
	}
	case SET_CHARGING_EN:
	{
		if (size < 1) {
			resp = NOT_ENOUGH;
			err = -EINVAL;
		} else {
			resp = DATA;
			uint8_t val = 0;
			if (data[0]) {
				val = 1;
				if (charging_start() < 0) {
					resp = ERROR;
				}
			} else {
				val = 0;
				if (charging_stop() < 0) {
					resp = ERROR;
				}
			}
			if (resp == DATA) {
				commander_send_resp(interface, STIMULATOR, cmd, resp, 
						(void*)&val, sizeof(uint8_t));
			}
		}
		break;
	}
	case BUZZER_WARN:
	{	
		uint32_t freq = WARN_FREQ_MAX;

		if (size >= 4) {
			freq = (data[0] << 0) + 
					(data[1] << 8) + 
					(data[2] << 16) + 
					(data[3] << 24);
		}

		struct sound_event *sound_event_warn = new_sound_event();
		sound_event_warn->type = SND_WARN;
		EVENT_SUBMIT(sound_event_warn);

		/** unable to confirm frequency
S			 * <Timeout on getting a new warn zone freq>, Error code=-116, Sever~ */
		struct sound_set_warn_freq_event *sound_warn_freq = new_sound_set_warn_freq_event();
		sound_warn_freq->freq = freq;
		EVENT_SUBMIT(sound_warn_freq); 

		resp = DATA;
		commander_send_resp(interface, STIMULATOR, cmd, resp, (uint8_t*)&freq, sizeof(uint32_t));

		break;
	}
	case ELECTRICAL_PULSE:
	{
		struct warn_correction_pause_event *ev_q_test_zap =
			new_warn_correction_pause_event();
		ev_q_test_zap->reason = Reason_WARNPAUSEREASON_ZAP;
		ev_q_test_zap->fence_dist = 1;
		ev_q_test_zap->warn_duration = 1;
		EVENT_SUBMIT(ev_q_test_zap);

		/* Send electric pulse */
		struct ep_status_event *ready_ep_event = new_ep_status_event();
		ready_ep_event->ep_status = EP_RELEASE;
		ready_ep_event->is_first_pulse = false;
		EVENT_SUBMIT(ready_ep_event);
		k_sleep(K_SECONDS(2));
		struct sound_status_event *ev_idle = new_sound_status_event();
		ev_idle->status = SND_STATUS_IDLE;
		EVENT_SUBMIT(ev_idle);
		break;
	}

	default:
		resp = UNKNOWN_CMD;
		break;
	}

	/* Send all non-data responses here, data has been sent earlier */
	if (resp != DATA) {
		commander_send_resp(interface, STIMULATOR, cmd, resp, NULL, 0);
	}

	return err;
}