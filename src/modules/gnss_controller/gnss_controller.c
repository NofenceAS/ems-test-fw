#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include "gnss_controller.h"
#include "gnss_controller_events.h"
#include "error_event.h"
#include "gnss.h"
#include "kernel.h"
#include "diagnostics_events.h"

#define STACK_SIZE 1024
#define PRIORITY 7

#define GNSS_1SEC 1000

K_SEM_DEFINE(new_data_sem, 0, 1);

#define MODULE gnss_controller
LOG_MODULE_REGISTER(MODULE, CONFIG_GNSS_CONTROLLER_LOG_LEVEL);

#define MS_IN_49_DAYS 4233600000
static _Noreturn void publish_gnss_data(void *ctx);
static int gnss_data_update_cb(const gnss_t *);
static void gnss_timed_out(void);
static int gnss_set_mode(gnss_mode_t mode, bool wakeup);
static void gnss_thread_fn(void);

static gnss_t gnss_data_buffer;
static gnss_mode_t current_mode = GNSSMODE_NOMODE;
static uint16_t current_rate_ms = UINT16_MAX;
const struct device *gnss_dev = NULL;
static uint8_t gnss_reset_count = 0;
static uint8_t gnss_timeout_count = 0;

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
static int diagnostic_force_gnss_mode = GNSSMODE_MAX;
static int last_force_gnss_mode = GNSSMODE_MAX;
#endif

K_THREAD_STACK_DEFINE(pub_gnss_stack, STACK_SIZE);
K_THREAD_DEFINE(send_to_gnss, CONFIG_GNSS_STACK_SIZE, gnss_thread_fn, NULL, NULL, NULL,
		K_PRIO_COOP(CONFIG_GNSS_THREAD_PRIORITY), 0, 0);

enum gnss_action_e { GNSS_ACTION_NUL = 0, GNSS_ACTION_SET_MODE };

typedef struct gnss_msgq_t {
	enum gnss_action_e action;
	void *arg;
} gnss_msgq_t;

K_MSGQ_DEFINE(gnss_msgq, sizeof(gnss_msgq_t), 1, 4);

static void gnss_thread_fn(void)
{
	while (true) {
		gnss_msgq_t msg;
		k_msgq_get(&gnss_msgq, &msg, K_FOREVER);

		switch (msg.action) {
		case GNSS_ACTION_SET_MODE: {
			LOG_DBG("setting mode");
			int rc = 0;
			int retries = CONFIG_GNSS_CMD_RETRIES;
			gnss_mode_t mode = (gnss_mode_t)(msg.arg);
			do {
				rc = gnss_set_mode(mode, true);
			} while ((retries-- > 0) && (rc != 0));

			if (rc == 0) {
				current_mode = mode;
			} else {
				LOG_ERR("Failed to set mode %d with %d retries", rc,
					CONFIG_GNSS_CMD_RETRIES);
				char *msg = "Failed to set GNSS receiver mode";
				nf_app_error(ERR_GNSS_CONTROLLER, rc, msg, sizeof(*msg));
			}
			/* Send out an event (to the amc_handler)to answer with current mode */
			struct gnss_mode_changed_event *ev = new_gnss_mode_changed_event();
			ev->mode = current_mode;
			EVENT_SUBMIT(ev);
			break;
		}
		default:
			LOG_ERR("Unrecognized action %d", msg.action);
		}
	}
}

struct k_thread pub_gnss_thread;
static bool initialized = false;

#if defined(CONFIG_TEST)
k_tid_t pub_gnss_thread_id;
#endif

/** @brief Sends a timeout event from the GNSS controller */
static void gnss_controller_send_timeout_event(void)
{
	gnss_t gnss_no_fix;
	memset(&gnss_no_fix, 0, sizeof(gnss_t));

	struct gnss_data *new_data = new_gnss_data();
	new_data->gnss_data = gnss_no_fix;
	new_data->timed_out = true;

	EVENT_SUBMIT(new_data);
}

/** @brief Reset and initialize GNSS  */
static int gnss_controller_reset_and_setup_gnss(uint16_t mask)
{
	int ret = gnss_wakeup(gnss_dev);
	if (ret != 0) {
		LOG_ERR("gnss_wakeup failed %d", ret);
		return ret;
	}

	ret = gnss_reset(gnss_dev, mask, GNSS_RESET_MODE_HW_IMMEDIATELY);
	if (ret != 0) {
		LOG_ERR("gnss_reset failed %d", ret);
		return ret;
	}

	/* @todo config in BBR is cleared for HW resets, we must re-configure for now */
	/* See https://content.u-blox.com/sites/default/files/documents/MIA-M10Q_IntegrationManual_UBX-21028173.pdf 2.1 */
	ret = gnss_setup(gnss_dev, false);
	if (ret != 0) {
		LOG_ERR("gnss_setup failed %d", ret);
		return ret;
	}
	ret = gnss_set_mode(current_mode, false);
	if (ret != 0) {
		LOG_ERR("gnss_set_mode %d", ret);
		return ret;
	}
	return 0;
}

