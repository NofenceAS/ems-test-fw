#include "ublox_protocol.h"

#include "ubx_ids.h"

#include <sys/mutex.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(UBLOX_PROTOCOL, CONFIG_GNSS_LOG_LEVEL);

/* Constants and macros used for parsing U-blox messages */
#define UBLOX_OFFS_SC_1		0
#define UBLOX_OFFS_SC_2		1
#define UBLOX_OFFS_CLASS	2
#define UBLOX_OFFS_ID		3
#define UBLOX_OFFS_LENGTH	4
#define UBLOX_OFFS_PAYLOAD	6
#define UBLOX_OFFS_CK_A(x)	(6+x)
#define UBLOX_OFFS_CK_B(x)	(7+x)

#define UBLOX_CHK_SIZE		2

#define UBLOX_OVERHEAD_SIZE		(UBLOX_OFFS_PAYLOAD+UBLOX_CHK_SIZE)

#define UBLOX_MSG_IDENTIFIER(c,i)	((c<<8)|(i))
#define UBLOX_MSG_IDENTIFIER_EMPTY	0

/* This can be adjusted, but should never be larger than receive buffer 
 * Any payloads greater than this will lead to the message being discarded. */
#define UBLOX_MAX_PAYLOAD_SIZE	512

/* Registered handlers for periodic messages. 
 * Links the message class/ids to callbacks.
 */ 
#define UBLOX_MAX_HANDLERS 20
struct ublox_handler {
	uint8_t msg_class;
	uint8_t msg_id;
	int (*handle)(void*,void*,uint32_t);
	void* context;
};
static struct ublox_handler handlers[UBLOX_MAX_HANDLERS];
static uint32_t callback_count = 0;

/* Sending commands will often result in an acknowledgement and data response.
 * These variables stores information of which command is awaiting response,
 * and where to relay the ack/data. 
 */
static struct k_mutex cmd_mutex;
static uint16_t cmd_identifier = UBLOX_MSG_IDENTIFIER_EMPTY;
static void* cmd_context = NULL;
static int (*cmd_poll_handle)(void*,uint8_t,uint8_t,void*,uint32_t) = NULL;
static int (*cmd_ack_handle)(void*,uint8_t,uint8_t,bool) = NULL;

void ublox_protocol_init(void)
{
	callback_count = 0;
	memset(handlers, 0, sizeof(handlers));
	k_mutex_init(&cmd_mutex);
}

/**
 * @brief Function to find the correct handler for a specified class/id
 *
 * @param[in] msg_class Class of message according to U-blox protocol.
 * @param[in] msg_id ID of message according to U-blox protocol. 
 * @param[out] handler Resolved object registered with matching class and ID.
 *
 * @return 0 if handler was resolved, error code otherwise. 
 */
static int ublox_resolve_handler(uint8_t msg_class, uint8_t msg_id,
				 struct ublox_handler** handler)
{
	*handler = NULL;
	for (uint32_t i = 0; i < UBLOX_MAX_HANDLERS; i++)
	{
		if ((handlers[i].msg_class == msg_class) && 
			(handlers[i].msg_id == msg_id))
		{
			*handler = &handlers[i];
			break;
		}
	}

	if (*handler == NULL)
	{
		return -ENOMSG;
	}

	return 0;
}

int ublox_register_handler(uint8_t msg_class, uint8_t msg_id, 
			   int (*handle)(void*,void*,uint32_t), void* context)
{
	struct ublox_handler* handler = NULL;
	int ret = ublox_resolve_handler(msg_class, msg_id, &handler);
	if (ret == 0)
	{
		/* Handler already registered */
		if (handler == NULL)
		{
			/* Clear entry */
			memset(handler, 0, 
			       sizeof(*handler));
		} else 
		{
			/* Update it */
			handler->handle = handle;
			handler->context = context;
		}

		return 0;
	}

	/* Iterate over all available slots, find empty */
	for (uint32_t i = 0; i < UBLOX_MAX_HANDLERS; i++)
	{
		if ((handlers[i].msg_class == 0) && 
		    (handlers[i].msg_id == 0))
		{
			handler = &handlers[i];
			break;
		}
	}

	if (handler == NULL)
	{
		return -ENOBUFS;
	}

	/* Register entry */
	handler->msg_class = msg_class;
	handler->msg_id = msg_id;
	handler->handle = handle;
	handler->context = context;

	return 0;
}

