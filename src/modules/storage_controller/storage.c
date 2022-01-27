/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <device.h>
#include <devicetree.h>
#include <stdio.h>
#include <string.h>
#include "flash_memory.h"
#include "storage_events.h"
#include <fs/fcb.h>

/* Get sizes and offset definitions from pm_static.yml. */
#include <pm_config.h>

#include <logging/log.h>
#define MODULE storage_controller
LOG_MODULE_REGISTER(MODULE, CONFIG_STORAGE_CONTROLLER_LOG_LEVEL);

#define FLASH_LOG_NUM_SECTORS PM_LOG_PARTITION_SIZE / CONFIG_STORAGE_SECTOR_SIZE
#define FLASH_ANO_NUM_SECTORS PM_ANO_PARTITION_SIZE / CONFIG_STORAGE_SECTOR_SIZE

const struct flash_area *log_area;
struct fcb log_fcb;
struct flash_sector log_sectors[FLASH_LOG_NUM_SECTORS];

const struct flash_area *ano_area;
struct fcb ano_fcb;
struct flash_sector ano_sectors[FLASH_ANO_NUM_SECTORS];

/* Semaphore used by storage controller to continue the walk process only
 * when this semaphore was given in the consumed event sent out by the
 * consumer/requester.
 */
K_SEM_DEFINE(data_consumed_sem, 0, 1);

#define NUM_STG_MSGQ_EVENTS 2
/* 4 means 4-byte alignment. */
K_MSGQ_DEFINE(read_event_msgq, sizeof(struct stg_read_memrec_event),
	      CONFIG_MSGQ_READ_EVENT_SIZE, 4);
K_MSGQ_DEFINE(write_event_msgq, sizeof(struct stg_write_memrec_event),
	      CONFIG_MSGQ_WRITE_EVENT_SIZE, 4);

struct k_poll_event stg_msgq_events[NUM_STG_MSGQ_EVENTS] = {
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&read_event_msgq, 0),
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&write_event_msgq, 0),
};

static inline int init_fcb_on_partition(flash_partition_t partition)
{
	int err;
	const struct device *dev;
	struct fcb *fcb;
	uint32_t sector_cnt;
	int area_id;
	const struct flash_area *area;
	struct flash_sector *sector_ptr;

	/* Based on parameter, setup variables required by FCB. */
	if (partition == STG_PARTITION_LOG) {
		fcb = &log_fcb;
		sector_cnt = FLASH_LOG_NUM_SECTORS;
		area_id = FLASH_AREA_ID(log_partition);
		area = log_area;
		sector_ptr = log_sectors;
	} else if (partition == STG_PARTITION_ANO) {
		fcb = &ano_fcb;
		sector_cnt = FLASH_ANO_NUM_SECTORS;
		area_id = FLASH_AREA_ID(ano_partition);
		area = ano_area;
		sector_ptr = ano_sectors;
	} else {
		LOG_ERR("Invalid partition given. %d", -EINVAL);
		return -EINVAL;
	}

	err = flash_area_open(area_id, &area);
	if (err) {
		LOG_ERR("Error opening flash area for partition %d, err %d",
			partition, err);
		return err;
	}

	/* Check if area has a flash device available. */
	dev = device_get_binding(area->fa_dev_name);
	flash_area_close(area);

	if (dev == NULL) {
		LOG_ERR("Could not get device for partition %d.", partition);
		return -ENODEV;
	}

	/* Used to get flash parameters and driver specific attributes. */
	const struct flash_parameters *fp;
	fp = flash_get_parameters(dev);

	/* Setup a fresh flash circular buffer, 
	 * this does not EMPTY existing contents on flash, but is
	 * required for fcb_init to work properly. 
	 */
	(void)memset(fcb, 0, sizeof(*fcb));

	fcb->f_magic = 0U;
	fcb->f_erase_value = fp->erase_value;

	/* Setup the sectors. We input sector_cnt to be MAX 255
	 * since this is the limitation of FCB. Sector count is
	 * IN,OUT parameter, in which case it retrieves the MAX
	 * number of sectors, and outputs how many is available
	 * based on partition size.
	 */
	err = flash_area_get_sectors(area_id, &sector_cnt, sector_ptr);
	if (err) {
		LOG_ERR("Unable to setup sectors for partition %d, err %d.",
			partition, err);
		return err;
	}

	fcb->f_sector_cnt = (uint8_t)sector_cnt;
	fcb->f_sectors = sector_ptr;

	err = fcb_init(area_id, fcb);
	if (err) {
		LOG_ERR("Unable to initialize fcb for partition %d, err %d.",
			partition, err);
		return err;
	}

	LOG_INF("Setup FCB for partition %d: %d sectors with sizes %db.",
		partition, fcb->f_sector_cnt, fcb->f_sectors[0].fs_size);
	return err;
}

