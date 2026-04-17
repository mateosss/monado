# Copyright 2019-2021, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

add_library(
	st_prober STATIC
	p_documentation.h
	p_dump.c
	p_prober.c
	p_prober.h
	p_tracking.c
	)

target_link_libraries(st_prober PUBLIC xrt-interfaces)
target_link_libraries(
	st_prober
	PRIVATE
		drv_includes
		drv_multi
		aux_util
		aux_util_sink
		aux_os
		aux_tracking
	)

# Add libudev
if(XRT_HAVE_LIBUDEV)
	target_sources(st_prober PRIVATE p_udev.c)
	target_include_directories(st_prober PRIVATE ${UDEV_INCLUDE_DIRS})
	target_link_libraries(st_prober PRIVATE ${UDEV_LIBRARIES})
endif()

# Add libusb
if(XRT_HAVE_LIBUSB)
	target_sources(st_prober PRIVATE p_libusb.c p_hid_libusb.c)
	target_include_directories(st_prober PUBLIC ${LIBUSB1_INCLUDE_DIRS})
	target_link_libraries(st_prober PRIVATE ${LIBUSB1_LIBRARIES})
endif()

# Add libuvc
if(XRT_HAVE_LIBUVC)
	target_sources(st_prober PRIVATE p_libuvc.c)
	target_include_directories(st_prober PRIVATE ${LIBUVC_INCLUDES})
	target_link_libraries(st_prober PRIVATE ${LIBUVC_LIBRARIES})
endif()

if(XRT_HAVE_V4L2)
	# Uses v4l2_fs_create
	target_link_libraries(st_prober PRIVATE drv_v4l2)
endif()

if(XRT_BUILD_DRIVER_VF)
	target_link_libraries(st_prober PRIVATE drv_vf)
endif()
