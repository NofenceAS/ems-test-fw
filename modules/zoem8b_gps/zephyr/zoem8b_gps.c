

#define DT_DRV_COMPAT ublox_zoem8bspi
#include <drivers/gpio.h>
#include <drivers/gps.h>
#include <init.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logging/log.h>
#include <math.h>

#include <autoconf.h>
#include <devicetree.h>
#include <drivers/spi.h>

LOG_MODULE_REGISTER(zoem8b_gps);

/*
struct zoem8b_spi_config {
  const char *spi_bus;
  struct spi_config spi_cfg;
  const char *cs_gpio;
  gpio_pin_t cs_pin;
};
*/

struct lis2dh_data {
  const struct device *dev;
  const struct device *bus;
  gps_event_handler_t handler;
  struct gps_config current_cfg;
  atomic_t is_init;
  atomic_t is_active;
  atomic_t is_shutdown;
  atomic_t timeout_occurred;
  K_THREAD_STACK_MEMBER(thread_stack,CONFIG_ZOEM8B_GPS_THREAD_STACK_SIZE);
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
struct lis2dh_spi_cfg {
  struct spi_config spi_conf;
  const char *cs_gpios_label;
};
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(spi) */

union lis2dh_bus_cfg {
#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
  uint16_t i2c_slv_addr;
#endif

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)
  const struct lis2dh_spi_cfg *spi_cfg;
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(spi) */
};

struct lis2dh_config {
  const char *bus_name;
  int (*bus_init)(const struct device *dev);
  const union lis2dh_bus_cfg bus_cfg;
  const char *extint_gpio_name;
  gpio_pin_t extint_pin;

};

int gps_lis2dh_init(const struct device *dev)
{
  struct lis2dh_data *lis2dh = dev->data;
  const struct lis2dh_config *cfg = dev->config;
  int status;
  uint8_t id;

  lis2dh->bus = device_get_binding(cfg->bus_name);
  if (!lis2dh->bus) {
    LOG_ERR("master not found: %s", cfg->bus_name);
    return -EINVAL;
  }

  cfg->bus_init(dev);


  LOG_INF("bus=%s",cfg->bus_name);

 return 0;
}


static int lis2dh_spi_init(const struct device *dev)
{
  struct lis2dh_data *data = dev->data;
  data->dev = dev;
  const struct lis2dh_config *cfg = dev->config;
  const struct lis2dh_spi_cfg *spi_cfg = cfg->bus_cfg.spi_cfg;


  if (spi_cfg->cs_gpios_label != NULL) {

    /* handle SPI CS thru GPIO if it is the case */
    data->cs_ctrl.gpio_dev =
        device_get_binding(spi_cfg->cs_gpios_label);
    if (!data->cs_ctrl.gpio_dev) {
      LOG_ERR("Unable to get GPIO SPI CS device");
      return -ENODEV;
    }

    LOG_DBG("SPI GPIO CS configured on %s:%u",
            spi_cfg->cs_gpios_label, data->cs_ctrl.gpio_pin);
  }

  return 0;
}

static int zoem8b_spi_raw_read(const struct device *dev,
                           uint8_t *value, uint8_t len)
{
  int ret;

  struct lis2dh_data *data = dev->data;
  const struct lis2dh_config *cfg = dev->config;
  const struct spi_config *spi_cfg = &cfg->bus_cfg.spi_cfg->spi_conf;

  uint8_t buffer_tx[64];
 // uint8_t buffer_1[] = {0xB5,0x62,0x0A,0x04,0x00,0x00,0x0E,0x34};
  uint8_t buffer_1[] = {0x5B,0x62,0x06,0x09,0x0c,0x00,0x1f,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x59,0x58};
  memset(buffer_tx,0xFF,sizeof(buffer_tx));
  memcpy(buffer_tx,buffer_1,sizeof(buffer_1));

  const struct spi_buf tx_buf = {
      .buf = buffer_1,
      .len = sizeof(buffer_1),
  };
  const struct spi_buf_set tx = {
      .buffers = &tx_buf,
      .count = 1
  };
  const struct spi_buf rx_buf = {
          .buf = value,
          .len = len,
  };
  const struct spi_buf_set rx = {
      .buffers = &rx_buf,
      .count = 1
  };


  if (len > 64) {
    return -EIO;
  }
  ret = spi_transceive(data->bus, spi_cfg, &tx, &rx);
  //ret = spi_transceive(data->bus, spi_cfg, &tx, &rx);
  if (ret) {
    LOG_ERR("spi_transceive %d\n",ret);
    return -EIO;
  }
  return 0;

}

