#include <event_manager.h>
#include "../../drivers/gnss/zephyr/gnss.h"

/** @brief GNSS data rate. */
enum gnss_data_rate {
	LOW = 0, /* 1Hz */
	HIGH = 1 /* 4Hz */
};

enum gnss_mode {
	GNSSMODE_NOMODE = 0,
	GNSSMODE_INACTIVE = 1,
	GNSSMODE_PSM = 2,
	GNSSMODE_CAUTION = 3,
	GNSSMODE_MAX = 4,
	GNSSMODE_SIZE = 5
};

/** @brief Last GNSS fix received from the GNSS driver. */
struct new_gnss_fix {
	struct event_header header;
	gnss_last_fix_struct_t fix;
};

EVENT_TYPE_DECLARE(new_gnss_fix);


/** @brief Last GNSS data received from the GNSS driver. */
struct new_gnss_data {
	struct event_header header;
	gnss_struct_t gnss_data;
};

EVENT_TYPE_DECLARE(new_gnss_data);


/** @brief Set GNSS receiver data update rate. */
struct gnss_rate {
	struct event_header header;
	uint16_t rate;
};

EVENT_TYPE_DECLARE(gnss_rate);


/** @brief Set GNSS mode. */
struct gnss_set_mode {
	struct event_header header;
	enum gnss_mode mode;
};

EVENT_TYPE_DECLARE(gnss_set_mode);

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

/** @brief Switch on GNSS receiver. */
struct gnss_no_zone {
	struct event_header header;
};

EVENT_TYPE_DECLARE(gnss_no_zone);

