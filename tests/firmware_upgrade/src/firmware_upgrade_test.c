/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"

/* Variable used to compare the output from dfu module. The sequence should
 * be AS_IS (-1) -> IDLE (0) -> IN_PROGRESS (1) -> REBOOT_SCHEDULED (2).
 */
static int dfu_status_sequence = -1;
static K_SEM_DEFINE(test_frag_rec, 0, 1);
static K_SEM_DEFINE(test_status, 0, 1);

enum test_event_id { TEST_EVENT_APPLY_DFU = 0, TEST_EVENT_EXCEED_SIZE = 1 };
static enum test_event_id cur_id = TEST_EVENT_APPLY_DFU;

/* Want to cycle through all the statuses, if this counter mismatches,
 * we missed a status event.
 */
static int dfu_status_cycle_number = 3;
static int dfu_status_cycle_counter = 0;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	/* Expect the next event to be IDLE event. */
	dfu_status_sequence = DFU_STATUS_IDLE;

	ztest_returns_value(dfu_target_reset, 0);
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	zassert_false(fw_upgrade_module_init(),
		      "Error when initializing firmware upgrade module");
}

static void submit_fragment(uint32_t fragment_size, uint32_t file_size)
{
	/* Set fragment data to 0xAE as we do not care about the contents
	 * but still want to verify that the contents stay unchanged.
	 */
	uint8_t dummy_data[fragment_size];
	memset(dummy_data, 0xAE, sizeof(dummy_data));

	/* Create event. */
	struct dfu_fragment_event *ts = new_dfu_fragment_event(fragment_size);
	zassert_not_null(ts, "Failed to allocate event");

	/* Store fragment and size and submit the event. */
	memcpy(ts->dyndata.data, dummy_data, fragment_size);
	ts->file_size = file_size;
	EVENT_SUBMIT(ts);
}

void test_perform_dfu(void)
{
	/* Expect the next event to be IN_PROGRESS event. */
	dfu_status_sequence = DFU_STATUS_IN_PROGRESS;

	/* Mock dfu writing/setup to internal flash functions. */
	ztest_returns_value(dfu_target_reset, 0);
	ztest_returns_value(dfu_target_mcuboot_set_buf, 0);
	ztest_returns_value(dfu_target_img_type, 0);
	ztest_returns_value(dfu_target_init, 0);

	/* Simulate a file transfer over the event manager, if file sizes
	 * mismatch, or something happens with the fragments, test fails.
	 * We calculate how many fragments is needed based on our dummy file
	 * file size if fragments are 512 in size. 
	 * I.e 2570 / 512 = 5.01953125 => 5 + 1 => 6 fragments
	 * where the last fragment is 0.01953125 * 512 = 10 bytes in size
	 */
	uint32_t file_size = 2570;
	uint32_t fragment_size = 512;
	uint32_t num_fragments = (uint32_t)(file_size / fragment_size) + 1;
	for (uint32_t i = 0; i < num_fragments; i++) {
		if (i == num_fragments - 1) {
			fragment_size = 10;
			ztest_returns_value(dfu_target_done, 0);
		}
		ztest_returns_value(dfu_target_write, 0);
		submit_fragment(fragment_size, file_size);
		int err = k_sem_take(&test_frag_rec, K_SECONDS(30));
		zassert_equal(err, 0, "Test fragment event execution hanged");
	}
	/* Expect the next status event to be reboot. */
	dfu_status_sequence = DFU_STATUS_SUCCESS_REBOOT_SCHEDULED;
}

void test_exceed_file_size(void)
{
	/* Mock dfu writing/setup to internal flash functions. */
	ztest_returns_value(dfu_target_reset, 0);
	ztest_returns_value(dfu_target_mcuboot_set_buf, 0);
	ztest_returns_value(dfu_target_img_type, 0);
	ztest_returns_value(dfu_target_init, 0);
	ztest_returns_value(dfu_target_write, 0);

	/* First it goes to IN_PROGRESS then it goes to an ERROR. */
	dfu_status_sequence = 0;
	submit_fragment(512, 511);

	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

void test_main(void)
{
	ztest_test_suite(firmware_upgrade_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_perform_dfu),
			 ztest_unit_test(test_exceed_file_size));

	ztest_run_test_suite(firmware_upgrade_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_dfu_status_event(eh)) {
		struct dfu_status_event *ev = cast_dfu_status_event(eh);

		if (cur_id == TEST_EVENT_APPLY_DFU) {
			/* This checks if the dfu process goes as normal. 
			 * If we go through all steps from 
			 * IDLE -> PROGRESS -> REBOOT
			 * we know that everything works in regards
			 * to multiple fragments.
			 */
			zassert_ok(ev->dfu_error,
				   "Error occured during dfu process");
			zassert_equal(
				dfu_status_sequence, ev->dfu_status,
				"DFU status did not match expected value.");
			dfu_status_cycle_counter += 1;

			/* If the status indicates we're done, 
			 * check if we have gone
		 	 * through all the states that we expected. 
		 	 */
			if (dfu_status_sequence ==
			    DFU_STATUS_SUCCESS_REBOOT_SCHEDULED) {
				zassert_equal(dfu_status_cycle_counter,
					      dfu_status_cycle_number,
					      "Failed to go through all \
					states expected");

				/* Prepare for next test. */
				cur_id = TEST_EVENT_EXCEED_SIZE;
			}
		} else if (cur_id == TEST_EVENT_EXCEED_SIZE) {
			if (dfu_status_sequence == 0) {
				/* In progress, no errors. */
				zassert_equal(ev->dfu_error, 0,
					      "Error reported at IN PROGRESS.");
				dfu_status_sequence = 1;
			} else if (dfu_status_sequence == 1) {
				/* Error reported, file size should exceed. */
				zassert_not_equal(ev->dfu_error, 0,
						  "No error reported \
					      when it should.");
			}
		}
		k_sem_give(&test_status);
		return false;
	}
	if (is_dfu_fragment_event(eh)) {
		struct dfu_fragment_event *ev = cast_dfu_fragment_event(eh);

		/* This is the content we sent, so this is what we expect. */
		uint8_t dummy_data[ev->dyndata.size];
		memset(dummy_data, 0xAE, sizeof(dummy_data));
		zassert_mem_equal(ev->dyndata.data, dummy_data,
				  ev->dyndata.size, "Memory contents was \
					  not equal for fragment received.");
		/* Contents are valid, give semaphore so we can send next. */
		k_sem_give(&test_frag_rec);
		return false;
	}
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, dfu_status_event);
EVENT_SUBSCRIBE(test_main, dfu_fragment_event);
