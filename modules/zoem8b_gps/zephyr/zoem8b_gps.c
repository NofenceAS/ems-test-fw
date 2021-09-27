

#define DT_DRV_COMPAT ublox_zoem8bspi
#include <drivers/gnss.h>
#include <drivers/gpio.h>
#include <init.h>
#include <logging/log.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ubx_controller.h"
#include <autoconf.h>
#include <devicetree.h>
#include <drivers/gps.h>
#include <drivers/spi.h>

LOG_MODULE_REGISTER(zoem8b_gps);

struct zoem8b_data {
	const struct device *dev;
	const struct device *bus;
	gps_event_handler_t handler;
	// struct gps_config current_cfg;
	atomic_t is_init;
	atomic_t is_active;
	atomic_t is_shutdown;
	atomic_t timeout_occurred;
	K_THREAD_STACK_MEMBER(thread_stack, CONFIG_ZOEM8B_GPS_THREAD_STACK_SIZE);
	struct k_thread thread;
	k_tid_t thread_id;
	struct k_sem thread_run_sem;
	struct k_work stop_work;
	struct k_work_delayable timeout_work;
	struct k_work_delayable blocked_work;
	struct k_work_sync work_sync;

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)
	struct spi_cs_control cs_ctrl;
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(spi) */
};

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)
struct zoem8b_spi_cfg {
	struct spi_config spi_conf;
	const char *cs_gpios_label;
	const struct device *reset_gpio_dev;
	gpio_pin_t reset_gpio_pin;
	gpio_dt_flags_t reset_gpio_flags;
};
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(spi) */

union zoem8b_bus_cfg {
#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
	uint16_t i2c_slv_addr;
#endif

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)
	const struct zoem8b_spi_cfg *spi_cfg;
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(spi) */
};

struct zoem8b_config {
	const char *bus_name;
	int (*bus_init)(const struct device *dev);
	const union zoem8b_bus_cfg bus_cfg;
	const char *extint_gpio_name;
	gpio_pin_t extint_pin;
};

int gps_zoem8b_init(const struct device *dev)
{
	struct zoem8b_data *zoem8b = dev->data;
	const struct zoem8b_config *cfg = dev->config;
	int status;
	uint8_t id;

	zoem8b->bus = device_get_binding(cfg->bus_name);
	if (!zoem8b->bus) {
		LOG_ERR("master not found: %s", cfg->bus_name);
		return -EINVAL;
	}

	cfg->bus_init(dev);

	LOG_INF("bus=%s", cfg->bus_name);

	return 0;
}

#define GPS_INIT_MAX_TRY_COUNT 50
#define UBX_CFG_CFG_MASK 0x00001F1F
#define NF_TRACE_CHAR(a) LOG_INF(a)