static void notify_event(const struct device *dev, struct gps_event *evt)
{
  struct lis2dh_data *drv_data = dev->data;

  if (drv_data->handler) {
    drv_data->handler(dev, evt);
  }
}

static void stop_work_fn(struct k_work *work)
{
  struct lis2dh_data *drv_data =
      CONTAINER_OF(work, struct lis2dh_data, stop_work);
  const struct device *dev = drv_data->dev;
  struct gps_event evt = {
      .type = GPS_EVT_SEARCH_STOPPED
  };

  notify_event(dev, &evt);
}

static void timeout_work_fn(struct k_work *work)
{
  struct lis2dh_data *drv_data =
      CONTAINER_OF(work, struct lis2dh_data, timeout_work);
  const struct device *dev = drv_data->dev;
  struct gps_event evt = {
      .type = GPS_EVT_SEARCH_TIMEOUT
  };
  atomic_set(&drv_data->timeout_occurred, 1);
  notify_event(dev, &evt);
}

static void blocked_work_fn(struct k_work *work)
{
  int retval = 0;
  struct lis2dh_data *drv_data =
      CONTAINER_OF(work, struct lis2dh_data, blocked_work);

 // TODO  retval = gps_priority_set(drv_data, true);
  if (retval != 0) {
    LOG_ERR("Failed to set GPS priority, error: %d", retval);
  }
}

static void cancel_works(struct lis2dh_data *drv_data,
                         bool wait_for_completion)
{
  if (wait_for_completion) {
    k_work_cancel_delayable_sync(&drv_data->timeout_work,
                                 &drv_data->work_sync);
    k_work_cancel_delayable_sync(&drv_data->blocked_work,
                                 &drv_data->work_sync);
  } else {
    k_work_cancel_delayable(&drv_data->timeout_work);
    k_work_cancel_delayable(&drv_data->blocked_work);
  }
}