/**
 * @brief Function to calculate checksum of U-blox protocol message. 
 *        This is Fletcher's algorithm
 *
 * @param[in] data Data to calculate checksum from. 
 * @param[in] length Length of data.
 *
 * @return Calculated checksum. 
 */
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

/**
 * @brief Function to process an identified U-blox message. 
 *        This function will first check if this is an ack for a command.
 *        Secondly it will check if any command is awaiting data. 
 *        And finally it will check if the class/id matches any registered
 *        handlers. 
 *
 * @param[in] msg_class Class of message. 
 * @param[in] msg_id ID of message.
 * @param[in] payload Payload for message. 
 * @param[in] length Length of payload. 
 *
 * @return 0 if message was processed, error code otherwise. 
 */
static int ublox_process_message(uint8_t msg_class, uint8_t msg_id, 
				 uint8_t* payload, uint16_t length)
{
	uint16_t msg_identifier = UBLOX_MSG_IDENTIFIER(msg_class, msg_id);
	
	if (k_mutex_lock(&cmd_mutex, K_MSEC(10)) == 0) {
		/* Handle acknowledgements */
		if (msg_class == UBX_ACK)
		{
			bool ack = msg_id == UBX_ACK_ACK;

			struct ublox_ack_ack* msg_ack = (void*)payload;

			msg_identifier = UBLOX_MSG_IDENTIFIER(msg_ack->clsID, 
							      msg_ack->msgID);

			if (msg_identifier == cmd_identifier)
			{
				if (cmd_ack_handle != NULL)
				{
					cmd_ack_handle(cmd_context, 
						       msg_class, msg_id, 
						       ack);

					/* Make sure to disable any further 
					 * calls to this, as the ack is a 
					 * one-off */
					cmd_ack_handle = NULL;

					k_mutex_unlock(&cmd_mutex);
					return 0;
				}
			}
		}

		/* Is this a message awaiting poll response? */
		if (msg_identifier == cmd_identifier)
		{
			if (cmd_poll_handle != NULL)
			{
				cmd_poll_handle(cmd_context, 
						msg_class, msg_id, 
						(void*)payload, length);

				/* Make sure to disable any further calls to 
				 * this,as the poll is a one-off */
				cmd_poll_handle = NULL;

				k_mutex_unlock(&cmd_mutex);
				return 0;
			}
		}
		
		k_mutex_unlock(&cmd_mutex);
	}

	/* Check registered handlers for match */
	struct ublox_handler* handler = NULL;
	int ret = ublox_resolve_handler(msg_class, msg_id, &handler);
	if (ret == 0)
	{
		ret = handler->handle(handler->context, payload, length);
	}

	return ret;
}

/**
 * @brief Function that gets the checksum field of a U-blox message. 
 *
 * @param[in] data Data containing message. 
 * @param[in] length Length of data. 
 *
 * @return Checksum of message.
 */
static uint16_t ublox_get_checksum(uint8_t* data, uint16_t length)
{
	return data[UBLOX_OFFS_CK_A(length)] +
	      (data[UBLOX_OFFS_CK_B(length)]<<8);
}

