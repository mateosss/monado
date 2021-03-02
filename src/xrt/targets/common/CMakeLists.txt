# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0


####
# Lists
#

add_library(target_lists STATIC
	target_lists.c
	)
target_link_libraries(target_lists PRIVATE
	xrt-interfaces
	aux_util # TODO Remove this after removing #include "util/u_time.h" from xrt_defines.h
	)
target_include_directories(target_lists PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../drivers
	)
target_include_directories(target_lists PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	)

if(XRT_BUILD_DRIVER_ARDUINO)
	target_link_libraries(target_lists PRIVATE drv_arduino)
endif()

if(XRT_BUILD_DRIVER_DAYDREAM)
	target_link_libraries(target_lists PRIVATE drv_daydream)
endif()

if(XRT_BUILD_DRIVER_DUMMY)
	target_link_libraries(target_lists PRIVATE drv_dummy)
endif()

if(XRT_BUILD_DRIVER_HDK)
	target_link_libraries(target_lists PRIVATE drv_hdk)
endif()

if(XRT_BUILD_DRIVER_HYDRA)
	target_link_libraries(target_lists PRIVATE drv_hydra)
endif()

if(XRT_BUILD_DRIVER_NS)
	target_link_libraries(target_lists PRIVATE drv_ns)
endif()

if(XRT_BUILD_DRIVER_OHMD)
	target_link_libraries(target_lists PRIVATE drv_ohmd)
endif()

if(XRT_BUILD_DRIVER_HANDTRACKING)
	target_link_libraries(target_lists PRIVATE drv_ht)
endif()

if(XRT_BUILD_DRIVER_PSMV)
	target_link_libraries(target_lists PRIVATE drv_psmv)
endif()

if(XRT_BUILD_DRIVER_PSVR)
	target_link_libraries(target_lists PRIVATE drv_psvr)
endif()

if(XRT_BUILD_DRIVER_RS)
	target_link_libraries(target_lists PRIVATE drv_rs)
endif()

if(XRT_BUILD_DRIVER_REMOTE)
	target_link_libraries(target_lists PRIVATE drv_remote)
endif()

if(XRT_HAVE_V4L2)
	target_link_libraries(target_lists PRIVATE drv_v4l2)
endif()

if(XRT_BUILD_DRIVER_VF)
	target_link_libraries(target_lists PRIVATE drv_vf)
endif()

if(XRT_BUILD_DRIVER_VIVE)
	target_link_libraries(target_lists PRIVATE drv_vive)
endif()

if(XRT_BUILD_DRIVER_SURVIVE)
	target_link_libraries(target_lists PRIVATE drv_survive)
endif()

if(XRT_BUILD_DRIVER_ANDROID)
	target_link_libraries(target_lists PRIVATE drv_android)
endif()

if(XRT_BUILD_DRIVER_ILLIXR)
	target_link_libraries(target_lists PRIVATE drv_illixr)
endif()

target_link_libraries(target_lists PRIVATE drv_multi)

if(XRT_BUILD_DRIVER_QWERTY)
	target_link_libraries(target_lists PRIVATE drv_qwerty)
endif()

####
# Instance
#
if(XRT_FEATURE_COMPOSITOR_MAIN)
	add_library(target_instance STATIC
		target_instance.c
		)
	target_link_libraries(target_instance PRIVATE
		xrt-interfaces
		aux_util
		st_prober
		target_lists
		comp_main
		)
	target_include_directories(target_instance PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../../drivers
		)
	target_include_directories(target_instance PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		)
endif()

####
# Instance without Compositor
#

add_library(target_instance_no_comp STATIC
	target_instance_no_comp.c
	)
target_link_libraries(target_instance_no_comp PRIVATE
	xrt-interfaces
	aux_util
	st_prober
	target_lists
	)
target_include_directories(target_instance_no_comp PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../drivers
	)
target_include_directories(target_instance_no_comp PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	)
