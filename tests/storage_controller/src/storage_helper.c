
#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

ano_rec_t ano_cache_read;
ano_rec_t ano_cache_write;

log_rec_t log_cache_read;
log_rec_t log_cache_write;

fence_t fence_cache_read;
fence_t fence_cache_write;

int log_write_index_value = 0;
int log_read_index_value = 0;

int ano_write_index_value = 0;
int ano_read_index_value = 0;

/* We don't need a read value check for this, as we always expect the fence
 * to be the newest data anyways.
 */
int pasture_write_index_value = 0;

void request_data(flash_partition_t partition)
{
	struct stg_read_event *ev = new_stg_read_event();
	void *struct_ptr;
	if (partition == STG_PARTITION_LOG) {
		struct_ptr = &log_cache_read;
		ev->rotate = true;
		ev->newest_entry_only = false;
	} else if (partition == STG_PARTITION_ANO) {
		struct_ptr = &ano_cache_read;
		ev->rotate = true;
		ev->newest_entry_only = false;
	} else if (partition == STG_PARTITION_PASTURE) {
		struct_ptr = &fence_cache_read;
		ev->rotate = false;
		ev->newest_entry_only = true;
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	ev->data = struct_ptr;
	ev->partition = partition;

	EVENT_SUBMIT(ev);
}

void consume_data(flash_partition_t partition)
{
	struct stg_consumed_read_event *ev = new_stg_consumed_read_event();
	if (partition == STG_PARTITION_LOG) {
		printk("Consuming real log data %i, expects %i\n",
		       log_cache_read.header.len, log_read_index_value);
		zassert_equal(log_cache_read.header.len, log_read_index_value,
			      "Contents not equal.");
		log_read_index_value++;

	} else if (partition == STG_PARTITION_ANO) {
		printk("Consuming real ano data %i, expects %i\n",
		       ano_cache_read.header.len, ano_read_index_value);
		zassert_equal(ano_cache_read.header.len, ano_read_index_value,
			      "Contents not equal.");
		ano_read_index_value++;
	} else if (partition == STG_PARTITION_PASTURE) {
		printk("Consuming real log data %i, expects %i\n",
		       fence_cache_read.header.us_id,
		       pasture_write_index_value);
		zassert_equal(fence_cache_read.header.us_id,
			      pasture_write_index_value, "Contents not equal.");
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	/* After contents have been consumed successfully, 
	 * notify the storage controller to continue walk process if there
         * are any new entries.
	 */
	ev->partition = partition;
	EVENT_SUBMIT(ev);

	consumed_entries_counter++;
}

void write_data(flash_partition_t partition)
{
	void *struct_ptr;
	struct stg_write_event *ev = new_stg_write_event();
	if (partition == STG_PARTITION_LOG) {
		printk("Writes log data %i\n", log_write_index_value);
		log_cache_write.header.len = log_write_index_value;

		struct_ptr = &log_cache_write;
		log_write_index_value++;
		ev->rotate = false;
	} else if (partition == STG_PARTITION_ANO) {
		printk("Writes ano data %i\n", ano_write_index_value);
		ano_cache_write.header.len = ano_write_index_value;

		struct_ptr = &ano_cache_write;
		ano_write_index_value++;
		ev->rotate = false;
	} else if (partition == STG_PARTITION_PASTURE) {
		pasture_write_index_value++;
		printk("Writes pasture data %i\n", pasture_write_index_value);
		fence_cache_write.header.us_id = pasture_write_index_value;

		struct_ptr = &fence_cache_write;
		ev->rotate = true;
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	/* Everytime we write data, increment pointer so the ID changes,
         * so we increment the read value for each read as well and rotate,
         * which means the values should always be the same.
         */
	ev->data = struct_ptr;
	ev->partition = partition;
	EVENT_SUBMIT(ev);
}