int gnss_controller_setup(void)
{
	int ret = -ENOSYS;
	struct ublox_mon_ver mia_m10_versions;

	ret = gnss_set_data_cb(gnss_dev, gnss_data_update_cb);
	if (ret != 0) {
		LOG_WRN("Failed to register data CB! %d", ret);
		return ret;
	}

	ret = gnss_wakeup(gnss_dev);
	if (ret != 0) {
		LOG_WRN("gnss_wakeup failed %d", ret);
		return ret;
	}

	ret = gnss_setup(gnss_dev, false);
	if (ret != 0) {
		LOG_WRN("gnss_setup failed %d", ret);
		return ret;
	}
	// Default values to please ztest
	mia_m10_versions.swVersion[0] = 0;
	mia_m10_versions.hwVersion[0] = 0;
	// Query the mia m10 for SW version and HW version and print them.
	ret = gnss_version_get(gnss_dev, &mia_m10_versions);
	if (ret != 0) {
		LOG_WRN("Failed to get version of mia m10! %d", ret);
		return ret;
	}

	LOG_INF("The GNSS SW Version:%s", log_strdup(mia_m10_versions.swVersion));
	LOG_INF("The GNSS HW Version:%s", log_strdup(mia_m10_versions.hwVersion));
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
	struct gnss_fwhw_info_event *ev = new_gnss_fwhw_info_event();
	ret = gnss_version_get(gnss_dev, &ev->gnss_fwhw);
	if (ret != 0) {
		LOG_WRN("Failed to get version of mia m10! %d", ret);
		return ret;
	}
	EVENT_SUBMIT(ev);
#endif
	return ret;
}

int gnss_controller_init_retry_softcold_setup(void)
{
	int ret = -ENOSYS;
	uint8_t gnss_init_count = 0;

	while (gnss_init_count < CONFIG_GNSS_MAX_COUNT_INIT) {
		LOG_INF(" Trying INIT [count=%d, ret=%d]", gnss_init_count, ret);
		ret = gnss_controller_setup();
		if (ret == 0) {
			return ret;
		}
		gnss_init_count++;
	}

	return ret;
}

int gnss_controller_init_retry_softcold(void)
{
	int ret = -ENOSYS;
	gnss_reset_count = 0;

	ret = gnss_controller_init_retry_softcold_setup();
	if (ret == 0) {
		return ret;
	}

	while (gnss_reset_count < CONFIG_GNSS_MAX_COUNT_SOFT_RESTART) {
		ret = gnss_reset(gnss_dev, GNSS_RESET_MASK_COLD, GNSS_RESET_MODE_HW_IMMEDIATELY);

		gnss_reset_count++;
		LOG_INF("RESTART [ iteration=%d, ret=%d ]", gnss_reset_count, ret);

		ret = gnss_controller_init_retry_softcold_setup();
		if (ret == 0) {
			return ret;
		}
	}

	return ret;
}

int gnss_controller_init_retry(void)
{
	int ret = -ENOSYS;

	ret = gnss_controller_init_retry_softcold();
	if (ret == 0) {
		return ret;
	}
	// If SOFT-COLD resets fail, try to toggle the resetn pin for a hard reset.
	ret = gnss_resetn_pin(gnss_dev);
	if (ret != 0) {
		char *msg = "gnss_resetn_pin toggle failed";
		nf_app_error(ERR_GNSS_CONTROLLER, ret, msg, sizeof(*msg));
		return ret;
	}

	ret = gnss_controller_init_retry_softcold();

	return ret;
}

int gnss_controller_init(void)
{
	current_mode = GNSSMODE_NOMODE;
	gnss_timeout_count = 0;
	int ret;

	gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));
	if (!gnss_dev) {
		return -ENODEV;
	}

	ret = gnss_controller_init_retry();
	if (ret != 0) {
		char *msg = "gnss init failed after using the resetn pin";
		nf_app_error(ERR_GNSS_CONTROLLER, ret, msg, sizeof(*msg));
		return ret;
	}

#if defined(CONFIG_TEST)
	pub_gnss_thread_id =
#endif
		k_thread_create(&pub_gnss_thread, pub_gnss_stack,
				K_KERNEL_STACK_SIZEOF(pub_gnss_stack),
				(k_thread_entry_t)publish_gnss_data, (void *)NULL, NULL, NULL,
				PRIORITY, 0, K_NO_WAIT);
	return 0;
}

