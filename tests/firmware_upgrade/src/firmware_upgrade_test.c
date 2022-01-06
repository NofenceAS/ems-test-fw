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

static int event_test_id = 0;

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

static void test_submit_fragment(uint32_t fragment_size, uint32_t file_size)
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

int apply_fragment(void)
{
	ztest_get_return_value();
}

void test_verify_fragment(void)
{
	/* This test verifies that the contents of the fragment is intact
	 * when read from the event handler function. We mock the apply fragment
	 * function since we do not care about that sequence in this test.
	 */
	ztest_returns_values(apply_fragment, 0);
	test_submit_fragment(512, 512);
}

void test_perform_dfu(void)
{
	/* We 
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
	for (uint32_t i = 0; i < num_fragments; ++i) {
		if (i == num_fragments) {
			fragment_size = 10;
			/* Expect the next event to be REBOOT event since
			 * we're finished.
			 */
			dfu_status_sequence =
				DFU_STATUS_SUCCESS_REBOOT_SCHEDULED;
			/* Mock the dfu_done function since we're finished. */
			ztest_returns_value(dfu_target_done, 0);
		}
		ztest_returns_value(dfu_target_write, 0);
		test_submit_fragment(fragment_size, file_size);
	}
}

void test_main(void)
{
	ztest_test_suite(firmware_upgrade_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_perform_dfu));

	ztest_run_test_suite(firmware_upgrade_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_dfu_status_event(eh)) {
		struct dfu_status_event *ev = cast_dfu_status_event(eh);

		/* This checks if the dfu process goes as normal. If we go
		 * through all steps from IDLE -> PROGRESS -> REBOOT
		 * we know that everything works in regards
		 * to multiple fragments.
		 */
		zassert_equal(dfu_status_sequence, ev->dfu_status,
			      "DFU status did not match expected value.");
		return false;
	}
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, dfu_status_event);
