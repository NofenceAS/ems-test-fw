/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

static K_SEM_DEFINE(write_pasture_ack_sem, 0, 1);
static K_SEM_DEFINE(read_pasture_ack_sem, 0, 1);
static K_SEM_DEFINE(consumed_pasture_ack_sem, 0, 1);

void test_pasture_write(void)
{
	write_data(STG_PARTITION_PASTURE);
	int err = k_sem_take(&write_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write log ack.");
}

void test_pasture_read(void)
{
	/* First we request, then we get callback in event_handler when
	 * data is ready. There might be multiple entries since
	 * last read, so the event_handler can trigger several times
	 * with new entries. We just have to take care of the
	 * consume_event once we've consumed the data.
	 */
	request_data(STG_PARTITION_PASTURE);

	int err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read log ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed log ack.");

	/* We know at this stage that we do not have more entries, test done. */
}

void test_pasture_extended_write_read(void)
{
	/* Update fence three times before reading, and then read. 
	 * This should never happen but its good to have a test for it.
	 */
	int fence_updates = 3;
	for (int i = 0; i < fence_updates; i++) {
		write_data(STG_PARTITION_PASTURE);
		int err = k_sem_take(&write_pasture_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write pasture ack.");
	}
	request_data(STG_PARTITION_PASTURE);

	int err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read pasture ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	/* We know at this stage that we do not have more entries, test done. */
}

static bool event_handler(const struct event_header *eh)
{
	if (is_stg_ack_write_event(eh)) {
		struct stg_ack_write_event *ev = cast_stg_ack_write_event(eh);

		if (ev->partition == STG_PARTITION_PASTURE) {
			k_sem_give(&write_pasture_ack_sem);
		}
		return false;
	}
	if (is_stg_ack_read_event(eh)) {
		struct stg_ack_read_event *ev = cast_stg_ack_read_event(eh);

		if (ev->partition == STG_PARTITION_PASTURE) {
			k_sem_give(&read_pasture_ack_sem);
			/* Consume the data, where we also publish consumed events
			 * once we're done with the data. This continues the walk
			 * process and can trigger further read acks.
			 */
			consume_data(ev);
		}
		return false;
	}
	if (is_stg_consumed_read_event(eh)) {
		if (cur_partition == STG_PARTITION_PASTURE) {
			k_sem_give(&consumed_pasture_ack_sem);
		}
		return false;
	}
	return false;
}

EVENT_LISTENER(test_pasture, event_handler);

EVENT_SUBSCRIBE(test_pasture, stg_ack_write_event);
EVENT_SUBSCRIBE(test_pasture, stg_ack_read_event);
EVENT_SUBSCRIBE(test_pasture, stg_consumed_read_event);
