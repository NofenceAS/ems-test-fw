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
		      "Test execution hanged waiting for write pasture ack.");
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
		      "Test execution hanged waiting for read pasture ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	/* We know at this stage that we do not have more entries, test done. */
}

void test_pasture_extended_write_read(void)
{
	cur_test_id = TEST_ID_PASTURE_DYNAMIC;

	/* Set random seed. */
	srand(k_uptime_get_32());

	/* Update fence 300 times before reading. 
	 * This should never happen but its good to have a test for it. Here 
	 * we test that we can exceed the FCB, and still read out the 
	 * needed expected fence.
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
	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	cur_test_id = TEST_ID_NORMAL;
}

void test_reboot_persistent_pasture(void)
{
	/* Write something. */
	write_pasture_data(13);
	int err = k_sem_take(&write_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write pasture ack.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */
	err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	/* Read something. */
	request_pasture_data();

	err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read pasture ack.");
	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");
}

/** @brief Test to check that the fcb_offset_last_n function works
 *         if there have been no writes between the requests.
 */
void test_request_pasture_twice(void)
{
	/* Write once, request twice. */
	write_pasture_data(13);
	int err = k_sem_take(&write_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write pasture ack.");

	/* Read something. */
	request_pasture_data();

	err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read pasture ack.");
	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	k_sleep(K_SECONDS(5));

	/* Read something. */
	request_pasture_data();

	err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read pasture ack.");
	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");
}