int init_storage_controller(void)
{
	int err;

	/* Initialize FCB on LOG and ANO partitions
	 * based on pm_static.yml/.dts flash setup.
	 */
	err = init_fcb_on_partition(STG_PARTITION_LOG);
	if (err) {
		return err;
	}

	err = init_fcb_on_partition(STG_PARTITION_ANO);
	if (err) {
		return err;
	}
	return 0;
}

void stg_fcb_write_entry()
{
	struct stg_write_memrec_event ev;
	int err = k_msgq_get(&write_event_msgq, &ev, K_NO_WAIT);
	if (err) {
		LOG_ERR("Error getting write_event from msgq: %d", err);
		return;
	}

	struct fcb *fcb;
	struct fcb_entry loc;

	uint8_t *data = (uint8_t *)ev.new_rec;
	size_t len = sizeof(mem_rec);

	if (ev.partition == STG_PARTITION_LOG) {
		fcb = &log_fcb;
	} else if (ev.partition == STG_PARTITION_ANO) {
		fcb = &ano_fcb;
	} else {
		return;
	}

	/* Appending a new entry, rotate(replaces) oldests if no space. */
	err = fcb_append(fcb, len, &loc);
	if (err == -ENOSPC) {
		err = fcb_rotate(fcb);
		if (err) {
			LOG_ERR("Unable to rotate fcb from -ENOSPC, err %d",
				err);
			return;
		}
	} else if (err) {
		LOG_ERR("Error appending new fcb entry, err %d", err);
		return;
	}

	/* If ppsize-512 is not defined in 
	 * .dts for flash device, 256 bytes is used for max page size. 
	 */
	int max_write_size = FLASH_PAGE_MAX_CNT;
	int num_write_cycles =
		(len / max_write_size) + (len % max_write_size != 0);
	int last_write_size = len - ((num_write_cycles - 1) * max_write_size);

	/* Writing data fragments. */
	for (int i = 0; i < num_write_cycles; i++) {
		off_t offset = i * max_write_size;
		int write_size = max_write_size;

		/* If we're on the last fragment, alter the buffer size to
		 * remaining data of the container.
		 */
		if (i == num_write_cycles - 1) {
			write_size = last_write_size;
		}
		err = flash_area_write(fcb->fap,
				       FCB_ENTRY_FA_DATA_OFF(loc) + offset,
				       data, write_size);
		if (err) {
			LOG_ERR("Error writing to flash area. err %d", err);
			return;
		}
	}

	/* Finish entry. */
	err = fcb_append_finish(fcb, &loc);
	if (err) {
		LOG_ERR("Error finishing new entry. err %d", err);
		return;
	}

	/* Notify data has been written. */
	struct stg_ack_write_memrec_event *event =
		new_stg_ack_write_memrec_event();
	event->partition = ev.partition;
	EVENT_SUBMIT(event);
	return;
}
static int counter = 0;
static int stg_fcb_walk_cb(struct fcb_entry_ctx *loc_ctx, void *arg)
{
	struct stg_read_memrec_event *ev = (struct stg_read_memrec_event *)arg;

	uint16_t len;
	uint8_t *data = (uint8_t *)ev->new_rec;
	int err;

	len = loc_ctx->loc.fe_data_len;

	err = flash_area_read(loc_ctx->fap, FCB_ENTRY_FA_DATA_OFF(loc_ctx->loc),
			      data, len);
	if (err) {
		LOG_ERR("Error reading from flash area, err %d", err);
		return err;
	}
	mem_rec *mr = (mem_rec *)data;
	printk("ITERATION %i :: HEADER %i :: ELEM_OFF %i :: SECTOR_OFF %i\n",
	       counter, mr->header.len,
	       (int)FCB_ENTRY_FA_DATA_OFF(loc_ctx->loc),
	       (int)loc_ctx->loc.fe_sector->fs_off);
	counter++;

	/* Publish read ack event and wait for consumed ack, 
	 * only then do we read the next entry in the walk process.
	 */
	struct stg_ack_read_memrec_event *event =
		new_stg_ack_read_memrec_event();
	event->partition = ev->partition;
	EVENT_SUBMIT(event);

	err = k_sem_take(&data_consumed_sem,
			 K_SECONDS(CONFIG_WALK_CONSUME_TIMEOUT_SEC));
	if (err) {
		LOG_ERR("Failed to wait for requester to consume.");
		return err;
	}
	return 0;
}