static _Noreturn void publish_gnss_data(void *ctx)
{
	int ret;
	int timeout_ms;
	while (true) {
		if (current_rate_ms == UINT16_MAX ||
		    current_rate_ms < CONFIG_GNSS_MINIMUM_ALLOWED_GNSS_RATE) {
			timeout_ms = CONFIG_GNSS_DEFAULT_TIMEOUT_RATE_MS;
		} else {
			/* Allow some slack on GNSS solution */
			timeout_ms = current_rate_ms + CONFIG_GNSS_TIMEOUT_SLACK_MS;
		}
		if ((ret = k_sem_take(&new_data_sem, K_MSEC(timeout_ms))) == 0) {
			gnss_reset_count = 0;
			gnss_timeout_count = 0;
			struct gnss_data *new_data = new_gnss_data();
			new_data->gnss_data = gnss_data_buffer;
			new_data->timed_out = false;
			LOG_DBG("  GNSS data: %d, %d, %d, %d, %d", gnss_data_buffer.latest.lon,
				gnss_data_buffer.latest.lat, gnss_data_buffer.latest.pvt_flags,
				gnss_data_buffer.latest.h_acc_dm, gnss_data_buffer.latest.num_sv);
			EVENT_SUBMIT(new_data);
			initialized = true;
		} else {
			if (initialized && current_mode != GNSSMODE_INACTIVE) {
				gnss_timed_out();
			}
		}
		if (gnss_data_buffer.latest.msss >= MS_IN_49_DAYS && gnss_reset_count == 0) {
			gnss_controller_reset_and_setup_gnss(GNSS_RESET_MASK_COLD);
		}
	}
}

static int gnss_data_update_cb(const gnss_t *data)
{
	memcpy(&gnss_data_buffer, data, sizeof(gnss_t));
	k_sem_give(&new_data_sem);
	return 0;
}

static int gnss_set_mode(gnss_mode_t mode, bool wakeup)
{
	int ret;

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
	wakeup = false;
	if ((diagnostic_force_gnss_mode != last_force_gnss_mode) &&
	    (last_force_gnss_mode == GNSSMODE_INACTIVE)) {
		last_force_gnss_mode = diagnostic_force_gnss_mode;
		wakeup = true;
		LOG_INF("EMS GNSS mode: new %d last %d", diagnostic_force_gnss_mode,
			last_force_gnss_mode);
	}
	mode = diagnostic_force_gnss_mode;
	LOG_INF("EMS GNSS mode: %d", mode);
#endif

	if (wakeup) {
		ret = gnss_wakeup(gnss_dev);
		if (ret != 0) {
			LOG_ERR("gnss_wakeup failed %d", ret);
			return ret;
		}
	}

	if (mode == GNSSMODE_INACTIVE) {
		ret = gnss_set_backup_mode(gnss_dev);
		if (ret != 0) {
			LOG_ERR("failed to set GNSS in backup %d", ret);
			return ret;
		}
		current_rate_ms = UINT16_MAX;
	} else if (mode == GNSSMODE_PSM || mode == GNSSMODE_CAUTION || mode == GNSSMODE_MAX) {
		ret = gnss_set_power_mode(gnss_dev, mode);
		if (ret != 0) {
			LOG_ERR("failed to set GNSS to mode %u %d", mode, ret);
			return ret;
		}
		ret = gnss_get_rate(gnss_dev, &current_rate_ms);
		if (ret != 0) {
			LOG_ERR("failed to get rate %d", ret);
			return ret;
		}
	}

	return 0;
}

static bool gnss_controller_event_handler(const struct event_header *eh)
{
	if (is_gnss_set_mode_event(eh)) {
		struct gnss_set_mode_event *ev = cast_gnss_set_mode_event(eh);
		LOG_DBG("MODE = %d old = %d ", ev->mode, current_mode);
		if (ev->mode != current_mode) {
			LOG_DBG("setting mode");
			gnss_msgq_t msg;
			msg.arg = (void *)(ev->mode);
			msg.action = GNSS_ACTION_SET_MODE;
			k_msgq_put(&gnss_msgq, &msg, K_NO_WAIT);
		}
		return false;
	}
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
	else if (is_diag_thread_cntl_event(eh)) {
		struct diag_thread_cntl_event *event = cast_diag_thread_cntl_event(eh);
		diagnostic_force_gnss_mode = event->force_gnss_mode;
		LOG_WRN("Force gnss mode = %d", diagnostic_force_gnss_mode);
		return false;
	}
#endif
	return false;
}

EVENT_LISTENER(MODULE, gnss_controller_event_handler);
EVENT_SUBSCRIBE(MODULE, gnss_set_mode_event);
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
EVENT_SUBSCRIBE(MODULE, diag_thread_cntl_event);
#endif
/**
 * @brief Handles GNSS timeouts when no messages has been received. 
 *        Will reset GNSS with various modes depending on reset count. 
 *
 * @return None
 *
 */
static void gnss_timed_out(void)
{
	/* Always send an event */
	gnss_timeout_count++;
	gnss_controller_send_timeout_event();

	/* be much more careful to send reset events than timeouts, as this might stress the GNSS receiver */
	if (gnss_timeout_count > CONFIG_GNSS_TIMEOUTS_BEFORE_RESET) {
		gnss_timeout_count = 0;
		LOG_DBG("resets GNSS %d", gnss_reset_count);
		if (gnss_reset_count < 1) {
			gnss_reset_count++;
			gnss_controller_reset_and_setup_gnss(GNSS_RESET_MASK_HOT);
		} else if (gnss_reset_count < 2) {
			gnss_reset_count++;
			gnss_controller_reset_and_setup_gnss(GNSS_RESET_MASK_WARM);
		} else if (gnss_reset_count >= 2) {
			gnss_reset_count++;
			gnss_controller_reset_and_setup_gnss(GNSS_RESET_MASK_COLD);
		}
	}
}
