#include <event_manager.h>
#include "collar_protocol.h"
#include "stg_config.h"

/** @brief Empty event published by the messaging module to acknowledge
 *         reception of proto_in messages from the cellular controller. 
 */
struct messaging_ack_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(messaging_ack_event);

/** @brief Empty event published by the messaging module to notify
 *         the cellular controller to terminate the connection and put the modem to
 *         sleep.
 */
struct messaging_stop_connection_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(messaging_stop_connection_event);

/** @brief Outbound proto messages to be sent to the server (binary format).
 *         Published by the messaging module and consumed by the cellular controller.
 */
struct messaging_proto_out_event {
	struct event_header header;
	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(messaging_proto_out_event);

/** @brief Notify cellular_controller of a new modem fw version on the FTP server.
 *         Published by the messaging module and consumed by the cellular controller.
 */
struct messaging_mdm_fw_event {
	struct event_header header;
	char *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(messaging_mdm_fw_event);

/** @brief Notify cellular_controller of a new host address.
 *         Published by the messaging module and consumed by the cellular controller.
 */
struct messaging_host_address_event {
	struct event_header header;
	char address[STG_CONFIG_HOST_PORT_BUF_LEN];
};

EVENT_TYPE_DECLARE(messaging_host_address_event);

/** @brief Notify on change in the collar mode (Normal, teach, tracking)
 *         Published by the amc module and consumed by the messaging controller.
 */
struct update_collar_mode {
	struct event_header header;
	Mode collar_mode;
};

EVENT_TYPE_DECLARE(update_collar_mode);

/** @brief Notify on change in the collar status (Normal, unknown,
 *         off-animal, ...)
 *         Published by the amc module and consumed by the messaging controller.
 */
struct update_collar_status {
	struct event_header header;
	CollarStatus collar_status;
};

EVENT_TYPE_DECLARE(update_collar_status);

/** @brief Notify on change in the fence status.
 *         Published by the amc module and consumed by the messaging controller.
 */
struct update_fence_status {
	struct event_header header;
	FenceStatus fence_status;
};

EVENT_TYPE_DECLARE(update_fence_status);

/** @brief Notify messaging after successfully activating a new fence.
 *         Published by the amc module and consumed by the messaging controller.
 */
struct update_fence_version {
	struct event_header header;
	uint32_t fence_version;
	uint32_t total_fences;
};

EVENT_TYPE_DECLARE(update_fence_version);

/** @brief Notify messaging after successfully erasing the external flash.
 *         Published by the storage module and consumed by messaging.
 */
struct update_flash_erase {
	struct event_header header;
};

EVENT_TYPE_DECLARE(update_flash_erase);

/** @brief Notify messaging with new zap count (after giving a zap).
 *         Published by the amc module and consumed by messaging.
 */
struct update_zap_count {
	struct event_header header;
	uint16_t count;
};

EVENT_TYPE_DECLARE(update_zap_count);

/** @brief Notify amc after successfully downloading a new fence.
 *         Published by the messaging module and consumed by the amc.
 */
struct new_fence_available {
	struct event_header header;
	uint32_t new_fence_version;
};

EVENT_TYPE_DECLARE(new_fence_available);

/** @brief notify gps controller after successfully downloading and storing ano
 * data.
 * Published by the messaging module and consumed by the gps controller.
 * */
struct ano_ready {
	struct event_header header;
};

EVENT_TYPE_DECLARE(ano_ready);

/** @brief Notify messaging on warning tone.
 *         Published by the amc controller and consumed by the messaging module.
 *         To send to the server ASAP.
 */
struct animal_warning_event {
	struct event_header header;
	int16_t fence_dist;
};

EVENT_TYPE_DECLARE(animal_warning_event);

/** @brief Notify messaging on escape.
 *         Published by the amc controller and consumed by the messaging module 
 *         To send to the server ASAP.
 */
struct animal_escape_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(animal_escape_event);

/** @brief Notify cellular controller to check if cellular connection is ready.
 *Published by the messaging before sending out a new message to the server,
 * cellular controller should reply with connection_ready event.
 */
struct check_connection {
	struct event_header header;
};

EVENT_TYPE_DECLARE(check_connection);

/** @brief Notify cellular controller to check if cellular connection is ready.
 *Published by the messaging before sending out a new message to the server,
 * cellular controller should reply with connection_ready event.
 */
struct send_poll_request_now {
	struct event_header header;
};

EVENT_TYPE_DECLARE(send_poll_request_now);

/** @brief Published by AMC to notify other modules that correction
 *         has started.
 */
struct warn_correction_start_event {
	struct event_header header;
	int16_t fence_dist;
};

EVENT_TYPE_DECLARE(warn_correction_start_event);

/** @brief Published by AMC to notify other modules that correction
 *         has ended.
 */
struct warn_correction_end_event {
	struct event_header header;
	int16_t fence_dist;
};

EVENT_TYPE_DECLARE(warn_correction_end_event);

/** @brief Published by AMC to notify other modules that correction
 *         has paused.
 */
struct warn_correction_pause_event {
	struct event_header header;
	int16_t fence_dist;
	uint32_t warn_duration;
	Reason reason;
};

EVENT_TYPE_DECLARE(warn_correction_pause_event);

/** @brief Published by AMC to notify other modules that we gave a zap.
 */
struct amc_zapped_now_event {
	struct event_header header;
	int16_t fence_dist;
};

EVENT_TYPE_DECLARE(amc_zapped_now_event);

/** @brief Published by messaging to notify AMC modules to invalidate the
		current pasture.
*/
struct turn_off_fence_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(turn_off_fence_event);
