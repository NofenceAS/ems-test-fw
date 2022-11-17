#include "cmd_stimulator.h"

#include "buzzer.h"
#include "sound_event.h"
#include "ep_event.h"
#include "event_manager.h"
#include "messaging_module_events.h"
#include "gnss_hub.h"
#include "gnss.h"
#include "modem_nf.h"
#include "onboard_data.h"
#include "charging.h"

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
		case GET_GNSS_DATA:
		{
			gnss_struct_t *gnss_data;
			onboard_get_gnss_data(&gnss_data);

			resp = DATA;
			commander_send_resp(interface, STIMULATOR, cmd, resp, 
					(void*)gnss_data, sizeof(gnss_struct_t));
			break;
		}
		case GET_SENS_DATA:
		{
			onboard_sens_data_struct_t *sens_data;
			onboard_get_sens_data(&sens_data);

			resp = DATA;
			commander_send_resp(interface, STIMULATOR, cmd, resp, 
					(void*)sens_data, sizeof(onboard_sens_data_struct_t));
			break;
		}
		case SET_CHARGING_EN:
		{
			if (size < 1) {
				resp = NOT_ENOUGH;
				err = -EINVAL;
			} else {
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
				commander_send_resp(interface, STIMULATOR, cmd, resp, 
						(void*)&val, sizeof(uint8_t));
			}
			break;
		}
		case BUZZER_WARN:
		{
			/** @todo Warning frequency as argument? */

			/* Simulate the highest tone event */
			struct sound_event *sound_event_warn = new_sound_event();
			sound_event_warn->type = SND_WARN;
			EVENT_SUBMIT(sound_event_warn);
			
			struct sound_set_warn_freq_event *sound_warn_freq = new_sound_set_warn_freq_event();
			sound_warn_freq->freq = WARN_FREQ_MAX;
			EVENT_SUBMIT(sound_warn_freq);

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