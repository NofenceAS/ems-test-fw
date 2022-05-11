#include <event_manager.h>
#include "../../drivers/gnss/zephyr/gnss.h"

/** @brief GNSS data rate. */
enum gnss_data_rate {
	LOW = 0, /* 1Hz */
	HIGH = 1 /* 4Hz */
};

/** @brief GNSS data received from the GNSS driver. */
struct gnss_data {
	struct event_header header;
	gnss_t gnss_data;
	bool timed_out;
};

EVENT_TYPE_DECLARE(gnss_data);

/** @brief Set GNSS receiver data update rate. */
struct gnss_rate {
	struct event_header header;
	uint16_t rate;
};

EVENT_TYPE_DECLARE(gnss_rate);

/** @brief Set GNSS mode. */
struct gnss_set_mode_event {
	struct event_header header;
	gnss_mode_t mode;
};

EVENT_TYPE_DECLARE(gnss_set_mode_event);

/** @brief Switch off GNSS receiver. e.g: when in beacon. */
struct gnss_switch_off {
	struct event_header header;
};

EVENT_TYPE_DECLARE(gnss_switch_off);

/** @brief Switch on GNSS receiver. */
struct gnss_switch_on {
	struct event_header header;
};

EVENT_TYPE_DECLARE(gnss_switch_on);
