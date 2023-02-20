/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _COMMANDER_DEF_H_
#define _COMMANDER_DEF_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
	/* Overall group context of the command */
	uint8_t group;
	/* Command to execute, specific for each group context */
	uint8_t command;
	/* CRC16 CCITT */
	uint16_t checksum;
} commander_cmd_header_t;

typedef struct __attribute__((packed)) {
	/* Overall group context of the command */
	uint8_t group;
	/* Command to execute, specific for each group context */
	uint8_t command;
	/* Response type */
	uint8_t response;
	/* CRC16 CCITT */
	uint16_t checksum;
} commander_resp_header_t;

typedef enum {
	ACK = 0x00,
	DATA = 0x01,

	CHK_FAILED = 0xC0,
	NOT_ENOUGH = 0xD0,
	NOT_IMPLEMENTED = 0xD1,
	ERROR = 0xE0,
	UNKNOWN_CMD = 0xFC,
	UNKNOWN_GRP = 0xFE,
	UNKNOWN = 0xFF
} commander_resp_t;

typedef enum {
	SYSTEM = 0x00,
	SETTINGS = 0x01,
	STIMULATOR = 0x02,
	STORAGE = 0x03,
	MODEM = 0x04,
} commander_group_t;

typedef enum {
	SERIAL = 0x00,
	HOST_PORT = 0x01,
	EMS_PROVIDER = 0x02,
	PRODUCT_GENERATION = 0x03,
	PRODUCT_MODEL = 0x04,
	PRODUCT_REVISION = 0x05,
	HW_VERSION = 0x06,
	PRODUCT_TYPE = 0x07,
	ACC_SIGMA_NOACT = 0x08,
	ACC_SIGMA_SLEEP = 0x09,
	OFF_ANIMAL_TIME = 0x0A,
	FW_VERSION = 0x0B,
	FLASH_TEACH_MODE_FINISHED = 0x0C,
	FLASH_KEEP_MODE = 0x0D,
	FLASH_ZAP_CNT_TOT = 0x0E,
	FLASH_ZAP_CNT_DAY = 0x0F,
	FLASH_WARN_CNT_TOT = 0x10,
} settings_id_t;

typedef enum {
	PING = 0x55,
	REPORT = 0x5E,
	LOG = 0x70,
	TEST = 0x7E,
	SLEEP = 0xE0,
	WAKEUP = 0xE1,
	REBOOT = 0xEB,
	THREAD_CONTROL = 0x40,
	READ_THREAD_CONTROL = 0x41,

	FORCE_POLL_REQ = 0x42,
	CLEAR_PASTURE = 0xC0,
	ERASE_FLASH = 0xEF,

	GET_DIAG_FLAGS = 0x80,
	SET_DIAG_FLAGS = 0x82,
	CLR_DIAG_FLAGS = 0x84,

	SET_LOCK_BIT = 0xDE,
} system_cmd_t;

typedef enum { READ = 0x00, WRITE = 0x01, ERASE_ALL = 0xEA } settings_cmd_t;

typedef enum {
	GNSS_HUB = 0x10,
	GNSS_SEND = 0x11,
	GNSS_RECEIVE = 0x12,

	MODEM_HUB = 0x20,
	MODEM_SEND = 0x21,
	MODEM_RECEIVE = 0x22,

	GET_ONBOARD_DATA = 0xA0,
	GET_OB_DATA = 0xA2,
	GET_GNSS_DATA = 0xA4,
	GET_GSM_DATA = 0xA6,
	GET_OB_DEVICE_VERSION_DATA = 0xA7,

	SET_CHARGING_EN = 0xC0,
	TURN_ONOFF_CHARGING = 0xE1,

	BUZZER_WARN = 0xB0,
	BUZZER_TEST = 0xB1,
	BUZZER_STOP = 0xBF,
	ELECTRICAL_PULSE = 0xE0,
	ELECTRICAL_PULSE_INFINITE = 0xE3,
} simulator_cmd_t;

typedef enum {
	GET_CCID = 0x00,
	GET_VINT_STATUS = 0x01,
	GET_IP = 0x02,
	TEST_MODEM_TX = 0x7E,
} modem_cmd_t;

int commander_send_resp(enum diagnostics_interface interface, commander_group_t group, uint8_t cmd,
			commander_resp_t resp, uint8_t *data, uint8_t data_size);

#endif /* _COMMANDER_DEF_H_ */