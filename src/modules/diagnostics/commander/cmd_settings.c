#include "cmd_settings.h"
#include "stg_config.h"
#include "nf_version.h"

#include <string.h>

/* Comment from Zephyr OS: 
 * newlib doesn't declare this function unless __POSIX_VISIBLE >= 200809.  No
 * idea how to make that happen, so lets put it right here.
 */
size_t strnlen(const char *s, size_t maxlen);

static int commander_settings_get_id(uint8_t *id, uint8_t *data, uint32_t size);
static int commander_settings_read(enum diagnostics_interface interface, settings_id_t id);
static int commander_settings_write(enum diagnostics_interface interface, settings_id_t id,
				    uint8_t *data, uint32_t size);

int commander_settings_handler(enum diagnostics_interface interface, uint8_t cmd, uint8_t *data,
			       uint32_t size)
{
	int err = 0;

	switch (cmd) {
	case READ: {
		uint8_t id = 0;
		if (commander_settings_get_id(&id, data, size) != 0) {
			commander_send_resp(interface, SETTINGS, cmd, NOT_ENOUGH, NULL, 0);
			break;
		}

		err = commander_settings_read(interface, id);

		break;
	}
	case WRITE: {
		uint8_t id = 0;
		if (commander_settings_get_id(&id, data, size) != 0) {
			commander_send_resp(interface, SETTINGS, cmd, NOT_ENOUGH, NULL, 0);
			break;
		}

		err = commander_settings_write(interface, id, &data[1], size - 1);

		break;
	}

	case ERASE_ALL:
	default: {
		commander_send_resp(interface, SETTINGS, cmd, UNKNOWN_CMD, NULL, 0);
		err = -EINVAL;
	}
	}

	return err;
}

static int commander_settings_get_id(uint8_t *id, uint8_t *data, uint32_t size)
{
	if (size < 1) {
		return -EINVAL;
	}

	*id = data[0];

	return 0;
}