/**
 * @brief Checks if the checksum of a U-blox message matches the 
 *        content of the message. 
 *
 * @param[in] data Data containing message. 
 * @param[in] length Length of data. 
 *
 * @return Checksum of message.
 */
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

	if (size < UBLOX_OVERHEAD_SIZE) {
		/* Not enough data for full packet yet */
		return 0;
	}

	uint16_t payload_length = data[UBLOX_OFFS_LENGTH] + 
			 	 (data[UBLOX_OFFS_LENGTH+1]<<8);
	uint16_t packet_length = (UBLOX_OVERHEAD_SIZE + payload_length);
	
	if (payload_length > UBLOX_MAX_PAYLOAD_SIZE) {
		/* Payload is unreasonably large, 
		 * return and consume single byte to resync */
		return 1;
	}

	if (size < packet_length) {
		/* Not enough data for packet with payload yet */
		return 0;
	}

	if (!ublox_is_checksum_correct(data, payload_length)) {
		/* Wrong checksum, ignore data */
		return packet_length;
	}

	/* Packet received and verified, process contents */

	uint8_t msg_class = data[UBLOX_OFFS_CLASS];
	uint8_t msg_id = data[UBLOX_OFFS_ID];

	ublox_process_message(msg_class, 
			      msg_id, 
			      &data[UBLOX_OFFS_PAYLOAD],
			      payload_length);

	return packet_length;
}

