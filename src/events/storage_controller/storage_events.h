/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

#include "event_manager.h"

/** Used to tell storage controller which region to read/write to. */
typedef enum {
	STG_PARTITION_LOG = 0,
	STG_PARTITION_ANO = 1,
	STG_PARTITION_PASTURE = 2
} flash_partition_t;

/** 
 * @brief Writes a pointer to given data to a given region.
 * 
 * @param data pointer to where the data is found. Important that the
 *             module requesting a write keeps the data available
 *             until a write ack is received.
 * @param partition type of partition to write to.
 * @param len Length of the data written, this is just sizeof(struct), if we're
 *            bytecasting entire structs.
 * @param rotate_to_this bool, rotates the read pointer
 *               to this entry if true after its written.
 *               MUST BE SET to avoid garbage data. 
 *               If false, rotation to the newest 
 *               (this entry) will not happen. 
 *               
 */
struct stg_write_event {
	struct event_header header;

	/** Pointer to memory record, raw binary encoded protobuf message. 
         *  The messaging module keeps the data until it recieves 
         *  storage_data_ack_event sent from the storage controller.
         *  Which in turn means the messaging module can free up the data.
        */
	uint8_t *data;
	size_t len;

	flash_partition_t partition;

	bool rotate_to_this;
};

/** 
 * @brief Ack struct sent out by the storage controller once it has 
 *        consumed the new mem_rec data.
 * 
 * @param partition type of partition to write to.
 * 
 * @return rotated, bool whether the FCB was 
 *         full and we had to rotate the buffer.
 */
struct stg_ack_write_event {
	struct event_header header;
	flash_partition_t partition;

	bool rotated;
};

/** 
 * @brief Gives out a pointer to a memory record where the storage controller
 *        needs to memcpy its data from flash_read based on region requested.
 *        There's no need for a read_newest_entry flag, as if the write
 *        request has rotate_to_this=true will only have that entry
 *        available.
 * 
 * @param data pointer to where the data should be read and memcpy-ed to
 * @param rotate Whether to rotate the entries (delete) them once they're read,
 *               MUST BE SET to avoid garbage data.
 * @param len Length of the data written, this is just sizeof(struct), if we're
 *            bytecasting entire structs.
 * @param partition type of partition to write to.
 */
struct stg_read_event {
	struct event_header header;

	bool rotate;

	flash_partition_t partition;
};

/** 
 * @brief Ack for when the requested module has consumed the data, in which
 *        case we can update the pointer to data written.
 * @param partition type of partition to write to.
 */
struct stg_ack_read_event {
	struct event_header header;

	/** Pointer to where the storage controller copy its flash contents to.
	 *  The consumer/requester copy/consumes this data when it receives it,
	 *  which in turn publishes the consume_event, telling the
	 *  storage controller to de-allocate the cached contents.
        */
	uint8_t *data;
	size_t len;

	flash_partition_t partition;
};

/** 
 * @brief Notifies the storage controller that the data has been consumed.
 * 
 * @param do_not_rotate bool, false or not defined will tell storage controller 
 *               to rotate fcb after read so that the next read 
 *               will be the next entry. For fence/ano data this is not 
 *               the case in which case this needs to be true.
 */
struct stg_consumed_read_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(stg_write_event);
EVENT_TYPE_DECLARE(stg_read_event);

EVENT_TYPE_DECLARE(stg_ack_write_event);
EVENT_TYPE_DECLARE(stg_ack_read_event);

EVENT_TYPE_DECLARE(stg_consumed_read_event);
#endif /* _STORAGE_EVENT_H_ */
