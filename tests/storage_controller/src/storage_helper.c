
#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

uint8_t *write_ptr;

int log_write_index_value = 0;
int log_read_index_value = 0;

int ano_write_index_value = 0;
int ano_read_index_value = 0;

/* We don't need a read value check for this, as we always expect the fence
 * to be the newest data anyways.
 */
uint16_t pasture_write_index_value = 0;

void request_data(flash_partition_t partition)
{
	struct stg_read_event *ev = new_stg_read_event();
	k_sleep(K_SECONDS(10));

	if (partition == STG_PARTITION_LOG) {
		ev->rotate = true;
	} else if (partition == STG_PARTITION_ANO) {
		ev->rotate = true;
	} else if (partition == STG_PARTITION_PASTURE) {
		ev->rotate = false;
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	ev->partition = partition;
	EVENT_SUBMIT(ev);
}

void consume_data(struct stg_ack_read_event *ev)
{
	if (ev->partition == STG_PARTITION_LOG) {
		log_rec_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);

		/** @note Printing size_t variable (which is long unsigned int)
                 *        halts and stucks the test forever unless cast to
                 *        printable (int) value %ul is used.
                 */
		printk("Consuming real log data %i, expects %i\n",
		       (int)rec->header.len, log_read_index_value);
		zassert_equal(rec->header.len, log_read_index_value,
			      "Contents not equal.");
		log_read_index_value++;

		k_free(rec);

	} else if (ev->partition == STG_PARTITION_ANO) {
		ano_rec_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);

		/** @note Printing size_t variable (which is long unsigned int)
                 *        halts and stucks the test forever unless cast to
                 *        printable (int) value or %ul is used.
                 */
		printk("Consuming real ano data %i, expects %i\n",
		       (int)rec->header.len, ano_read_index_value);

		zassert_equal(rec->header.len, ano_read_index_value,
			      "Contents not equal.");
		ano_read_index_value++;

		k_free(rec);
	} else if (ev->partition == STG_PARTITION_PASTURE) {
		fence_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);

		printk("Consuming real fence data %i, expects %i\n",
		       rec->header.us_id, pasture_write_index_value);
		zassert_equal(rec->header.us_id, pasture_write_index_value,
			      "Contents not equal.");
		pasture_write_index_value++;

		k_free(rec);
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	cur_partition = ev->partition;

	/* After contents have been consumed successfully, 
	 * notify the storage controller to continue walk process if there
         * are any new entries.
	 */
	struct stg_consumed_read_event *ev_read = new_stg_consumed_read_event();
	EVENT_SUBMIT(ev_read);

	consumed_entries_counter++;
}

/** @param[out] len output of allocated size
 */
static inline log_rec_t *get_simulated_log_data(size_t *len)
{
	size_t struct_len = sizeof(log_rec_t);
	log_rec_t *rec = k_malloc(struct_len);

	/* Store a dummy variable that is incremented for each write. */
	rec->header.len = log_write_index_value;
	log_write_index_value++;

	*len = struct_len;

	return rec;
}

/** @param[out] len output of allocated size
 */
static inline ano_rec_t *get_simulated_ano_data(size_t *len)
{
	size_t struct_len = sizeof(ano_rec_t);
	ano_rec_t *rec = k_malloc(struct_len);

	rec->header.len = ano_write_index_value;
	ano_write_index_value++;
	*len = struct_len;

	return rec;
}

/** @brief Helper function for writing simulated fence_data.
 * 
 *  @param[in] n_points number of fence points we want to simulate
 *  @param[out] len output of allocated size
 */
static inline fence_t *get_simulated_fence_data(uint8_t n_points, size_t *len)
{
	size_t struct_len =
		sizeof(fence_t) + (n_points * (sizeof(fence_coordinate_t)));
	fence_t *rec = k_malloc(struct_len);

	/* Store a dummy variable that is incremented once
         * it has been written, read and consumed. 
         */
	rec->header.us_id = pasture_write_index_value;
	rec->header.n_points = n_points;
	*len = struct_len;

	return rec;
}

void write_data(flash_partition_t partition)
{
	size_t len;
	bool rotate_to_this;
	struct stg_write_event *ev = new_stg_write_event();

	if (partition == STG_PARTITION_LOG) {
		/* Simulate log data. */
		write_ptr = (uint8_t *)get_simulated_log_data(&len);

		/* Setup event based on partition. */
		rotate_to_this = false;
	} else if (partition == STG_PARTITION_ANO) {
		/* Simulate ano data. */
		write_ptr = (uint8_t *)get_simulated_ano_data(&len);

		/* Setup event based on partition. */
		rotate_to_this = false;
	} else if (partition == STG_PARTITION_PASTURE) {
		/* Simulate fence data. */
		int num_fence_pts = 13;
		write_ptr = (uint8_t *)get_simulated_fence_data(num_fence_pts,
								&len);

		/* Setup event based on partition. */
		rotate_to_this = true;
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	/* Everytime we write data, increment pointer so the ID changes,
         * so we increment the read value for each read as well and rotate,
         * which means the values should always be the same.
         */
	ev->data = write_ptr;
	ev->partition = partition;
	ev->len = len;
	ev->rotate_to_this = rotate_to_this;

	EVENT_SUBMIT(ev);
	/* When we get write_ack, free the data. */
	// k_free(write_ptr), which is done in event_handler
}