static UBX_CB_RET_t gps_callback(uint16_t ubxId, ubx_payload_t *p, uint16_t payload_len)
{
	uint8_t i;

	switch (ubxId) {
	case UBXID_NAV_PVT:
		LOG_INF("Lat %d Lon %d #sat %u\n", p->nav_pvt.lat, p->nav_pvt.lon,
			p->nav_pvt.numSV);
		/*
       m_GpsData.pvt_flags = p->nav_pvt.flags;
       m_GpsData.pvt_valid = p->nav_pvt.valid;
       m_GpsData.Lon = p->nav_pvt.lon;
       m_GpsData.Lat = p->nav_pvt.lat;
       m_GpsData.numSV = p->nav_pvt.numSV;
       m_GpsData.Speed = (uint16_t) p->nav_pvt.gSpeed; //cm/s
       m_GpsData.headVeh = (int16_t) (p->nav_pvt.headVeh / 1000);
       m_GpsData.headAcc = (uint16_t) (p->nav_pvt.headAcc / 1000); // Heading
       accuracy int32_t myTmpHeight = p->nav_pvt.height / 100; // Recalculate to
       dm if (myTmpHeight > INT16_MAX) { myTmpHeight = INT16_MAX;
       }
       m_GpsData.Height = (int16_t) myTmpHeight;
       if (p->nav_pvt.hAcc > UINT16_MAX * 100) {
       m_GpsData.hAccDm = UINT16_MAX;
       } else {
       m_GpsData.hAccDm = (uint16_t) (p->nav_pvt.hAcc / 100);
       }
       if (p->nav_pvt.vAcc > UINT16_MAX * 100) {
       m_GpsData.vAccDm = UINT16_MAX;
       } else {
       m_GpsData.vAccDm = (uint16_t) (p->nav_pvt.vAcc / 100);
       }

       // If bits 0,1,2 are set, we can trust the time, we don't care about bit
       4 for now if (((p->nav_pvt.valid & 0x07) == 0x07) && (p->nav_pvt.flags &
       1)) { myTime.year = p->nav_pvt.year; myTime.month = p->nav_pvt.month;
       myTime.day = p->nav_pvt.day;
       myTime.hour = p->nav_pvt.hour;
       myTime.minute = p->nav_pvt.min;
       myTime.second = p->nav_pvt.sec;
       uint32_t tempTime = time2unix(&myTime);
       if (tempTime != 0) {
       nf_set_unix_time(tempTime);
       }
       }
     */
		break;
		/*   case UBXID_NAV_DOP:
m_gps_sol_updated_flags |= (1 << 1);
m_GpsData.hDOP = p->nav_dop.hDOP;
break;

case UBXID_NAV_STATUS:
m_gps_sol_updated_flags |= (1 << 2);
m_GpsData.msss = p->nav_status.msss;
m_GpsData.ttff = p->nav_status.ttff;
break;

case UBXID_MGA_ACK_DATA0:
if (p->mga_ack_data0.msgId == UBXID_MGA_ANO_MSGID) {
#ifdef DEBUG_GPS
bool found = false;
#endif
for (i = 0; i < GPS_MAX_MGA_ANOS_IN_ACKBUF; i++) {
if (m_ano_state.ack_states[i].type == GPS_UBX_MGA_ACK_TYPE_WAITING
&& p->mga_ack_data0.msgPayloadStart[2] ==
m_ano_state.ack_states[i].svId
&& p->mga_ack_data0.msgPayloadStart[3] ==
m_ano_state.ack_states[i].gnssId) { m_ano_state.ack_states[i].type =
p->mga_ack_data0.type; m_ano_state.ack_states[i].infoCode =
p->mga_ack_data0.infoCode; m_ano_state.mga_ack_timestamp_ms = get16msTimer();
#ifdef   DEBUG_GPS
found = true;
#endif
break;
}
}

} else if (p->mga_ack_data0.msgId == UBXID_MGA_INI_TIME_UTC_MSGID) {
NF_TRACE("ano:MGA-INI:ack");
m_ano_state.mga_ack_timestamp_ms = get16msTimer();
m_ano_state.mga_ini_tim_ack_type = GPS_UBX_MGA_ACK_TYPE_WAITING;

} else {
NF_TRACE("?NAK?");
}
break;*/

	default:
		break;
	}

	return UBX_CB_CONTINUE;
}

