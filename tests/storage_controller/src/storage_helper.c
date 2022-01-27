
#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "flash_memory.h"
#include "storage_helper.h"

mem_rec cached_mem_rec_read;
mem_rec cached_mem_rec_write;

void request_data(flash_partition_t partition)
{
	struct stg_read_memrec_event *ev = new_stg_read_memrec_event();

	ev->new_rec = &cached_mem_rec_read;
	ev->partition = partition;

	EVENT_SUBMIT(ev);
}

void consume_data(flash_partition_t partition)
{
	zassert_equal(cached_mem_rec_read.header.ID, 1337,
		      "Contents not equal.");

	/* After contents have been consumed successfully, 
	 * notify the storage controller to continue walk process if there
         * are any new entries.
	 */
	struct stg_data_consumed_event *ev = new_stg_data_consumed_event();
	ev->partition = partition;
	EVENT_SUBMIT(ev);
}

void write_data(flash_partition_t partition)
{
	//memset(&cached_mem_rec_write, 0, sizeof(mem_rec));
	cached_mem_rec_write.header.ID = 1337;

	struct stg_write_memrec_event *ev = new_stg_write_memrec_event();
	ev->new_rec = &cached_mem_rec_write;
	ev->partition = partition;
	EVENT_SUBMIT(ev);
}