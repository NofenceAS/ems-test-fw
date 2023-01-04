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
	PRODUCT_RECORD_REV = 0x03,
	BOM_MEC_REV = 0x04,
	BOM_PCB_REV = 0x05,
	HW_VERSION = 0x06,
	PRODUCT_TYPE = 0x07,
} settings_id_t;

typedef enum {
	PING = 0x55,
	REPORT = 0x5E,
	LOG = 0x70,
	TEST = 0x7E,
	SLEEP = 0xE0,
	WAKEUP = 0xE1,
	REBOOT = 0xEB,
	THREAD_CONTROL = 0x40
} system_cmd_t;

typedef enum {
	READ = 0x00,
	WRITE = 0x01,
	ERASE_ALL = 0xEA
} settings_cmd_t;

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

	SET_CHARGING_EN = 0xC0,

	BUZZER_WARN = 0xB0,
	ELECTRICAL_PULSE = 0xE0,
} simulator_cmd_t;

typedef enum {
	GET_CCID = 0x00,
	GET_VINT_STATUS = 0x01,
	TEST_MODEM_TX = 0x7E,
} modem_cmd_t;


int commander_send_resp(enum diagnostics_interface interface, commander_group_t group, uint8_t cmd,
			commander_resp_t resp, uint8_t *data, uint8_t data_size);

#endif /* _COMMANDER_DEF_H_ */