static int stop_gps(const struct device *dev, bool is_timeout)
{
  struct lis2dh_data *drv_data = dev->data;
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
int bme280_pm_ctrl(const struct device *dev, uint32_t ctrl_command,
                   void *context, pm_device_cb cb, void *arg)
{
  // TODO
  return 0;
}
#endif /* CONFIG_PM_DEVICE */

static int agps_write(const struct device *dev, enum gps_agps_type type,
                      void *data, size_t data_len)
{
  int err;
  struct lis2dh_data *drv_data = dev->data;

  err = -1;  // TODO
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
  struct lis2dh_data *drv_data = dev->data;
  int len;
  bool operation_blocked = false;
  bool has_fix = false;
  struct gps_event evt = {
      .type = GPS_EVT_SEARCH_STARTED
  };
  uint8_t buf[64];

wait:
  k_sem_take(&drv_data->thread_run_sem, K_FOREVER);
  LOG_INF("GPS thread starting\n");

  evt.type = GPS_EVT_SEARCH_STARTED;
  notify_event(dev, &evt);

  while (true) {
    // TODO, look at nrf9160 gps here
    k_sleep(K_MSEC(500));
    has_fix = true;
    memset(buf,0x01,sizeof(buf));
    zoem8b_spi_raw_read(dev,buf, sizeof(buf));
    for (int i = 0; i < sizeof(buf); i++) {
      if (buf[i] != 0xFF) {
        LOG_HEXDUMP_INF(buf, sizeof(buf), "SPI");
        break;
      }
    }

    struct gps_event evt = {0};

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
  struct lis2dh_data *drv_data = dev->data;

  drv_data->thread_id = k_thread_create(
      &drv_data->thread, drv_data->thread_stack,
      K_THREAD_STACK_SIZEOF(drv_data->thread_stack),
      (k_thread_entry_t)gps_thread, (void *)dev, NULL, NULL,
      K_PRIO_PREEMPT(CONFIG_ZOEM8B_GPS_THREAD_PRIORITY),
      0, K_NO_WAIT);
  k_thread_name_set(drv_data->thread_id, "nrf9160_gps_driver");

  return 0;
}

static int init(const struct device *dev, gps_event_handler_t handler)
{
  struct lis2dh_data *drv_data = dev->data;
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
    LOG_ERR("Could not initialize GPS thread, error: %d",
            err);
    return err;
  }

  atomic_set(&drv_data->is_init, 1);

  return 0;
}

static int start(const struct device *dev, struct gps_config *cfg)
{

  struct lis2dh_data *drv_data = dev->data;

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
  struct lis2dh_data *drv_data = dev->data;

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


#define LIS2DH_DEVICE_INIT(inst)					\
	DEVICE_DT_INST_DEFINE(inst,					\
			    gps_lis2dh_init,				\
			    NULL,					\
			    &lis2dh_data_##inst,			\
			    &lis2dh_config_##inst,			\
			    POST_KERNEL,				\
			    CONFIG_SENSOR_INIT_PRIORITY,		\
			    &gps_api_funcs);

#define IS_LSM303AGR_DEV(inst) \
	DT_NODE_HAS_COMPAT(DT_DRV_INST(inst), st_lsm303agr_accel)

#define DISC_PULL_UP(inst) \
	DT_INST_PROP(inst, disconnect_sdo_sa0_pull_up)

/*
 * Instantiation macros used when a device is on a SPI bus.
 */

#define LIS2DH_HAS_CS(inst) DT_INST_SPI_DEV_HAS_CS_GPIOS(inst)

#define LIS2DH_DATA_SPI_CS(inst)					\
	{ .cs_ctrl = {							\
		.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(inst),		\
		.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(inst), \
		.delay = 20,                                         \
		},							\
	}

#define LIS2DH_DATA_SPI(inst)						\
	COND_CODE_1(LIS2DH_HAS_CS(inst),				\
		    (LIS2DH_DATA_SPI_CS(inst)),				\
		    ({}))

#define LIS2DH_SPI_CS_PTR(inst)						\
	COND_CODE_1(LIS2DH_HAS_CS(inst),				\
		    (&(lis2dh_data_##inst.cs_ctrl)),			\
		    (NULL))

#define LIS2DH_SPI_CS_LABEL(inst)					\
	COND_CODE_1(LIS2DH_HAS_CS(inst),				\
		    (DT_INST_SPI_DEV_CS_GPIOS_LABEL(inst)), (NULL))

#define LIS2DH_SPI_CFG(inst)						\
	(&(struct lis2dh_spi_cfg) {					\
		.spi_conf = {						\
			.frequency =					\
				DT_INST_PROP(inst, spi_max_frequency),	\
			.operation = (SPI_WORD_SET(8) |	SPI_OP_MODE_MASTER | SPI_MODE_CPHA ),         \
			.slave = DT_INST_REG_ADDR(inst),		\
			.cs = LIS2DH_SPI_CS_PTR(inst),			\
		},							\
		.cs_gpios_label = LIS2DH_SPI_CS_LABEL(inst),		\
	})



#define LIS2DH_CONFIG_SPI(inst)						\
	{								\
		.bus_name = DT_INST_BUS_LABEL(inst),			\
		.bus_init = lis2dh_spi_init,				\
		.bus_cfg = { .spi_cfg = LIS2DH_SPI_CFG(inst)	}, \
	}

#define LIS2DH_DEFINE_SPI(inst)						\
	static struct lis2dh_data lis2dh_data_##inst =			\
		LIS2DH_DATA_SPI(inst);					\
	static const struct lis2dh_config lis2dh_config_##inst =	\
		LIS2DH_CONFIG_SPI(inst);				\
	LIS2DH_DEVICE_INIT(inst)

/*
 * Main instantiation macro. Use of COND_CODE_1() selects the right
 * bus-specific macro at preprocessor time.
 */

#define LIS2DH_DEFINE(inst) \
	COND_CODE_1(DT_INST_ON_BUS(inst, spi),				\
		    (LIS2DH_DEFINE_SPI(inst)),				\
		    (LIS2DH_DEFINE_I2C(inst)))

DT_INST_FOREACH_STATUS_OKAY(LIS2DH_DEFINE)