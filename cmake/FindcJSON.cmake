# Copyright 2019-2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# Original Author:
# 2019-2022 Rylie Pavlik <rylie.pavlik@collabora.com> <rylie@ryliepavlik.com>

#.rst:
# FindcJSON
# ---------------
#
# Find the cJSON lightweight JSON parser
#
# Targets
# ^^^^^^^
#
# If successful, the following import target is created.
#
# ``cJSON::cJSON``
#
# Cache variables
# ^^^^^^^^^^^^^^^
#
# The following cache variable may also be set to assist/control the operation of this module:
#
# ``CJSON_ROOT_DIR``
#  The root to search for cJSON.

set(CJSON_ROOT_DIR
    "${CJSON_ROOT_DIR}"
    CACHE PATH "Root to search for cJSON")

include(FindPackageHandleStandardArgs)

# Many distro-provided cJSON config files embed absolute /usr paths, which are
# incorrect for cross builds with a sysroot. Prefer manual discovery there.
if(NOT CMAKE_CROSSCOMPILING)
    # Check for CMake config first on native builds.
    find_package(cJSON QUIET CONFIG)
endif()
if(cJSON_FOUND AND TARGET cjson)
    set_target_properties(cjson PROPERTIES IMPORTED_GLOBAL TRUE)
    # Found config, let's prefer it.
    find_package_handle_standard_args(cJSON CONFIG_MODE)
    set(CJSON_LIBRARY cjson)

else()
    # Manually find
    find_path(
        CJSON_INCLUDE_DIR
        NAMES cjson/cJSON.h
        PATHS ${CJSON_ROOT_DIR}
        PATH_SUFFIXES include)
    find_library(
        CJSON_LIBRARY
        NAMES cjson
        PATHS ${CJSON_ROOT_DIR}
        PATH_SUFFIXES lib lib/aarch64-linux-gnu lib64)

    find_package_handle_standard_args(cJSON REQUIRED_VARS CJSON_INCLUDE_DIR
                                                          CJSON_LIBRARY)
endif()

if(CJSON_FOUND)
    set(CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIR}")
    set(CJSON_LIBRARIES "${CJSON_LIBRARY}")
    if(NOT TARGET cJSON::cJSON)
        if(TARGET "${CJSON_LIBRARY}")
            # Alias if we found the config file
            add_library(cJSON::cJSON ALIAS cjson)
        else()
            add_library(cJSON::cJSON UNKNOWN IMPORTED)
            set_target_properties(
                cJSON::cJSON PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                        "${CJSON_INCLUDE_DIR}")
            set_target_properties(
                cJSON::cJSON PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                                        IMPORTED_LOCATION "${CJSON_LIBRARY}")

        endif()
    endif()
    mark_as_advanced(CJSON_INCLUDE_DIR CJSON_LIBRARY)
endif()

mark_as_advanced(CJSON_ROOT_DIR)
