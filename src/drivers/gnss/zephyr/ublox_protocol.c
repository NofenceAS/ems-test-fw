#include "ublox_protocol.h"

#include "ublox_types.h"

#include <stdbool.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(UBLOX_PROTOCOL, CONFIG_GNSS_LOG_LEVEL);

/* UBX checksum is Fletcher's algorithm */
static uint16_t ublox_calculate_checksum(uint8_t* data, uint16_t length)
{
	/* Checksum is calculated from and including message class up to 
	 * and excluding CK_A */
	uint16_t calc_end = UBLOX_OFFS_CK_A(length); 

	uint8_t ck_a = 0;
	uint8_t ck_b = 0;

	for (uint32_t i = UBLOX_OFFS_CLASS; i < calc_end; i++) {
		ck_a = ck_a + data[i];
		ck_b = ck_b + ck_a;
	}

	return (ck_a + (ck_b<<8));
}

static int ublox_process_message(uint8_t class, uint8_t id, 
				 uint8_t* payload, uint16_t length)
{
	LOG_ERR("Process U-blox Message: %d, %d", class, id);

	return 0;
}

static uint16_t ublox_get_checksum(uint8_t* data, uint16_t length)
{
	return data[UBLOX_OFFS_CK_A(length)] +
	      (data[UBLOX_OFFS_CK_B(length)]<<8);
}

static bool ublox_is_checksum_correct(uint8_t* data, uint32_t length)
{
	uint16_t calc_checksum = ublox_calculate_checksum(data, length);
	uint16_t msg_checksum = ublox_get_checksum(data, length);

	return (calc_checksum == msg_checksum);
}

uint32_t ublox_parse(uint8_t* data, uint32_t size)
{
	if (size < 2) {
		/* Not enough data for sync header */
		return 0;
	}

	if (!((data[UBLOX_OFFS_SC_1] == UBLOX_SYNC_CHAR_1) && 
	      (data[UBLOX_OFFS_SC_2] == UBLOX_SYNC_CHAR_2))) {
		/* Sync char did not match, return and consume single byte */
		return 1;
	}

	if (size < UBLOX_MIN_PACKET_SIZE) {
		/* Not enough data for full packet yet */
		return 0;
	}

	uint16_t payload_length = data[UBLOX_OFFS_LENGTH] + 
			 	 (data[UBLOX_OFFS_LENGTH+1]<<8);
	uint16_t packet_length = (UBLOX_MIN_PACKET_SIZE + payload_length);
	
	if (size < packet_length) {
		/* Not enough data for packet with payload yet */
		return 0;
	}

	if (!ublox_is_checksum_correct(data, payload_length)) {
		/* Wrong checksum, ignore data */
		LOG_ERR("Checksum failed");
		return packet_length;
	}

	/* Packet received and verified, process contents */

	uint8_t msg_class = data[UBLOX_OFFS_CLASS];
	uint8_t msg_id = data[UBLOX_OFFS_ID];

	if (ublox_process_message(msg_class, 
				      msg_id, 
				      &data[UBLOX_OFFS_PAYLOAD],
				      payload_length) != 0) {
		LOG_ERR("Failed processing message");
	}

	return packet_length;
}

int ublox_build_cfg_valget(uint8_t* buffer, uint32_t* size, uint32_t max_size, 
			   enum ublox_cfg_layer layer, 
			   uint16_t position, 
			   uint32_t* keys, 
			   uint8_t key_cnt)
{
	/* Buffer must be 32bit aligned */
	if (((uint32_t)buffer%4) != 0) {
		return -EFAULT;
	}

	/* Calculate and validate packet length */
	uint32_t packet_length = (UBLOX_MIN_PACKET_SIZE + 4 + key_cnt*4);
	if (packet_length > max_size) {
		return -ENOBUFS;
	}

	return 0;
}