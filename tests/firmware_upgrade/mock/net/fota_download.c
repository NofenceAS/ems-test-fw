#include <ztest.h>
#include <zephyr.h>
#include <net/fota_download.h>

enum test_event_id { TEST_EVENT_START = 0, TEST_EVENT_ERROR = 1 };

enum test_event_id current = TEST_EVENT_START;

fota_download_callback_t cb;

void simulate_callback_event()
{
	struct fota_download_evt evt;
	if (current == TEST_EVENT_START) {
		evt.id = FOTA_DOWNLOAD_EVT_FINISHED;
		current = TEST_EVENT_ERROR;
	} else if (current == TEST_EVENT_ERROR) {
		evt.id = FOTA_DOWNLOAD_EVT_ERROR;
		evt.cause = FOTA_DOWNLOAD_ERROR_CAUSE_DOWNLOAD_FAILED;
	}
	cb(&evt);
}

int fota_download_init(fota_download_callback_t client_callback)
{
	cb = client_callback;
	zassert_not_null(cb, "");
	return ztest_get_return_value();
}

int fota_download_start(const char *host, const char *file, int sec_tag,
			uint8_t pdn_id, size_t fragment_size)
{
	/* add zasserts for correct host/path inputs. */
	return ztest_get_return_value();
}