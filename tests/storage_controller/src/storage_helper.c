
#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "flash_memory.h"
#include "storage_helper.h"

mem_rec cached_mem_rec_read;
mem_rec cached_mem_rec_write;

int write_index_value = 0;
int read_index_value = 0;

void request_data(flash_partition_t partition)
{
	k_sleep(K_SECONDS(1));
	struct stg_read_memrec_event *ev = new_stg_read_memrec_event();

	ev->new_rec = &cached_mem_rec_read;
	ev->partition = partition;

	EVENT_SUBMIT(ev);
}

void consume_data(flash_partition_t partition)
{
	printk("Consuming real data %i, expects %i\n",
	       cached_mem_rec_read.header.ID, read_index_value);

	zassert_equal(cached_mem_rec_read.header.ID, read_index_value,
		      "Contents not equal.");

	/* After contents have been consumed successfully, 
	 * notify the storage controller to continue walk process if there
         * are any new entries.
	 */
	struct stg_data_consumed_event *ev = new_stg_data_consumed_event();
	ev->partition = partition;
	EVENT_SUBMIT(ev);

	consumed_entries_counter++;
	read_index_value++;
}

void write_data(flash_partition_t partition)
{
	printk("Writes data %i\n", write_index_value);
	cached_mem_rec_write.header.ID = write_index_value;

	/* Everytime we write data, increment pointer so the ID changes,
         * so we increment the read value for each read as well and rotate,
         * which means the values should always be the same.
         */
	write_index_value++;

	struct stg_write_memrec_event *ev = new_stg_write_memrec_event();
	ev->new_rec = &cached_mem_rec_write;
	ev->partition = partition;
	EVENT_SUBMIT(ev);
}