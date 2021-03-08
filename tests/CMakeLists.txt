# Copyright 2018-2020, Collabora, Ltd. SPDX-License-Identifier: BSL-1.0

# Catch2 main test driver
add_library(tests_main STATIC tests_main.cpp)
target_link_libraries(tests_main PUBLIC xrt-external-catch2)
if(ANDROID)
	target_link_libraries(tests_main PUBLIC log)
endif()


# Input transform test
add_executable(tests_input_transform tests_input_transform.cpp)
target_link_libraries(tests_input_transform PRIVATE tests_main)
target_link_libraries(tests_input_transform PRIVATE
	st_oxr
	xrt-interfaces
	xrt-external-openxr
	aux_util)
add_test(NAME input_transform COMMAND tests_input_transform --success)
