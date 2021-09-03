#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nf_ble_test)

# generate runner for the test
test_runner_generate(src/beacon_processor_test.c)

# create mock for foo module. Add header to relative path 'foo'. That is to
# allow including header using <foo/foo.h>
#cmock_handle(src/foo/foo.h foo)

# add module foo
#target_sources(app PRIVATE src/foo/foo.c)
#target_include_directories(app PRIVATE src)

# add module uut
set(APP_SOURCE ../../src/ble)
target_sources(app PRIVATE ${APP_SOURCE}/beacon_processor.c)
target_include_directories(app PRIVATE ${APP_SOURCE})

# add test file
target_sources(app PRIVATE src/beacon_processor_test.c)
target_include_directories(app PRIVATE .)
target_include_directories(app PRIVATE src/foo)

# Preinclude file to the module under test to redefine IS_ENABLED() macro
# which is used in the module.
set_property(SOURCE ${APP_SOURCE}/beacon_processor.c PROPERTY COMPILE_FLAGS
	"-include src/beacon_processor_test.h")