int ublox_reset_response_handlers(void)
{
	if (k_mutex_lock(&cmd_mutex, K_MSEC(10)) == 0) {
		/* Remove class/id and function pointers for 
		 * awaiting poll and ack */
		cmd_identifier = UBLOX_MSG_IDENTIFIER_EMPTY;
		cmd_poll_handle = NULL;
		cmd_ack_handle = NULL;
		cmd_context = NULL;

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return 0;
}

int ublox_set_response_handlers(
			uint8_t* buffer,
			int (*poll_cb)(void*,uint8_t,uint8_t,void*,uint32_t),
			int (*ack_cb)(void*,uint8_t,uint8_t,bool),
			void* context)
{
	struct ublox_header* header = (void*)buffer;

	if (k_mutex_lock(&cmd_mutex, K_MSEC(10)) == 0) {
		cmd_context = context;
		cmd_poll_handle = poll_cb;
		cmd_ack_handle = ack_cb;
		cmd_identifier = UBLOX_MSG_IDENTIFIER(header->msg_class, 
						      header->msg_id);

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return 0;
}

/**
 * @brief Calculates the data size from a configuration key.
 *
 * @param[in] key Key of configuration data.
 *
 * @return Size of data in bytes. 
 */
static uint8_t ublox_get_cfg_size(uint32_t key)
{
	uint8_t decoded_size = 0;
	uint8_t size_field = ((key>>28)&7);
	switch (size_field) {
		case 1:
		case 2:
			decoded_size = 1;
			break;
		case 3:
			decoded_size = 2;
			break;
		case 4:
			decoded_size = 4;
			break;
		case 5:
			decoded_size = 8;
			break;
		default:
			break;
	}
	return decoded_size;
}

int ublox_get_cfg_val(uint8_t* payload, uint32_t size, 
		      uint8_t val_size, uint64_t* val)
{
	struct ublox_cfg_val* msg = (void*)payload;
	if (size < offsetof(struct ublox_cfg_val, keys)) {
		return -EINVAL;
	}
	size -= offsetof(struct ublox_cfg_val, keys);
	uint8_t* cfgval = (uint8_t*)&msg->keys;

	if (size < 5) {
		return -EINVAL;
	}

	uint32_t key = GET_LE32(cfgval);
	uint8_t* key_val = &cfgval[4];

	uint8_t decoded_size = ublox_get_cfg_size(key);
	if (decoded_size != val_size) {
		return -ECOMM;
	}
	if (size < (4+val_size)) {
		return -ENODATA;
	}

	if (val_size == 1) {
		*val = GET_LE8(key_val);
	} else if (val_size == 2) {
		*val = GET_LE16(key_val);
	} else if (val_size == 4) {
		*val = GET_LE32(key_val);
	} else if (val_size == 8) {
		*val = GET_LE64(key_val);
	} 

	return 0;
}

int ublox_build_cfg_valget(uint8_t* buffer, uint32_t* size, uint32_t max_size,
			   enum ublox_cfg_val_layer layer, 
			   uint16_t position, 
			   uint32_t key)
{
	/* Calculate packet length */
	uint32_t payload_length = 4 + 4;
	uint32_t packet_length = UBLOX_OVERHEAD_SIZE + payload_length;

	if (max_size < packet_length) {
		return -ENOBUFS;
	}

	uint8_t msg_class = UBX_CFG;
	uint8_t msg_id = UBX_CFG_VALGET;

	/* Build data structure pointers */
	struct ublox_header* header = (void*)buffer;
	struct ublox_cfg_val* cfg_valget = 
				(void*)&buffer[sizeof(struct ublox_header)];
	union ublox_checksum* checksum = 
				(void*)&buffer[sizeof(struct ublox_header) + \
					       payload_length];

	/* Fill header */
	header->sync1 = UBLOX_SYNC_CHAR_1;
	header->sync2 = UBLOX_SYNC_CHAR_2;
	header->msg_class = msg_class;
	header->msg_id = msg_id;
	header->length = payload_length;

	/* Null payload first to account for reserved fields */
	memset(cfg_valget, 0, payload_length);
	/* Fill payload */
	cfg_valget->version = 0x00;
	cfg_valget->layer = (uint8_t) layer;
	cfg_valget->position = position;
	cfg_valget->keys = key;

	/* Calculate checksum */
	checksum->ck = ublox_calculate_checksum(buffer, header->length);

	*size = packet_length;

	return 0;
}

int ublox_build_cfg_valset(uint8_t* buffer, uint32_t* size, uint32_t max_size,
			   enum ublox_cfg_val_layer layer, 
			   uint32_t key,
			   uint64_t value)
{
	uint8_t decoded_size = ublox_get_cfg_size(key);

	/* Calculate packet length */
	uint32_t payload_length = 4 + 4 + decoded_size;
	uint32_t packet_length = UBLOX_OVERHEAD_SIZE + payload_length;

	if (max_size < packet_length) {
		return -ENOBUFS;
	}

	uint8_t msg_class = UBX_CFG;
	uint8_t msg_id = UBX_CFG_VALSET;

	/* Build data structure pointers */
	struct ublox_header* header = (void*)buffer;
	struct ublox_cfg_val* cfg_valset = 
				(void*)&buffer[sizeof(struct ublox_header)];
	union ublox_checksum* checksum = 
				(void*)&buffer[sizeof(struct ublox_header) + \
					       payload_length];

	/* Fill header */
	header->sync1 = UBLOX_SYNC_CHAR_1;
	header->sync2 = UBLOX_SYNC_CHAR_2;
	header->msg_class = msg_class;
	header->msg_id = msg_id;
	header->length = payload_length;

	/* Null payload first to account for reserved fields */
	memset(cfg_valset, 0, payload_length);
	/* Fill payload */
	cfg_valset->version = 0x00;
	cfg_valset->layer = (uint8_t) layer;
	cfg_valset->keys = key;
	uint8_t* key_val = &((uint8_t*)&cfg_valset->keys)[sizeof(key)];
	for (uint8_t i = 0; i < decoded_size; i++) {
		key_val[i] = (value>>(8*i))&0xFF;
	}

	/* Calculate checksum */
	checksum->ck = ublox_calculate_checksum(buffer, header->length);

	*size = packet_length;

	return 0;
}

int ublox_build_cfg_rst(uint8_t* buffer, uint32_t* size, uint32_t max_size,
			uint16_t mask,
			uint8_t mode)
{
	/* Calculate packet length */
	uint32_t payload_length = 4;
	uint32_t packet_length = UBLOX_OVERHEAD_SIZE + payload_length;

	if (max_size < packet_length) {
		return -ENOBUFS;
	}

	uint8_t msg_class = UBX_CFG;
	uint8_t msg_id = UBX_CFG_RST;

	/* Build data structure pointers */
	struct ublox_header* header = (void*)buffer;
	struct ublox_cfg_rst* cfg_rst = 
				(void*)&buffer[sizeof(struct ublox_header)];
	union ublox_checksum* checksum = 
				(void*)&buffer[sizeof(struct ublox_header) + \
					       payload_length];

	/* Fill header */
	header->sync1 = UBLOX_SYNC_CHAR_1;
	header->sync2 = UBLOX_SYNC_CHAR_2;
	header->msg_class = msg_class;
	header->msg_id = msg_id;
	header->length = payload_length;

	/* Null payload first to account for reserved fields */
	memset(cfg_rst, 0, payload_length);
	/* Fill payload */
	cfg_rst->navBbrMask = mask;
	cfg_rst->resetMode = mode;

	/* Calculate checksum */
	checksum->ck = ublox_calculate_checksum(buffer, header->length);

	*size = packet_length;

	return 0;
}

int ublox_build_mga_ano(uint8_t* buffer, uint32_t* size, uint32_t max_size,
			uint8_t* data,
			uint32_t data_size)
{
	/* Verify data size from interface description document */
	if (data_size != 76) {
		return -EINVAL;
	}

	/* Calculate packet length */
	uint32_t payload_length = data_size;
	uint32_t packet_length = UBLOX_OVERHEAD_SIZE + payload_length;

	if (max_size < packet_length) {
		return -ENOBUFS;
	}

	uint8_t msg_class = UBX_MGA;
	uint8_t msg_id = UBX_MGA_ANO;

	/* Build data structure pointers */
	struct ublox_header* header = (void*)buffer;
	uint8_t* payload = (void*)&buffer[sizeof(struct ublox_header)];
	union ublox_checksum* checksum = 
				(void*)&buffer[sizeof(struct ublox_header) + \
					       payload_length];

	/* Fill header */
	header->sync1 = UBLOX_SYNC_CHAR_1;
	header->sync2 = UBLOX_SYNC_CHAR_2;
	header->msg_class = msg_class;
	header->msg_id = msg_id;
	header->length = payload_length;

	/* Fill payload */
	memcpy(payload, data, payload_length);

	/* Calculate checksum */
	checksum->ck = ublox_calculate_checksum(buffer, header->length);

	*size = packet_length;

	return 0;
}

int ublox_build_rxm_pmreq(uint8_t* buffer, uint32_t* size, uint32_t max_size)
{

    /* Calculate packet length */
    uint32_t payload_length = sizeof(struct ublox_rxm_pmreq);
    uint32_t packet_length = UBLOX_OVERHEAD_SIZE + payload_length;

    if (max_size < packet_length) {
        return -ENOBUFS;
    }

    uint8_t msg_class = UBX_RXM;
    uint8_t msg_id = UBX_RXM_PMREQ;

    /* Build data structure pointers */
    struct ublox_header* header = (void*)buffer;
    struct ublox_rxm_pmreq * payload = (void*)&buffer[sizeof(struct ublox_header)];
    union ublox_checksum* checksum =
            (void*)&buffer[sizeof(struct ublox_header) + \
					       payload_length];

    /* Fill header */
    header->sync1 = UBLOX_SYNC_CHAR_1;
    header->sync2 = UBLOX_SYNC_CHAR_2;
    header->msg_class = msg_class;
    header->msg_id = msg_id;
    header->length = payload_length;

    /* Fill payload */
    memset(payload,0,payload_length);
    // TODO, use FORCE in PMREQ ??
    payload->duration = 0;
    payload->flags |= (UBX_RXM_PMREQ_FLAGS_BACKUP_BITFIELD);
    payload->wakeupSources |= (UBX_RXM_PMREQ_WAKEUPSOURCES_EXTINT0_BITFIELD);

    /* Calculate checksum */
    checksum->ck = ublox_calculate_checksum(buffer, header->length);

    *size = packet_length;

    return 0;
}
