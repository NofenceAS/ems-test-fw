#include "messaging_module_events.h"

EVENT_TYPE_DEFINE(messaging_ack_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(messaging_proto_out_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(messaging_mdm_fw_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(messaging_stop_connection_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(messaging_host_address_event, false, NULL, NULL);

/* State update events (published by AMC) */
EVENT_TYPE_DEFINE(update_collar_mode, false, NULL, NULL);

EVENT_TYPE_DEFINE(update_collar_status, false, NULL, NULL);

EVENT_TYPE_DEFINE(update_fence_status, false, NULL, NULL);

EVENT_TYPE_DEFINE(update_fence_version, false, NULL, NULL);

EVENT_TYPE_DEFINE(update_flash_erase, false, NULL, NULL);

EVENT_TYPE_DEFINE(update_zap_count, false, NULL, NULL);

EVENT_TYPE_DEFINE(new_fence_available, false, NULL, NULL);

EVENT_TYPE_DEFINE(ano_ready, false, NULL, NULL);

EVENT_TYPE_DEFINE(animal_warning_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(animal_escape_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(check_connection, false, NULL, NULL);

EVENT_TYPE_DEFINE(send_poll_request_now, false, NULL, NULL);

EVENT_TYPE_DEFINE(warn_correction_start_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(warn_correction_end_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(warn_correction_pause_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(amc_zapped_now_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(turn_off_fence_event, false, NULL, NULL);
