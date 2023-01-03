#include "amc_test_common.h"
#include "pasture_structure.h"
#include "amc_cache.h"
#include "amc_dist.h"
#include "embedded.pb.h"
#include <ztest.h>
#include <math.h>
#include <stdlib.h>

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
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 10, .s_y_dm = -10 }, { .s_x_dm = 10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = 10 }, { .s_x_dm = -10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = -10 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	zassert_equal(-10, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 10, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 20, &fence_index, &vertex_index);
	zassert_equal(10, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(10, 30, &fence_index, &vertex_index);
	zassert_equal(20, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(-5, -3, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
	zassert_equal(VERTEX_LEFT, vertex_index, "");

	d = fnc_calc_dist(-5, 3, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
	zassert_equal(VERTEX_LEFT, vertex_index, "");

	d = fnc_calc_dist(5, -3, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(5, 3, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(15, 15, &fence_index, &vertex_index);
	zassert_equal((int16_t)sqrt(5 * 5 + 5 * 5), d, "");

	d = fnc_calc_dist(-15, -15, &fence_index, &vertex_index);
	zassert_equal((int16_t)sqrt(5 * 5 + 5 * 5), d, "");
}

void test_fnc_calc_dist_quadratic_max(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 10, .s_y_dm = -10 },  { .s_x_dm = 10, .s_y_dm = -9 },
		{ .s_x_dm = 10, .s_y_dm = -8 },	  { .s_x_dm = 10, .s_y_dm = -7 },
		{ .s_x_dm = 10, .s_y_dm = -6 },	  { .s_x_dm = 10, .s_y_dm = -5 },
		{ .s_x_dm = 10, .s_y_dm = -4 },	  { .s_x_dm = 10, .s_y_dm = -3 },
		{ .s_x_dm = 10, .s_y_dm = -2 },	  { .s_x_dm = 10, .s_y_dm = -1 },
		{ .s_x_dm = 10, .s_y_dm = 1 },	  { .s_x_dm = 10, .s_y_dm = 2 },
		{ .s_x_dm = 10, .s_y_dm = 3 },	  { .s_x_dm = 10, .s_y_dm = 4 },
		{ .s_x_dm = 10, .s_y_dm = 5 },	  { .s_x_dm = 10, .s_y_dm = 6 },
		{ .s_x_dm = 10, .s_y_dm = 7 },	  { .s_x_dm = 10, .s_y_dm = 8 },
		{ .s_x_dm = 10, .s_y_dm = 9 },	  { .s_x_dm = 10, .s_y_dm = 10 },
		{ .s_x_dm = 9, .s_y_dm = 10 },	  { .s_x_dm = 8, .s_y_dm = 10 },
		{ .s_x_dm = 7, .s_y_dm = 10 },	  { .s_x_dm = 6, .s_y_dm = 10 },
		{ .s_x_dm = 5, .s_y_dm = 10 },	  { .s_x_dm = 4, .s_y_dm = 10 },
		{ .s_x_dm = 3, .s_y_dm = 10 },	  { .s_x_dm = 2, .s_y_dm = 10 },
		{ .s_x_dm = 1, .s_y_dm = 10 },	  { .s_x_dm = 0, .s_y_dm = 10 },
		{ .s_x_dm = -1, .s_y_dm = 10 },	  { .s_x_dm = -2, .s_y_dm = 10 },
		{ .s_x_dm = -3, .s_y_dm = 10 },	  { .s_x_dm = -4, .s_y_dm = 10 },
		{ .s_x_dm = -5, .s_y_dm = 10 },	  { .s_x_dm = -6, .s_y_dm = 10 },
		{ .s_x_dm = -7, .s_y_dm = 10 },	  { .s_x_dm = -10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = -10 }, { .s_x_dm = 10, .s_y_dm = -10 }
	};

	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	zassert_equal(-10, d, "");

	d = fnc_calc_dist(10, 10, &fence_index, &vertex_index);
	zassert_equal(0, d, "");

	d = fnc_calc_dist(10, 20, &fence_index, &vertex_index);
	zassert_equal(10, d, "");

	d = fnc_calc_dist(10, 30, &fence_index, &vertex_index);
	zassert_equal(20, d, "");

	d = fnc_calc_dist(-5, -5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(-5, 5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(5, -5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(5, 5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
}

void test_fnc_calc_dist_rect(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 100, .s_y_dm = -10 }, { .s_x_dm = 100, .s_y_dm = 10 },
		{ .s_x_dm = -100, .s_y_dm = 10 }, { .s_x_dm = -100, .s_y_dm = -10 },
		{ .s_x_dm = 100, .s_y_dm = -10 },

	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	d = fnc_calc_dist(50, 0, &fence_index, &vertex_index);
	zassert_equal(-10, d, "");

	d = fnc_calc_dist(100, 10, &fence_index, &vertex_index);
	zassert_equal(0, d, "");

	d = fnc_calc_dist(50, 20, &fence_index, &vertex_index);
	zassert_equal(10, d, "");

	d = fnc_calc_dist(50, 30, &fence_index, &vertex_index);
	zassert_equal(20, d, "");

	d = fnc_calc_dist(-50, -5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(-50, 5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(50, -5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");

	d = fnc_calc_dist(50, 5, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
}

void test_fnc_calc_dist_2_fences_hole(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 2,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	pasture.fences[1].m.e_fence_type = FenceDefinitionMessage_FenceType_Inverted;
	pasture.fences[1].m.us_id = 0;
	pasture.fences[1].m.fence_no = 1;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 20, .s_y_dm = -20 }, { .s_x_dm = 20, .s_y_dm = 20 },
		{ .s_x_dm = -20, .s_y_dm = 20 }, { .s_x_dm = -20, .s_y_dm = -20 },
		{ .s_x_dm = 20, .s_y_dm = -20 },
	};
	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));

	fence_coordinate_t points2[] = {
		{ .s_x_dm = 10, .s_y_dm = -10 }, { .s_x_dm = 10, .s_y_dm = 10 },
		{ .s_x_dm = -10, .s_y_dm = 10 }, { .s_x_dm = -10, .s_y_dm = -10 },
		{ .s_x_dm = 10, .s_y_dm = -10 },
	};
	pasture.fences[1].m.n_points = sizeof(points2) / sizeof(points2[0]);
	memcpy(pasture.fences[1].coordinates, points2, sizeof(points2));

	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	zassert_equal(10, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_RIGHT, vertex_index, "");

	d = fnc_calc_dist(0, 10, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 14, &fence_index, &vertex_index);
	zassert_equal(-4, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 16, &fence_index, &vertex_index);
	zassert_equal(-4, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 20, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 25, &fence_index, &vertex_index);
	zassert_equal(5, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 105, &fence_index, &vertex_index);
	zassert_equal(85, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 1020, &fence_index, &vertex_index);
	zassert_equal(1000, d, "");
	d = fnc_calc_dist(0, INT16_MAX - 100, &fence_index, &vertex_index);
	zassert_equal(INT16_MAX - 100 - 20, d, "");
}

void test_fnc_calc_dist_2_fences_max_size(void)
{
	const int16_t MAX_FENCE = 32500;

	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 1,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = -MAX_FENCE, .s_y_dm = -MAX_FENCE },
		{ .s_x_dm = MAX_FENCE, .s_y_dm = -MAX_FENCE },
		{ .s_x_dm = MAX_FENCE, .s_y_dm = MAX_FENCE },
		{ .s_x_dm = -MAX_FENCE, .s_y_dm = MAX_FENCE },
		{ .s_x_dm = -MAX_FENCE, .s_y_dm = -MAX_FENCE },

	};

	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));
	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	int16_t d;
	uint8_t fence_index;
	uint8_t vertex_index;

	d = fnc_calc_dist(0, -MAX_FENCE, &fence_index, &vertex_index);
	zassert_equal(0, d, "");

	d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	zassert_equal(-MAX_FENCE, d, "");

	d = fnc_calc_dist(MAX_FENCE, MAX_FENCE, &fence_index, &vertex_index);
	zassert_equal(-0, d, "");

	d = fnc_calc_dist(MAX_FENCE, -MAX_FENCE, &fence_index, &vertex_index);
	zassert_equal(-0, d, "");

	d = fnc_calc_dist(-MAX_FENCE, MAX_FENCE, &fence_index, &vertex_index);
	zassert_equal(-0, d, "");

	d = fnc_calc_dist(-MAX_FENCE, -MAX_FENCE, &fence_index, &vertex_index);
	zassert_equal(-0, d, "");

	/* INT MAX/MIN. */
	d = fnc_calc_dist(0, INT16_MAX, &fence_index, &vertex_index);
	zassert_equal((int32_t)INT16_MAX - MAX_FENCE, d, "");

	d = fnc_calc_dist(0, INT16_MIN, &fence_index, &vertex_index);
	zassert_equal(abs(INT16_MIN + MAX_FENCE), d, "");

	d = fnc_calc_dist(INT16_MAX, INT16_MAX, &fence_index, &vertex_index);
	zassert_equal(round(sqrt(2 * pow(INT16_MAX - MAX_FENCE, 2))), d, "");

	d = fnc_calc_dist(INT16_MIN, INT16_MIN, &fence_index, &vertex_index);
	zassert_equal(round(sqrt(2 * pow(INT16_MIN + MAX_FENCE, 2))), d, "");
}

void test_fnc_calc_dist_2_fences_hole2(void)
{
	/* Pasture. */
	pasture_t pasture = {
		.m.ul_total_fences = 2,
	};

	/* Fences. */
	pasture.fences[0].m.e_fence_type = FenceDefinitionMessage_FenceType_Normal;
	pasture.fences[0].m.us_id = 0;
	pasture.fences[0].m.fence_no = 0;

	pasture.fences[1].m.e_fence_type = FenceDefinitionMessage_FenceType_Inverted;
	pasture.fences[1].m.us_id = 0;
	pasture.fences[1].m.fence_no = 1;

	/* Coordinates. */
	fence_coordinate_t points1[] = {
		{ .s_x_dm = 100, .s_y_dm = -50 },  { .s_x_dm = 100, .s_y_dm = 100 },
		{ .s_x_dm = -100, .s_y_dm = 100 }, { .s_x_dm = -100, .s_y_dm = -50 },
		{ .s_x_dm = 100, .s_y_dm = -50 },
	};

	pasture.fences[0].m.n_points = sizeof(points1) / sizeof(points1[0]);
	memcpy(pasture.fences[0].coordinates, points1, sizeof(points1));

	fence_coordinate_t points2[] = { { .s_x_dm = 90, .s_y_dm = 80 },
					 { .s_x_dm = 90, .s_y_dm = 90 },
					 { .s_x_dm = -90, .s_y_dm = 90 },
					 { .s_x_dm = -90, .s_y_dm = 80 },
					 { .s_x_dm = 90, .s_y_dm = 80 } };

	pasture.fences[1].m.n_points = sizeof(points2) / sizeof(points2[0]);
	memcpy(pasture.fences[1].coordinates, points2, sizeof(points2));

	zassert_false(set_pasture_cache((uint8_t *)&pasture, sizeof(pasture)), "");

	uint8_t fence_index;
	uint8_t vertex_index;
	int16_t d;

	d = fnc_calc_dist(0, 0, &fence_index, &vertex_index);
	zassert_equal(-50, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	/* Up. */
	d = fnc_calc_dist(0, 10, &fence_index, &vertex_index);
	zassert_equal(-60, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	d = fnc_calc_dist(0, 20, &fence_index, &vertex_index);
	zassert_equal(-60, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	d = fnc_calc_dist(0, 30, &fence_index, &vertex_index);
	zassert_equal(-50, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	d = fnc_calc_dist(0, 70, &fence_index, &vertex_index);
	zassert_equal(-10, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	d = fnc_calc_dist(0, 80, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_BOTTOM, vertex_index, "");

	d = fnc_calc_dist(0, 85, &fence_index, &vertex_index);
	zassert_equal(5, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 90, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(1, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 95, &fence_index, &vertex_index);
	zassert_equal(-5, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 100, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 105, &fence_index, &vertex_index);
	zassert_equal(5, d, "");
	zassert_equal(0, fence_index, "");
	zassert_equal(VERTEX_TOP, vertex_index, "");

	d = fnc_calc_dist(0, 1000, &fence_index, &vertex_index);
	zassert_equal(900, d, "");

	/* Down. */
	d = fnc_calc_dist(0, -10, &fence_index, &vertex_index);
	zassert_equal(-40, d, "");
	d = fnc_calc_dist(0, -20, &fence_index, &vertex_index);
	zassert_equal(-30, d, "");
	d = fnc_calc_dist(0, -50, &fence_index, &vertex_index);
	zassert_equal(0, d, "");
}