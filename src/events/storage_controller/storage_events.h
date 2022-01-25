/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

#include "event_manager.h"
#include "flash_memory.h"

/** 
 * @brief Writes a pointer to given memrec struct to a given region.
 */
struct stg_write_memrec_event {
	struct event_header header;

	/** Pointer to memory record, raw binary encoded protobuf message. 
         *  The messaging module keeps the data until it recieves 
         *  storage_data_ack_event sent from the storage controller.
         *  Which in turn means the messaging module can free up the data.
        */
	mem_rec *new_rec;
	flash_regions_t region;
};

/** 
 * @brief Ack struct sent out by the storage controller once it has 
 *        consumed the new mem_rec data.
 */
struct stg_ack_write_memrec_event {
	struct event_header header;
};

/** 
 * @brief Gives out a pointer to a memory record where the storage controller
 *        needs to memcpy its data from flash_read based on region requested.
 */
struct stg_read_memrec_event {
	struct event_header header;

	/** Pointer to memory record where the storage controller shall memcpy
         *  the mem_rec region contents. The module requesting the data
         *  must control locking of the memory itself, to ensure it's not 
         *  reading while the contents are being written.
        */
	mem_rec *new_rec;
	flash_regions_t region;
};

EVENT_TYPE_DECLARE(stg_write_memrec_event);
EVENT_TYPE_DECLARE(stg_read_memrec_event);

EVENT_TYPE_DECLARE(stg_ack_write_memrec_event);
#endif /* _STORAGE_EVENT_H_ */