void stg_fcb_read_entry()
{
	struct stg_read_memrec_event ev;
	int err = k_msgq_get(&read_event_msgq, &ev, K_NO_WAIT);
	if (err) {
		LOG_ERR("Error getting read_event from msgq: %d", err);
		return;
	}

	struct fcb *fcb;

	if (ev.partition == STG_PARTITION_ANO) {
		fcb = &ano_fcb;
	} else if (ev.partition == STG_PARTITION_LOG) {
		fcb = &log_fcb;
	} else {
		return;
	}

	err = fcb_walk(fcb, fcb->f_oldest, stg_fcb_walk_cb, &ev);
	if (err) {
		LOG_ERR("Error walking over FCB storage, err %d", err);
		return;
	}
	counter = 0;

	/* Rotate, since the walk process went through with confirmed 
	 * consumed acks from requester.
	 */
	err = fcb_rotate(fcb);
	if (err) {
		LOG_ERR("Error rotating circular buffer after read ack.");
		return;
	}
	LOG_INF("Rotated the circular buffer, pointing at sector %i",
		fcb->f_active_id);
	return;
}

/**
 * @brief Main event handler function. 
 * 
 * @param[in] eh Event_header for the if-chain to 
 *               use to recognize which event triggered.
 * 
 * @return True or false based on if we want to consume the event or not.
 */

static bool event_handler(const struct event_header *eh)
{
	if (is_stg_read_memrec_event(eh)) {
		struct stg_read_memrec_event *ev =
			cast_stg_read_memrec_event(eh);
		printk("Header data is %i", ev->new_rec->header.ID);
		while (k_msgq_put(&read_event_msgq, ev, K_NO_WAIT) != 0) {
			k_msgq_purge(&read_event_msgq);
		}
		return false;
	}
	if (is_stg_write_memrec_event(eh)) {
		struct stg_write_memrec_event *ev =
			cast_stg_write_memrec_event(eh);
		while (k_msgq_put(&write_event_msgq, ev, K_NO_WAIT) != 0) {
			k_msgq_purge(&write_event_msgq);
		}
		return false;
	}
	if (is_stg_data_consumed_event(eh)) {
		k_sem_give(&data_consumed_sem);
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);

EVENT_SUBSCRIBE(MODULE, stg_write_memrec_event);
EVENT_SUBSCRIBE(MODULE, stg_read_memrec_event);
EVENT_SUBSCRIBE(MODULE, stg_data_consumed_event);

void storage_thread_fn()
{
	while (true) {
		int err =
			k_poll(stg_msgq_events, NUM_STG_MSGQ_EVENTS, K_FOREVER);
		if (err == 0) {
			while (k_msgq_num_used_get(&read_event_msgq) > 0) {
				stg_fcb_read_entry();
			}
			while (k_msgq_num_used_get(&write_event_msgq) > 0) {
				stg_fcb_write_entry();
			}
		}
	}
	/* Set all the events to not ready again. */
	for (int i = 0; i < NUM_STG_MSGQ_EVENTS; i++) {
		stg_msgq_events[i].state = K_POLL_STATE_NOT_READY;
	}
}

/* Thread stack area that we use for the storage controller operations 
 * such as read/write.
 */
K_THREAD_DEFINE(storage_thread, CONFIG_STORAGE_THREAD_SIZE, storage_thread_fn,
		NULL, NULL, NULL, CONFIG_STORAGE_THREAD_PRIORITY, 0, 0);