static int GPS_Setup(const struct device *dev)
{
	struct zoem8b_data *data = dev->data;
	const struct zoem8b_config *cfg = dev->config;
	const struct spi_config *spi_cfg = &cfg->bus_cfg.spi_cfg->spi_conf;

	ubxc_init(gps_callback);

	uint8_t state = 0;
	uint16_t tryCount = 0;
	bool finished = false;

	while (!finished && tryCount++ < GPS_INIT_MAX_TRY_COUNT) {
		switch (state) {
		case 0:
			// Clear default config
			if (UBX_CFG_CFG(data->bus, spi_cfg, 0x01, UBX_CFG_CFG_MASK) != UBX_OK) {
				break;
			}
			state++;
		case 1:
			NF_TRACE_CHAR("b");
			/* Load default config */
			if (UBX_CFG_CFG(data->bus, spi_cfg, 0x03, UBX_CFG_CFG_MASK) != UBX_OK) {
				break;
			}
			state++;
		case 2:
			NF_TRACE_CHAR("c");
			if (UBX_CFG_PRT(data->bus, spi_cfg, 0, 0) != UBX_OK) {
				break;
			}
			state++;
		case 3:
			NF_TRACE_CHAR("d");
			if (UBX_CFG_PRT(data->bus, spi_cfg, 1, 0) != UBX_OK) {
				break;
			}
			state++;
		case 4:
			NF_TRACE_CHAR("-");
			// NOF-517: Do not config port 2
			//				if (UBX_CFG_PRT(2, 0) != UBX_OK) {
			//					break;
			//				}
			state++;
		case 5:
			NF_TRACE_CHAR("f");
			if (UBX_CFG_PRT(data->bus, spi_cfg, 3, 0) != UBX_OK) {
				break;
			}
			state++;
		case 6:
			NF_TRACE_CHAR("g");
			if (UBX_CFG_PRT(data->bus, spi_cfg, 4, 1) != UBX_OK) {
				break;
			}
			state++;
		case 7:
			NF_TRACE_CHAR("h");
			// no op
			state++;
		case 8:
			NF_TRACE_CHAR("i");
			if (UBX_CFG_MSG(data->bus, spi_cfg, UBXID_NAV_PVT, 1) != UBX_OK) {
				break;
			}
			state++;
		case 9:
			NF_TRACE_CHAR("j");
			if (UBX_CFG_MSG(data->bus, spi_cfg, UBXID_NAV_DOP, 1) != UBX_OK) {
				break;
			}
			state++;
		case 10:
			NF_TRACE_CHAR("k");
			if (UBX_CFG_MSG(data->bus, spi_cfg, UBXID_NAV_STATUS, 1) != UBX_OK) {
				break;
			}
			state++;
		case 11:
			NF_TRACE_CHAR("l");
			//                if (UBX_CFG_MSG(UBXID_MON_HW, 1) != UBX_OK) {
			//                    break;
			//                }
			// NOF-458 measure TX activity
			if (UBX_CFG_MSG(data->bus, spi_cfg, UBXID_MON_TXBUF, 1) != UBX_OK) {
				break;
			}
			state++;
		case 12:
			NF_TRACE_CHAR("m");
			if (UBX_CFG_GNSS(data->bus, spi_cfg) != UBX_OK) {
				break;
			}
			state++;
		case 13:
			NF_TRACE_CHAR("n");
			if (UBX_CFG_NAV5(data->bus, spi_cfg) != UBX_OK) {
				break;
			}
			state++;
		case 14:
			NF_TRACE_CHAR("o");
			if (UBX_CFG_NAVX5(data->bus, spi_cfg) != UBX_OK) {
				break;
			}
			state++;
		case 15:
			NF_TRACE_CHAR("p");
			// Save default config
			if (UBX_CFG_CFG(data->bus, spi_cfg, 0x02, UBX_CFG_CFG_MASK) != UBX_OK) {
				break;
			}
			state++;
		case 16:
			NF_TRACE_CHAR("q");
			if (UBX_CFG_INF(data->bus, spi_cfg, 0, 0x03) != UBX_OK) {
				break;
			}
			state++;
			/*case 17:NF_TRACE_CHAR("r");
         if (UBX_CFG_RST(GPSRESET_COLD, 0) != UBX_OK) {
         break;
         }*/
			state++;
		case 18:
			NF_TRACE_CHAR("s");
			finished = true;
		}
		k_sleep(K_MSEC(100));
	}
	return tryCount < GPS_INIT_MAX_TRY_COUNT ? -0 : -EAGAIN;
}

