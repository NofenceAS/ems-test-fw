/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

#include "event_manager.h"

/** Used to tell storage controller which region to read/write to. */
typedef enum { STG_PARTITION_LOG = 0, STG_PARTITION_ANO = 1 } flash_partition_t;

/** 
 * @brief Writes a pointer to given data to a given region.
 * 
 * @param data pointer to where the data is found. Important that the
 *             module requesting a write keeps the data available
 *             until a write ack is received.
 * @param partition type of partition to write to.
 * @param rotate bool, if false or undefined, rotation to the newest 
 *               (this entry) will not happen. Rotates the read pointer
 *               to this entry if true after its written.
 *               MUST BE SET to avoid garbage data.
 *               
 */
struct stg_write_event {
	struct event_header header;

	/** Pointer to memory record, raw binary encoded protobuf message. 
         *  The messaging module keeps the data until it recieves 
         *  storage_data_ack_event sent from the storage controller.
         *  Which in turn means the messaging module can free up the data.
	 * 
	 * @note We do not need to give it a size, since when we know
	 *       which partition we write to, we also know which struct it is.
        */
	uint8_t *data;
	flash_partition_t partition;

	bool rotate;
};

/** 
 * @brief Ack struct sent out by the storage controller once it has 
 *        consumed the new mem_rec data.
 * @param partition type of partition to write to.
 */
struct stg_ack_write_event {
	struct event_header header;
	flash_partition_t partition;
};

/** 
 * @brief Gives out a pointer to a memory record where the storage controller
 *        needs to memcpy its data from flash_read based on region requested.
 * 
 * @param data pointer to where the data should be read and memcpy-ed to
 * @param newest_entry_only bool, only reads the newest entry if true,
 *                          MUST BE SET to avoid garbage data.
 * @param rotate Whether to rotate the entries (delete) them once they're read,
 *               MUST BE SET to avoid garbage data.
 * @param partition type of partition to write to.
 */
struct stg_read_event {
	struct event_header header;

	/** Pointer to memory record where the storage controller shall memcpy
         *  the mem_rec region contents. The module requesting the data
         *  must control locking of the memory itself, to ensure it's not 
         *  reading while the contents are being written.
	 * 
	 * @note We do not need to give it a size, if we know
	 *       which partition we read from, we also know which struct it is.
        */
	uint8_t *data;
	bool newest_entry_only;
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
	flash_partition_t partition;
};

/** 
 * @brief Notifies the storage controller that the data has been consumed.
 * 
 * @param do_not_rotate bool, false or not defined will tell storage controller 
 *               to rotate fcb after read so that the next read 
 *               will be the next entry. For fence/ano data this is not 
 *               the case in which case this needs to be true.
 * @param partition type of partition to write to.
 */
struct stg_consumed_read_event {
	struct event_header header;
	flash_partition_t partition;
	//bool do_not_rotate;
};

EVENT_TYPE_DECLARE(stg_write_event);
EVENT_TYPE_DECLARE(stg_read_event);

EVENT_TYPE_DECLARE(stg_ack_write_event);
EVENT_TYPE_DECLARE(stg_ack_read_event);

EVENT_TYPE_DECLARE(stg_consumed_read_event);
#endif /* _STORAGE_EVENT_H_ */
