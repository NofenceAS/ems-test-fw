/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _AMC_TEST_COMMON_H_
#define _AMC_TEST_COMMON_H_

void test_fnc_calc_dist_quadratic(void);
void test_fnc_calc_dist_quadratic_max(void);
void test_fnc_calc_dist_rect(void);
void test_fnc_calc_dist_2_fences_hole(void);
void test_fnc_calc_dist_2_fences_hole2(void);
void test_fnc_calc_dist_2_fences_max_size(void);

void test_zone_calc(void);

void test_gnss_fix(void);
void test_gnss_mode(void);

void test_collar_status(void);
void test_fence_status(void);

/* GNSS helper functions. */
void amc_gnss_init(void);
void simulate_no_fix(void);
void simulate_fix(void);
void simulate_easy_fix(void);
void simulate_accepted_fix(void);

#endif /* _AMC_TEST_COMMON_H_ */