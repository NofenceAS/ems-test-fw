
#include <zephyr.h>

enum dfu_target_evt_id { DFU_TARGET_EVT_TIMEOUT, DFU_TARGET_EVT_ERASE_DONE };

typedef void (*dfu_target_callback_t)(enum dfu_target_evt_id evt_id);

int dfu_target_reset(void);

int dfu_target_mcuboot_set_buf(uint8_t *data, size_t len);

int dfu_target_img_type(uint8_t *data, size_t len);

int dfu_target_init(int result, size_t file_size, dfu_target_callback_t cb);

int dfu_target_write(uint8_t *data, size_t len);

int dfu_target_done(bool status);
