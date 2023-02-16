#include <event_manager.h>
#include "../../drivers/gnss/zephyr/gnss.h"

/** @brief GNSS data received from the GNSS driver. */
struct gnss_data {
	struct event_header header;
	gnss_t gnss_data;
	bool timed_out;
};

EVENT_TYPE_DECLARE(gnss_data);

/** @brief Set GNSS mode. */
struct gnss_set_mode_event {
	struct event_header header;
	gnss_mode_t mode;
};

EVENT_TYPE_DECLARE(gnss_set_mode_event);

/** @brief send out GNSS mode changed status **/
struct gnss_mode_changed_event {
	struct event_header header;
	gnss_mode_t mode;
};

EVENT_TYPE_DECLARE(gnss_mode_changed_event);
