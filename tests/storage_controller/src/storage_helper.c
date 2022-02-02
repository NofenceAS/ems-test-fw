
#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

uint8_t *write_log_ptr;
uint8_t *write_ano_ptr;
uint8_t *write_pasture_ptr;

int log_write_index_value = 0;
int log_read_index_value = 0;

int ano_write_index_value = 0;
int ano_read_index_value = 0;

/* We don't need a read value check for this, as we always expect the fence
 * to be the newest data anyways.
 */
uint16_t pasture_write_index_value = 0;

void request_log_data()
{
	k_sleep(K_SECONDS(10));
	struct stg_read_event *ev = new_stg_read_event();

	ev->rotate = true;
	ev->partition = STG_PARTITION_LOG;
	EVENT_SUBMIT(ev);
}

void request_ano_data()
{
	k_sleep(K_SECONDS(10));
	struct stg_read_event *ev = new_stg_read_event();

	ev->rotate = true;
	ev->partition = STG_PARTITION_ANO;
	EVENT_SUBMIT(ev);
}

void request_pasture_data()
{
	k_sleep(K_SECONDS(10));
	struct stg_read_event *ev = new_stg_read_event();

	ev->rotate = false;
	ev->partition = STG_PARTITION_PASTURE;
	EVENT_SUBMIT(ev);
}

void consume_data(struct stg_ack_read_event *ev)
{
	if (ev->partition == STG_PARTITION_LOG) {
		log_rec_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);
		zassert_equal(rec->header.len, log_read_index_value,
			      "Contents not equal.");
		log_read_index_value++;

		k_free(rec);

	} else if (ev->partition == STG_PARTITION_ANO) {
		ano_rec_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);
		zassert_equal(rec->header.len, ano_read_index_value,
			      "Contents not equal.");
		ano_read_index_value++;

		k_free(rec);
	} else if (ev->partition == STG_PARTITION_PASTURE) {
		fence_t *rec = k_malloc(ev->len);
		memcpy(rec, ev->data, ev->len);
		zassert_equal(rec->header.us_id, pasture_write_index_value,
			      "Contents not equal.");
		pasture_write_index_value++;
		if (cur_test_id == TEST_ID_DYNAMIC) {
			/* Check if the last entry was the expected size of
                         * 100 fence points. Both using n_points from the
                         * header, but also sizeof(fence_t) calculation.
                         */
			zassert_equal(rec->header.n_points, 100, "");
			int num_points = (ev->len - (sizeof(fence_t))) /
					 sizeof(fence_coordinate_t);
			zassert_equal(num_points, 100, "");
		} else {
			/* If not dynamic test, we always simulate 13 fence
                         * points.
                         */
			zassert_equal(rec->header.n_points, 13, "");
			int num_points = (ev->len - (sizeof(fence_t))) /
					 sizeof(fence_coordinate_t);
			zassert_equal(num_points, 13, "");
		}
		k_free(rec);
	} else {
		zassert_unreachable("Unknown partition given.");
		return;
	}

	cur_partition = ev->partition;
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

void write_log_data()
{
	size_t len;
	struct stg_write_event *ev = new_stg_write_event();
	write_log_ptr = (uint8_t *)get_simulated_log_data(&len);

	ev->data = write_log_ptr;
	ev->partition = STG_PARTITION_LOG;
	ev->len = len;
	ev->rotate_to_this = false;

	EVENT_SUBMIT(ev);
	/* When we get write_ack, free the data. */
	// k_free(write_ptr), which is done in event_handler
}

void write_ano_data()
{
	size_t len;
	struct stg_write_event *ev = new_stg_write_event();
	write_ano_ptr = (uint8_t *)get_simulated_ano_data(&len);

	ev->data = write_ano_ptr;
	ev->partition = STG_PARTITION_ANO;
	ev->len = len;
	ev->rotate_to_this = false;

	EVENT_SUBMIT(ev);
	/* When we get write_ack, free the data. */
	// k_free(write_ptr), which is done in event_handler
}

void write_pasture_data(uint8_t num_points)
{
	size_t len;
	struct stg_write_event *ev = new_stg_write_event();
	write_pasture_ptr =
		(uint8_t *)get_simulated_fence_data(num_points, &len);

	ev->data = write_pasture_ptr;
	ev->partition = STG_PARTITION_PASTURE;
	ev->len = len;
	ev->rotate_to_this = true;

	EVENT_SUBMIT(ev);
	/* When we get write_ack, free the data. */
	// k_free(write_ptr), which is done in event_handler
}