static int zoem8b_spi_raw_read(const struct device *dev, uint8_t *value, uint8_t len)
{
	int ret;

	struct zoem8b_data *data = dev->data;
	const struct zoem8b_config *cfg = dev->config;
	const struct spi_config *spi_cfg = &cfg->bus_cfg.spi_cfg->spi_conf;

	const struct spi_buf rx_buf = {
		.buf = value,
		.len = len,
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	ret = spi_transceive(data->bus, spi_cfg, NULL, &rx);
	if (ret) {
		LOG_ERR("spi_transceive %d\n", ret);
		return -EAGAIN;
	}
	return 0;
}

static int zoem8b_spi_init(const struct device *dev)
{
	struct zoem8b_data *data = dev->data;

	data->dev = dev;
	const struct zoem8b_config *cfg = dev->config;
	const struct zoem8b_spi_cfg *spi_cfg = cfg->bus_cfg.spi_cfg;

	if (spi_cfg->cs_gpios_label != NULL) {
		/* handle SPI CS thru GPIO if it is the case */
		data->cs_ctrl.gpio_dev = device_get_binding(spi_cfg->cs_gpios_label);
		if (!data->cs_ctrl.gpio_dev) {
			LOG_ERR("Unable to get GPIO SPI CS device");
			return -ENODEV;
		}

		LOG_DBG("SPI GPIO CS configured on %s:%u", spi_cfg->cs_gpios_label,
			data->cs_ctrl.gpio_pin);
	}

	int ret = GPS_Setup(dev);

	if (!ret) {
		LOG_ERR("Unable to init ZOE GPS");
		return ret;
	}

	return 0;
}

static void notify_event(const struct device *dev, struct gps_event *evt)
{
	struct zoem8b_data *drv_data = dev->data;

	if (drv_data->handler) {
		drv_data->handler(dev, evt);
	}
}

static void stop_work_fn(struct k_work *work)
{
	struct zoem8b_data *drv_data = CONTAINER_OF(work, struct zoem8b_data, stop_work);
	const struct device *dev = drv_data->dev;
	struct gps_event evt = { .type = GPS_EVT_SEARCH_STOPPED };

	notify_event(dev, &evt);
}

static void timeout_work_fn(struct k_work *work)
{
	struct zoem8b_data *drv_data = CONTAINER_OF(work, struct zoem8b_data, timeout_work);
	const struct device *dev = drv_data->dev;
	struct gps_event evt = { .type = GPS_EVT_SEARCH_TIMEOUT };

	atomic_set(&drv_data->timeout_occurred, 1);
	notify_event(dev, &evt);
}

static void blocked_work_fn(struct k_work *work)
{
	int retval = 0;
	struct zoem8b_data *drv_data = CONTAINER_OF(work, struct zoem8b_data, blocked_work);

	// TODO  retval = gps_priority_set(drv_data, true);
	if (retval != 0) {
		LOG_ERR("Failed to set GPS priority, error: %d", retval);
	}
}

static void cancel_works(struct zoem8b_data *drv_data, bool wait_for_completion)
{
	if (wait_for_completion) {
		k_work_cancel_delayable_sync(&drv_data->timeout_work, &drv_data->work_sync);
		k_work_cancel_delayable_sync(&drv_data->blocked_work, &drv_data->work_sync);
	} else {
		k_work_cancel_delayable(&drv_data->timeout_work);
		k_work_cancel_delayable(&drv_data->blocked_work);
	}
}

static int stop_gps(const struct device *dev, bool is_timeout)
{
	struct zoem8b_data *drv_data = dev->data;
	int retval;

	if (is_timeout) {
		LOG_DBG("Stopping GPS due to timeout");
	} else {
		LOG_DBG("Stopping GPS");
	}

	atomic_set(&drv_data->is_active, 0);

	// TODO
	LOG_ERR("Failed to stop GPS");
	return -EIO;
}
#ifdef CONFIG_PM_DEVICE
int bme280_pm_ctrl(const struct device *dev, uint32_t ctrl_command, void *context, pm_device_cb cb,
		   void *arg)
{
	// TODO
	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static int agps_write(const struct device *dev, enum gps_agps_type type, void *data,
		      size_t data_len)
{
	int err;
	struct zoem8b_data *drv_data = dev->data;

	err = -1; // TODO
	if (err < 0) {
		LOG_ERR("Failed to send A-GPS data to modem, errno: %d", errno);
		return -errno;
	}

	LOG_DBG("Sent A-GPS data to modem, type: %d", type);

	return 0;
}

static void gps_thread(int dev_ptr)
{
	struct device *dev = INT_TO_POINTER(dev_ptr);
	struct zoem8b_data *drv_data = dev->data;
	const struct zoem8b_config *cfg = dev->config;
	const struct spi_config *spi_cfg = &cfg->bus_cfg.spi_cfg->spi_conf;
	int len;
	bool operation_blocked = false;
	bool has_fix = false;
	struct gps_event evt = { .type = GPS_EVT_SEARCH_STARTED };
	uint8_t buf[64];

wait:
	k_sem_take(&drv_data->thread_run_sem, K_FOREVER);
	LOG_INF("GPS thread starting\n");

	evt.type = GPS_EVT_SEARCH_STARTED;
  notify_event(dev, &evt);

  while (true) {
	  // TODO, look at nrf9160 gps here
	  k_sleep(K_MSEC(1000));
	  has_fix = true;
	  ubxc_read_all(drv_data->bus, spi_cfg);
	  /*
       memset(buf,0x01,sizeof(buf));
       zoem8b_spi_raw_read(dev,buf, sizeof(buf));
       for (int i = 0; i < sizeof(buf); i++) {
       if (buf[i] != 0xFF) {
       LOG_HEXDUMP_INF(buf, sizeof(buf), "SPI");
       break;
       }
       }
     */

	  struct gps_event evt = { 0 };

	  /* There is no way of knowing if nrf_recv() blocks because the
     * GPS timeout/retry value has expired or the GPS has gotten a
     * fix. This check makes sure that a GPS_EVT_SEARCH_TIMEOUT is
     * not propagated upon a fix.
     */
	  if (!has_fix) {
		  /* If nrf_recv() blocks for more than one second,
       * a fix has been obtained or the GPS has timed out.
       * Schedule a delayed timeout work to be executed after
       * five seconds to make sure the appropriate event is
       * propagated upon a block.
       */
		  k_work_schedule(&drv_data->timeout_work, K_SECONDS(5));
    }
  }
}

static int init_thread(const struct device *dev)
{
	struct zoem8b_data *drv_data = dev->data;

	drv_data->thread_id =
		k_thread_create(&drv_data->thread, drv_data->thread_stack,
				K_THREAD_STACK_SIZEOF(drv_data->thread_stack),
				(k_thread_entry_t)gps_thread, (void *)dev, NULL, NULL,
				K_PRIO_PREEMPT(CONFIG_ZOEM8B_GPS_THREAD_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(drv_data->thread_id, "nrf9160_gps_driver");

	return 0;
}

static int init(const struct device *dev, gps_event_handler_t handler)
{
	struct zoem8b_data *drv_data = dev->data;
	int err;

	if (atomic_get(&drv_data->is_init)) {
		LOG_WRN("GPS is already initialized");

		return -EALREADY;
	}

	if (handler == NULL) {
		LOG_ERR("No event handler provided");
		return -EINVAL;
  }

  drv_data->handler = handler;

  k_work_init(&drv_data->stop_work, stop_work_fn);
  k_work_init_delayable(&drv_data->timeout_work, timeout_work_fn);
  k_work_init_delayable(&drv_data->blocked_work, blocked_work_fn);
  k_sem_init(&drv_data->thread_run_sem, 0, 1);

  err = init_thread(dev);
  if (err) {
	  LOG_ERR("Could not initialize GPS thread, error: %d", err);
	  return err;
  }

  atomic_set(&drv_data->is_init, 1);

  return 0;
}

static int start(const struct device *dev, struct gps_config *cfg)
{
	struct zoem8b_data *drv_data = dev->data;

	if (atomic_get(&drv_data->is_shutdown) == 1) {
		return -EHOSTDOWN;
	}

	if (atomic_get(&drv_data->is_active)) {
		LOG_DBG("GPS is already active. Clean up before restart");
		/* Try to cancel the works (one of them may already be running,
     * so it may be too late to cancel it and the corresponding
     * notification will be generated anyway).
     * Don't wait for the works to become idle to avoid a deadlock
     * when the function is called from the context of the work that
     * could not be canceled and would need to be finished here.
     */
		cancel_works(drv_data, false);
  }

  if (atomic_get(&drv_data->is_init) != 1) {
    LOG_WRN("GPS must be initialized first");
    return -ENODEV;
  }

  atomic_set(&drv_data->is_active, 1);
  atomic_set(&drv_data->timeout_occurred, 0);
  k_sem_give(&drv_data->thread_run_sem);

  LOG_DBG("GPS operational");

  return 0;
}

static int stop(const struct device *dev)
{
	int err = 0;
	struct zoem8b_data *drv_data = dev->data;

	if (atomic_get(&drv_data->is_shutdown) == 1) {
		return -EHOSTDOWN;
	}

	/* Don't wait here for those works to become idle, as this function
   * may be invoked from those works (for example, when the GPS is to be
   * stopped directly on the GPS_EVT_SEARCH_TIMEOUT event).
   * One of those works may already be running, and the attempt to cancel
   * it will fail then, but since the GPS_EVT_SEARCH_STOPPED notification
   * is submitted below through the same work queue, it will be processed
   * after any notification from that work that could not be canceled.
   */
	cancel_works(drv_data, false);

	if (atomic_get(&drv_data->is_active) == 0) {
		/* The GPS is already stopped, attempting to stop it again would
     * result in an error. Instead, notify that it's stopped.
     */
		goto notify;
	}

	err = stop_gps(dev, false);
	if (err) {
		return err;
	}

notify:
	/* Offloading event dispatching to workqueue, as also other events
   * in this driver are sent from context different than the calling
   * context.
   */
	k_work_submit(&drv_data->stop_work);

	return 0;
}

static const struct gps_driver_api gps_api_funcs = {
    .init = init,
    .start = start,
    .stop = stop,
    .agps_write = agps_write,
};

#define ZOEM8B_DEVICE_INIT(inst)                                                                   \
	DEVICE_DT_INST_DEFINE(inst, gps_zoem8b_init, NULL, &zoem8b_data_##inst,                    \
			      &zoem8b_config_##inst, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,     \
			      &gps_api_funcs);

#define IS_LSM303AGR_DEV(inst) DT_NODE_HAS_COMPAT(DT_DRV_INST(inst), st_lsm303agr_accel)

#define DISC_PULL_UP(inst) DT_INST_PROP(inst, disconnect_sdo_sa0_pull_up)

/*
 * Instantiation macros used when a device is on a SPI bus.
 */

#define ZOEM8B_HAS_CS(inst) DT_INST_SPI_DEV_HAS_CS_GPIOS(inst)

#define ZOEM8B_DATA_SPI_CS(inst)                                                                   \
	{                                                                                          \
		.cs_ctrl = {                                                                       \
			.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(inst),                            \
			.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(inst),                     \
			.delay = 20,                                                               \
		},                                                                                 \
	}

#define ZOEM8B_DATA_SPI(inst) COND_CODE_1(ZOEM8B_HAS_CS(inst), (ZOEM8B_DATA_SPI_CS(inst)), ({}))

#define ZOEM8B_SPI_CS_PTR(inst)                                                                    \
	COND_CODE_1(ZOEM8B_HAS_CS(inst), (&(zoem8b_data_##inst.cs_ctrl)), (NULL))

#define ZOEM8B_SPI_CS_LABEL(inst)                                                                  \
	COND_CODE_1(ZOEM8B_HAS_CS(inst), (DT_INST_SPI_DEV_CS_GPIOS_LABEL(inst)), (NULL))

#define INIT_RESET_GPIO_FIELDS(idx)                                                                \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(idx, reset_gpios),                                       \
		    (.reset_gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(idx), reset_gpios)), \
		     .reset_gpio_pin = DT_INST_GPIO_PIN(idx, reset_gpios),                         \
		     .reset_gpio_flags = DT_INST_GPIO_FLAGS(idx, reset_gpios)),                    \
		    ())

#define ZOEM8B_SPI_CFG(inst)                                                                       \
	(&(struct zoem8b_spi_cfg){                                                   \
      .spi_conf =                                                              \
          {                                                                    \
              .frequency = DT_INST_PROP(inst, spi_max_frequency),              \
              .operation = (SPI_WORD_SET(8) | SPI_OP_MODE_MASTER),             \
              .slave = DT_INST_REG_ADDR(inst),                                 \
              .cs = ZOEM8B_SPI_CS_PTR(inst),                                   \
          },                                                                   \
      .cs_gpios_label = ZOEM8B_SPI_CS_LABEL(inst),                             \
      INIT_RESET_GPIO_FIELDS(inst)})

#define ZOEM8B_CONFIG_SPI(inst)                                                                    \
	{                                                                                          \
		.bus_name = DT_INST_BUS_LABEL(inst), .bus_init = zoem8b_spi_init,                  \
		.bus_cfg = { .spi_cfg = ZOEM8B_SPI_CFG(inst) },                                    \
	}

#define ZOEM8B_DEFINE_SPI(inst)                                                                    \
	static struct zoem8b_data zoem8b_data_##inst = ZOEM8B_DATA_SPI(inst);                      \
	static const struct zoem8b_config zoem8b_config_##inst = ZOEM8B_CONFIG_SPI(inst);          \
	ZOEM8B_DEVICE_INIT(inst)

/*
 * Main instantiation macro. Use of COND_CODE_1() selects the right
 * bus-specific macro at preprocessor time.
 */

#define ZOEM8B_DEFINE(inst)                                                                        \
	COND_CODE_1(DT_INST_ON_BUS(inst, spi), (ZOEM8B_DEFINE_SPI(inst)), (ZOEM8B_DEFINE_I2C(inst)))

DT_INST_FOREACH_STATUS_OKAY(ZOEM8B_DEFINE)