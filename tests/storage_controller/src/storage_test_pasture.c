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

#include "pm_config.h"
#include <stdlib.h>

void test_pasture_write(void)
{
	write_pasture_data(13);
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
	request_pasture_data();

	int err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read log ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed log ack.");

	/* We know at this stage that we do not have more entries, test done. */
}

/** @brief Tests what happens when we mix up the request/write calls to
 *         the storage controller, which is bound to happen in the system
 *         since multiple modules operate simultaneously and will 
 *         both request and write at the same time.
 */
void test_pasture_log_ano_write_read(void)
{
	/* Mix up the function calls, do not wait for semaphores.
	 * You cannot submit the same event subsequently, especially 
	 * not from the same thread since that would
	 * make the event_handler thread not process any of the events
	 * but the last. So duplicate calls is not performed in this test. 
	 */
	write_pasture_data(13);
	write_log_data();
	request_log_data();
	write_ano_data();
	request_ano_data();
	request_pasture_data();

	k_sleep(K_SECONDS(30));

	/* Test finished when we've consumed all the data, these are
	 * for log, ano and pasture data. However, we only need to check
	 * if we've consumed the pasture data, since that is the last
	 * function call. (Keep in mind that we also check the contents
	 * of every other function call in storage_helper.c)
	 * 
	 * Since the storage controller runs on a separate thread and uses
	 * message queue, we expect it to process all of the above, 
	 * regardless of order.
	 */
	int err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	k_sleep(K_SECONDS(30));
}

void test_pasture_extended_write_read(void)
{
	cur_test_id = TEST_ID_DYNAMIC;

	/* Set random seed. */
	srand(k_uptime_get_32());

	/* Update fence 300 times before reading, and then read. 
	 * This should never happen but its good to have a test for it.
	 */
	int fence_updates = 300;
	for (int i = 0; i < fence_updates; i++) {
		uint8_t num_fence_points = (uint8_t)(rand() % UINT8_MAX);

		/* Our last write is the one we will read, set it to a custom
		 * number we can check once we consume it.
		 */
		if (i == fence_updates - 1) {
			num_fence_points = 100;
		}

		write_pasture_data(num_fence_points);
		int err = k_sem_take(&write_pasture_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write pasture ack.");

		/* Sleep so we have time to process all the duplicate event
		 * submissions on the event_handler thread.
		 */
		k_sleep(K_MSEC(50));
	}
	request_pasture_data();

	int err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read pasture ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");
}