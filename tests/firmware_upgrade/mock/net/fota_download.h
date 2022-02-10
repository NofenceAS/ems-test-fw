/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _FOTA_DOWNLOAD_H_
#define _FOTA_DOWNLOAD_H_

/**
 * @brief FOTA download event IDs.
 */
enum fota_download_evt_id {
	/** FOTA download progress report. */
	FOTA_DOWNLOAD_EVT_PROGRESS,
	/** FOTA download finished. */
	FOTA_DOWNLOAD_EVT_FINISHED,
	/** FOTA download erase pending. */
	FOTA_DOWNLOAD_EVT_ERASE_PENDING,
	/** FOTA download erase done. */
	FOTA_DOWNLOAD_EVT_ERASE_DONE,
	/** FOTA download error. */
	FOTA_DOWNLOAD_EVT_ERROR,
	/** FOTA download cancelled. */
	FOTA_DOWNLOAD_EVT_CANCELLED
};

/**
 * @brief FOTA download error cause values.
 */
enum fota_download_error_cause {
	/** No error, used when event ID is not FOTA_DOWNLOAD_EVT_ERROR. */
	FOTA_DOWNLOAD_ERROR_CAUSE_NO_ERROR,
	/** Downloading the update failed. The download may be retried. */
	FOTA_DOWNLOAD_ERROR_CAUSE_DOWNLOAD_FAILED,
	/** The update is invalid and was rejected. Retry will not help. */
	FOTA_DOWNLOAD_ERROR_CAUSE_INVALID_UPDATE,
	/** Actual firmware type does not match expected. Retry will not help. */
	FOTA_DOWNLOAD_ERROR_CAUSE_TYPE_MISMATCH,
};

/**
 * @brief FOTA download event data.
 */
struct fota_download_evt {
	enum fota_download_evt_id id;

	union {
		/** Error cause. */
		enum fota_download_error_cause cause;
		/** Download progress %. */
		int progress;
	};
};

typedef void (*fota_download_callback_t)(const struct fota_download_evt *evt);

int fota_download_init(fota_download_callback_t client_callback);
int fota_download_start(const char *host, const char *file, int sec_tag,
			uint8_t pdn_id, size_t fragment_size);

void simulate_callback_event();

#endif /* _FOTA_DOWNLOAD_H_ */