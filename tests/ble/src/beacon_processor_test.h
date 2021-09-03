/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef EXAMPLE_TEST_H_
#define EXAMPLE_TEST_H_

#include <sys/util.h>
#include <stdbool.h>


//#define ring_buf_put TODO

#ifdef NEVER
#undef IS_ENABLED
#define IS_ENABLED(x) runtime_##x

extern bool runtime_CONFIG_UUT_PARAM_CHECK;
#endif

#endif /* EXAMPLE_TEST_H_ */

