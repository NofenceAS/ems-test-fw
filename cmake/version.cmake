# SPDX-License-Identifier: Apache-2.0

#.rst:
# version.cmake
# -------------
#
# Inputs:
#
#   ``*VERSION*`` and other constants set by
#   Nofence maintainers
#
# Outputs with examples::
#
#   X25_VERSION_NUMBER        2000
#
# Most outputs are converted to C macros, see ``version.h.in``
#


include(${ZEPHYR_BASE}/cmake/hex.cmake)
file(READ ${CMAKE_SOURCE_DIR}/VERSION ver)

string(REGEX MATCH "X25_VERSION_NUMBER = ([0-9]*)" _ ${ver})
set(PROJECT_X25_VERSION_NUMBER ${CMAKE_MATCH_1})
