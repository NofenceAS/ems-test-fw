#include "amc_test_common.h"
#include "pasture_structure.h"
#include "amc_cache.h"
#include "amc_dist.h"
#include "embedded.pb.h"
#include <ztest.h>
#include <math.h>

int16_t g_i16_warn_start_way_p[2];
int16_t g_i16_heading;

const uint8_t VERTEX_RIGHT = 1;
const uint8_t VERTEX_TOP = 2;
const uint8_t VERTEX_LEFT = 3;
const uint8_t VERTEX_BOTTOM = 4;

void test_fnc_calc_dist_quadratic(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type =
		FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = -10 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)),
		      "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	//d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	//printk("D is %i\n", d);
	//zassert_equal(-10, d, "");
	//zassert_equal(0, fence_index, "");
	//zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 10, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, 0);
	//zassert_equal(0, d, "");
	//zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 20, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, 10);
	zassert_equal(10, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 30, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, 20);
	//zassert_equal(20, d, "");
	//zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(-5, -3, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, -5);
	//zassert_equal(-5, d, "");
	//zassert_equal(VERTEX_LEFT, vertex_index, "");

	d = fnc_calc_dist(-5, 3, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, -5);
	//zassert_equal(-5, d, "");
	//zassert_equal(VERTEX_LEFT, vertex_index, "");

	d = fnc_calc_dist(5, -3, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, -5);
	//zassert_equal(-5, d, "");
	//zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(5, 3, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, -5);
	//zassert_equal(-5, d, "");
	//zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(15, 15, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, (int16_t)sqrt(5 * 5 + 5 * 5));
	//zassert_equal((int16_t)sqrt(5 * 5 + 5 * 5), d, "");

	d = fnc_calc_dist(-15, -15, &fence_index, &vertex_index);
	printk("D is %i, should be %i\n", d, (int16_t)sqrt(5 * 5 + 5 * 5));
	//zassert_equal((int16_t)sqrt(5 * 5 + 5 * 5), d, "");
}