static int commander_settings_read(enum diagnostics_interface interface, settings_id_t id)
{
	int err = 0;

	uint8_t buf[50];
	buf[0] = id;

	switch (id) {
	case SERIAL: {
		uint32_t serial = 0;
		err = stg_config_u32_read(STG_U32_UID, &serial);
		if (err == 0) {
			memcpy(&buf[1], &serial, sizeof(uint32_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint32_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case HOST_PORT: {
		//err = eep_read_host_port(&buf[1], sizeof(buf)-1);
		char port[STG_CONFIG_HOST_PORT_BUF_LEN];
		uint8_t port_length = 0;
		err = stg_config_str_read(STG_STR_HOST_PORT, port, &port_length);
		if (err == 0) {
			memcpy(&buf[1], port, port_length);
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + strnlen(&buf[1], sizeof(buf) - 1));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case EMS_PROVIDER: {
		uint8_t ems_provider = 0;
		err = stg_config_u8_read(STG_U8_EMS_PROVIDER, &ems_provider);
		if (err == 0) {
			buf[1] = ems_provider;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case PRODUCT_GENERATION: {
		uint8_t product_generation = 0;
		err = stg_config_u8_read(STG_U8_PRODUCT_GENERATION, &product_generation);

		if (err == 0) {
			buf[1] = product_generation;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case PRODUCT_MODEL: {
		uint8_t product_model = 0;
		err = stg_config_u8_read(STG_U8_PRODUCT_MODEL, &product_model);
		if (err == 0) {
			buf[1] = product_model;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case PRODUCT_REVISION: {
		uint8_t product_revision = 0;
		err = stg_config_u8_read(STG_U8_PRODUCT_REVISION, &product_revision);
		if (err == 0) {
			buf[1] = product_revision;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case HW_VERSION: {
		uint8_t hw_ver = 0;
		err = stg_config_u8_read(STG_U8_HW_VERSION, &hw_ver);
		if (err == 0) {
			buf[1] = hw_ver;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FW_VERSION: {
		uint16_t fwversion = NF_X25_VERSION_NUMBER;
		if (err == 0) {
			memcpy(&buf[1], &fwversion, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case PRODUCT_TYPE: {
		uint16_t prod_type = 0;
		err = stg_config_u16_read(STG_U16_PRODUCT_TYPE, &prod_type);
		if (err == 0) {
			memcpy(&buf[1], &prod_type, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case ACC_SIGMA_NOACT: {
		uint16_t accel_sigma_noact_limit = 0;
		err = stg_config_u16_read(STG_U16_ACC_SIGMA_NOACTIVITY_LIMIT,
					  &accel_sigma_noact_limit);
		if (err == 0) {
			memcpy(&buf[1], &accel_sigma_noact_limit, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case ACC_SIGMA_SLEEP: {
		uint16_t accel_sigma_sleep_limit = 0;
		err = stg_config_u16_read(STG_U16_ACC_SIGMA_SLEEP_LIMIT, &accel_sigma_sleep_limit);
		if (err == 0) {
			memcpy(&buf[1], &accel_sigma_sleep_limit, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case OFF_ANIMAL_TIME: {
		uint16_t off_animal_time_limit = 0;
		err = stg_config_u16_read(STG_U16_OFF_ANIMAL_TIME_LIMIT_SEC,
					  &off_animal_time_limit);
		if (err == 0) {
			memcpy(&buf[1], &off_animal_time_limit, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FLASH_TEACH_MODE_FINISHED: {
		uint8_t teachmode = 0;
		err = stg_config_u8_read(STG_U8_TEACH_MODE_FINISHED, &teachmode);
		if (err == 0) {
			buf[1] = teachmode;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FLASH_KEEP_MODE: {
		uint8_t keepmode = 0;
		err = stg_config_u8_read(STG_U8_KEEP_MODE, &keepmode);
		if (err == 0) {
			buf[1] = keepmode;
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint8_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FLASH_ZAP_CNT_TOT: {
		uint16_t zapcnttot = 0;
		err = stg_config_u16_read(STG_U16_ZAP_CNT_TOT, &zapcnttot);
		if (err == 0) {
			memcpy(&buf[1], &zapcnttot, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FLASH_ZAP_CNT_DAY: {
		uint16_t zapcntday = 0;
		err = stg_config_u16_read(STG_U16_ZAP_CNT_DAY, &zapcntday);
		if (err == 0) {
			memcpy(&buf[1], &zapcntday, sizeof(uint16_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint16_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}
	case FLASH_WARN_CNT_TOT: {
		uint32_t warncnttot = 0;
		err = stg_config_u32_read(STG_U32_WARN_CNT_TOT, &warncnttot);
		if (err == 0) {
			memcpy(&buf[1], &warncnttot, sizeof(uint32_t));
			commander_send_resp(interface, SETTINGS, READ, DATA, buf,
					    1 + sizeof(uint32_t));
		} else {
			commander_send_resp(interface, SETTINGS, READ, ERROR, NULL, 0);
		}
		break;
	}

	default: {
		commander_send_resp(interface, SETTINGS, READ, UNKNOWN, NULL, 0);

		err = -EINVAL;
		break;
	}
	}

	return err;
}

static int commander_settings_write(enum diagnostics_interface interface, settings_id_t id,
				    uint8_t *data, uint32_t size)
{
	int err = -EINVAL;

	switch (id) {
	case SERIAL: {
		/* Must be exactly 4 bytes for uint32_t */
		if (size == 4) {
			uint32_t new_serial =
				(data[0] << 0) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);

			uint32_t old_serial;
			err = stg_config_u32_read(STG_U32_UID, &old_serial);
			if ((new_serial != old_serial)) {
				err = stg_config_u32_write(STG_U32_UID, new_serial);
				/* reset business logic variables to 0 */
				err = stg_config_u8_write(STG_U8_TEACH_MODE_FINISHED, 0);
				err = stg_config_u8_write(STG_U8_KEEP_MODE, 0);
				err = stg_config_u16_write(STG_U16_ZAP_CNT_TOT, 0);
				err = stg_config_u16_write(STG_U16_ZAP_CNT_DAY, 0);
				err = stg_config_u32_write(STG_U32_WARN_CNT_TOT, 0);
			}
		}
		break;
	}
	case HOST_PORT: {
		if (size <= STG_CONFIG_HOST_PORT_BUF_LEN) {
			err = stg_config_str_write(STG_STR_HOST_PORT, data,
						   STG_CONFIG_HOST_PORT_BUF_LEN - 1);
		}
		break;
	}
	case EMS_PROVIDER: {
		/* Must be exactly 1 byte for uint8_t */
		if (size == 1) {
			uint8_t ems_provider = (data[0] << 0);
			err = stg_config_u8_write(STG_U8_EMS_PROVIDER, ems_provider);
		}
		break;
	}
	case PRODUCT_GENERATION: {
		/* Must be exactly 1 byte for uint8_t */
		if (size == 1) {
			uint8_t product_generation = (data[0] << 0);
			err = stg_config_u8_write(STG_U8_PRODUCT_GENERATION, product_generation);
		}
		break;
	}
	case PRODUCT_MODEL: {
		/* Must be exactly 1 byte for uint8_t */
		if (size == 1) {
			uint8_t product_model = (data[0] << 0);
			err = stg_config_u8_write(STG_U8_PRODUCT_MODEL, product_model);
		}
		break;
	}
	case PRODUCT_REVISION: {
		/* Must be exactly 1 byte for uint8_t */
		if (size == 1) {
			uint8_t product_revision = (data[0] << 0);
			err = stg_config_u8_write(STG_U8_PRODUCT_REVISION, product_revision);
		}
		break;
	}
	case HW_VERSION: {
		/* Must be exactly 1 byte for uint8_t */
		if (size == 1) {
			uint8_t hw_ver = (data[0] << 0);
			err = stg_config_u8_write(STG_U8_HW_VERSION, hw_ver);
		}
		break;
	}
	case PRODUCT_TYPE: {
		/* Must be exactly 2 bytes for uint16_t */
		if (size == 2) {
			uint32_t prod_type = (data[0] << 0) + (data[1] << 8);
			err = stg_config_u16_write(STG_U16_PRODUCT_TYPE, (uint16_t)prod_type);
		}
		break;
	}
	case ACC_SIGMA_NOACT: {
		/* Must be exactly 2 bytes for uint16_t */
		if (size == 2) {
			uint32_t accel_sigma_noact_limit = (data[0] << 0) + (data[1] << 8);
			err = stg_config_u16_write(STG_U16_ACC_SIGMA_NOACTIVITY_LIMIT,
						   (uint16_t)accel_sigma_noact_limit);
		}
		break;
	}
	case ACC_SIGMA_SLEEP: {
		/* Must be exactly 2 bytes for uint16_t */
		if (size == 2) {
			uint32_t accel_sigma_sleep_limit = (data[0] << 0) + (data[1] << 8);
			err = stg_config_u16_write(STG_U16_ACC_SIGMA_SLEEP_LIMIT,
						   (uint16_t)accel_sigma_sleep_limit);
		}
		break;
	}
	case OFF_ANIMAL_TIME: {
		/* Must be exactly 2 bytes for uint16_t */
		if (size == 2) {
			uint32_t off_animal_time_limit = (data[0] << 0) + (data[1] << 8);
			err = stg_config_u16_write(STG_U16_OFF_ANIMAL_TIME_LIMIT_SEC,
						   (uint16_t)off_animal_time_limit);
		}
		break;
	}
	default: {
		err = -EINVAL;
		break;
	}
	}

	if (err == 0) {
		commander_send_resp(interface, SETTINGS, WRITE, ACK, NULL, 0);
	} else if (err == -EINVAL) {
		commander_send_resp(interface, SETTINGS, WRITE, UNKNOWN, NULL, 0);
	} else {
		commander_send_resp(interface, SETTINGS, WRITE, ERROR, NULL, 0);
	}

	return err;
}