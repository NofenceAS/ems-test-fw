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
static K_SEM_DEFINE(test_status, 0, 1);

enum test_event_id {
	TEST_EVENT_APPLY_DFU = 0,
	TEST_EVENT_EXCEED_SIZE = 1,
	TEST_EVENT_INIT = 2
};
static enum test_event_id cur_id = TEST_EVENT_INIT;

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
	cur_id = TEST_EVENT_INIT;
	/* Expect the next event to be IDLE event. */
	dfu_status_sequence = DFU_STATUS_IDLE;

	ztest_returns_value(dfu_target_reset, 0);
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	zassert_false(fw_upgrade_module_init(),
		      "Error when initializing firmware upgrade module");

	/* See if we get the 'IDLE' flag as it should be after init. */
	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution \
		hanged waiting for IDLE.");
}

void test_apply_fragment(void)
{
	cur_id = TEST_EVENT_APPLY_DFU;

	/* Mock dfu writing/setup to internal flash functions. */
	ztest_returns_value(dfu_target_reset, 0);
	ztest_returns_value(dfu_target_mcuboot_set_buf, 0);
	ztest_returns_value(dfu_target_img_type, 0);
	ztest_returns_value(dfu_target_init, 0);

	/* Simulate a file transfer to the apply_fragment, if file sizes
	 * mismatch, or something happens with the fragments, test fails.
	 * We calculate how many fragments is needed based on our dummy file
	 * file size if fragments are 512 in size. 
	 * I.e 2570 / 512 = 5.01953125 => 5 + 1 => 6 fragments
	 * where the last fragment is 0.01953125 * 512 = 10 bytes in size
	 */
	int err = 0;
	uint32_t file_size = 2570;
	uint32_t fragment_size = 512;
	uint32_t num_fragments = (uint32_t)(file_size / fragment_size) + 1;
	for (uint32_t i = 0; i < num_fragments; i++) {
		if (i == num_fragments - 1) {
			fragment_size = 10;
			ztest_returns_value(dfu_target_done, 0);
		}

		uint8_t dummy_data[fragment_size];
		memset(dummy_data, 0xAE, sizeof(dummy_data));

		ztest_returns_value(dfu_target_write, 0);
		err = apply_fragment(dummy_data, fragment_size, file_size);
		zassert_equal(err, 0, "Error when applying fragment to flash");
	}

	/* See if we get the 'REBOOT_SCHEDULED' flag 
	 * as the DFU process should be finished. 
	 * There is also an assert to check that the flag is correct in
	 * event handler function.
	 */
	err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution \
		hanged waiting for REBOOT_SCHEDULED.");
}

void test_exceed_file_size(void)
{
	cur_id = TEST_EVENT_EXCEED_SIZE;

	/* Mock dfu writing/setup to internal flash functions. */
	ztest_returns_value(dfu_target_reset, 0);
	ztest_returns_value(dfu_target_mcuboot_set_buf, 0);
	ztest_returns_value(dfu_target_img_type, 0);
	ztest_returns_value(dfu_target_init, 0);
	ztest_returns_value(dfu_target_write, 0);

	int fragment_size = 512;
	uint8_t dummy_data[fragment_size];
	memset(dummy_data, 0xAE, sizeof(dummy_data));

	int err = apply_fragment(dummy_data, fragment_size, fragment_size - 1);
	zassert_not_equal(err, 0,
			  "Did not fail with exceeding memory when it should.");

	/* Should also see error on the status event, wait for it and see. */
	err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution hanged.");
}

void test_main(void)
{
	ztest_test_suite(firmware_upgrade_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_apply_fragment),
			 ztest_unit_test(test_exceed_file_size));

	ztest_run_test_suite(firmware_upgrade_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_dfu_status_event(eh)) {
		struct dfu_status_event *ev = cast_dfu_status_event(eh);

		if (cur_id == TEST_EVENT_EXCEED_SIZE) {
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
		} else if (cur_id == TEST_EVENT_APPLY_DFU) {
			/* We only give semaphore if dfu process went through
			 * and successfully changed to REBOOT flag.
			 */
			if (ev->dfu_status !=
			    DFU_STATUS_SUCCESS_REBOOT_SCHEDULED) {
				return false;
			}
		} else if (cur_id == TEST_EVENT_INIT) {
			zassert_equal(
				ev->dfu_status, DFU_STATUS_IDLE,
				"Status not idle as it should after init.");
		}
		k_sem_give(&test_status);
		return false;
	}
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, dfu_status_event);
