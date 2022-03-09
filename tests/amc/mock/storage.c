#include <ztest.h>
#include "storage.h"
#include "pasture_structure.h"

fence_t *dummy_fence = NULL;
size_t dummy_fence_len = 0;

int stg_read_pasture_data(stg_read_cb cb)
{
	int retval = ztest_get_return_value();
	if (retval) {
		return retval;
	}

	if (dummy_fence != NULL) {
		k_free(dummy_fence);
		dummy_fence = NULL;
	}

	dummy_fence_len = sizeof(fence_t) + (sizeof(fence_coordinate_t) * 4);
	dummy_fence = (fence_t *)k_malloc(dummy_fence_len);

	dummy_fence->header.n_points = 4;
	dummy_fence->header.e_fence_type = 1;
	dummy_fence->header.us_id = 128;

	dummy_fence->p_c[0].s_x_dm = 0xDE;
	dummy_fence->p_c[0].s_y_dm = 0xDE;

	dummy_fence->p_c[1].s_x_dm = 0xAD;
	dummy_fence->p_c[1].s_y_dm = 0xAD;

	dummy_fence->p_c[2].s_x_dm = 0xBE;
	dummy_fence->p_c[2].s_y_dm = 0xBE;

	dummy_fence->p_c[3].s_x_dm = 0xEF;
	dummy_fence->p_c[3].s_y_dm = 0xEF;

	zassert_false(cb((uint8_t *)dummy_fence, dummy_fence_len), "");

	return retval;
}