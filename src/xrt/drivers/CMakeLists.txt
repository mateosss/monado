# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0


set(ENABLED_HEADSET_DRIVERS)
set(ENABLED_DRIVERS)


if(XRT_BUILD_DRIVER_ARDUINO)
	set(ARDUINO_SOURCE_FILES
		arduino/arduino_device.c
		arduino/arduino_interface.h
		arduino/arduino_prober.c
		)

	add_library(drv_arduino STATIC ${ARDUINO_SOURCE_FILES})
	target_link_libraries(drv_arduino PRIVATE xrt-interfaces aux_util aux_os)
	list(APPEND ENABLED_DRIVERS arduino)
endif()

if(XRT_BUILD_DRIVER_DAYDREAM)
	set(DAYDREAM_SOURCE_FILES
		daydream/daydream_device.c
		daydream/daydream_device.h
		daydream/daydream_interface.h
		daydream/daydream_prober.c
		)

	add_library(drv_daydream STATIC ${DAYDREAM_SOURCE_FILES})
	target_link_libraries(drv_daydream PRIVATE xrt-interfaces aux_util aux_os)
	list(APPEND ENABLED_DRIVERS daydream)
endif()

if(XRT_BUILD_DRIVER_DUMMY)
	set(DUMMY_SOURCE_FILES
		dummy/dummy_hmd.c
		dummy/dummy_interface.h
		dummy/dummy_prober.c
		)

	add_library(drv_dummy STATIC ${DUMMY_SOURCE_FILES})
	target_link_libraries(drv_dummy PRIVATE xrt-interfaces aux_util)
	list(APPEND ENABLED_HEADSET_DRIVERS dummy)
endif()

if(XRT_BUILD_DRIVER_QWERTY)
	set(QWERTY_SOURCE_FILES
		qwerty/qwerty_device.c
		qwerty/qwerty_device.h
		qwerty/qwerty_interface.h
		qwerty/qwerty_prober.c
		qwerty/qwerty_sdl.c
		)

	add_library(drv_qwerty STATIC ${QWERTY_SOURCE_FILES})
	target_link_libraries(drv_qwerty PRIVATE xrt-interfaces aux_util ${SDL2_LIBRARIES})
	list(APPEND ENABLED_DRIVERS qwerty)

	add_library(drv_qwerty_includes INTERFACE)
	target_include_directories(drv_qwerty_includes INTERFACE qwerty)
endif()

if(XRT_BUILD_DRIVER_HDK)
	set(HDK_SOURCE_FILES
		hdk/hdk_device.cpp
		hdk/hdk_device.h
		hdk/hdk_interface.h
		hdk/hdk_prober.c
		)

	add_library(drv_hdk STATIC ${HDK_SOURCE_FILES})
	target_link_libraries(drv_hdk PRIVATE xrt-interfaces aux_math)
	list(APPEND ENABLED_HEADSET_DRIVERS hdk)
endif()


if(XRT_BUILD_DRIVER_HYDRA)
	set(HYDRA_SOURCE_FILES
		hydra/hydra_driver.c
		hydra/hydra_interface.h
		)

	add_library(drv_hydra STATIC ${HYDRA_SOURCE_FILES})
	target_link_libraries(drv_hydra PRIVATE xrt-interfaces aux_os)
	list(APPEND ENABLED_DRIVERS hydra)
endif()

if(XRT_BUILD_DRIVER_NS)
	set(NS_SOURCE_FILES
		north_star/distortion/utility_northstar.h
		north_star/distortion/deformation_northstar.h
		north_star/distortion/deformation_northstar.cpp
		north_star/ns_hmd.h
		north_star/ns_hmd.c
		north_star/ns_interface.h
		north_star/ns_prober.c
		)

	add_library(drv_ns STATIC ${NS_SOURCE_FILES})
	target_link_libraries(drv_ns PRIVATE xrt-interfaces aux_math xrt-external-cjson)
	list(APPEND ENABLED_HEADSET_DRIVERS ns)
endif()

if(XRT_BUILD_DRIVER_OHMD)
	set(OHMD_SOURCE_FILES
		ohmd/oh_device.c
		ohmd/oh_device.h
		ohmd/oh_interface.h
		ohmd/oh_prober.c
		)

	add_library(drv_ohmd STATIC ${OHMD_SOURCE_FILES})
	target_link_libraries(drv_ohmd PRIVATE xrt-interfaces OpenHMD::OpenHMD aux_util aux_math)
	list(APPEND ENABLED_HEADSET_DRIVERS openhmd)
endif()

if(XRT_BUILD_DRIVER_HANDTRACKING)
	set(HT_SOURCE_FILES
		ht/ht_driver.c
		ht/ht_driver.h
		ht/ht_interface.h
		ht/ht_prober.c
		)

	add_library(drv_ht STATIC ${HT_SOURCE_FILES})
	target_link_libraries(drv_ht PRIVATE xrt-interfaces aux_util aux_math)
	list(APPEND ENABLED_DRIVERS ht)
endif()

if(XRT_BUILD_DRIVER_PSMV)
	set(PSMOVE_SOURCE_FILES
		psmv/psmv_driver.c
		psmv/psmv_interface.h
		)

	add_library(drv_psmv STATIC ${PSMOVE_SOURCE_FILES})
	target_link_libraries(drv_psmv PRIVATE xrt-interfaces PUBLIC aux_os aux_tracking)
	list(APPEND ENABLED_DRIVERS psmv)
endif()

if(XRT_BUILD_DRIVER_PSVR)
	set(PSVR_SOURCE_FILES
		psvr/psvr_device.c
		psvr/psvr_device.h
		psvr/psvr_interface.h
		psvr/psvr_packet.c
		psvr/psvr_prober.c
		)

	add_library(drv_psvr STATIC ${PSVR_SOURCE_FILES})
	target_link_libraries(drv_psvr PRIVATE xrt-interfaces HIDAPI::hidapi aux_util)
	target_include_directories(drv_psvr PRIVATE ${HIDAPI_INCLUDE_DIRS})
	list(APPEND ENABLED_HEADSET_DRIVERS psvr)
endif()

if(XRT_BUILD_DRIVER_RS)
	set(RS_SOURCE_FILES
		realsense/rs_6dof.c
		realsense/rs_prober.c
		)

	add_library(drv_rs STATIC ${RS_SOURCE_FILES})
	target_link_libraries(drv_rs PRIVATE xrt-interfaces realsense2::realsense2 aux_util)
	list(APPEND ENABLED_HEADSET_DRIVERS rs)
endif()

if(XRT_BUILD_DRIVER_REMOTE)
	set(REMOTE_SOURCE_FILES
		remote/r_device.c
		remote/r_hmd.c
		remote/r_hub.c
		remote/r_interface.h
		remote/r_internal.h
		)

	add_library(drv_remote STATIC ${REMOTE_SOURCE_FILES})
	target_link_libraries(drv_remote PRIVATE xrt-interfaces aux_util)
	list(APPEND ENABLED_HEADSET_DRIVERS remote)
endif()

set(VIVE_CONFIG_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vive")
if(XRT_BUILD_DRIVER_VIVE)
	set(VIVE_SOURCE_FILES
		vive/vive_device.h
		vive/vive_device.c
		vive/vive_prober.h
		vive/vive_prober.c
		vive/vive_protocol.c
		vive/vive_protocol.h
		vive/vive_controller.h
		vive/vive_controller.c
		vive/vive_lighthouse.h
		vive/vive_lighthouse.c
		)

	add_library(drv_vive STATIC ${VIVE_SOURCE_FILES})
	target_link_libraries(drv_vive PRIVATE xrt-interfaces aux_os aux_util aux_math xrt-external-cjson aux_vive)
	target_link_libraries(drv_vive PRIVATE ${ZLIB_LIBRARIES})
	target_include_directories(drv_vive PRIVATE ${ZLIB_INCLUDE_DIRS})
	list(APPEND ENABLED_HEADSET_DRIVERS vive)
endif()

if(XRT_HAVE_V4L2)
	set(V4L2_SOURCE_FILES
		v4l2/v4l2_driver.c
		)

	add_library(drv_v4l2 STATIC ${V4L2_SOURCE_FILES})
	target_link_libraries(drv_v4l2 PRIVATE xrt-interfaces aux_os)
	list(APPEND ENABLED_DRIVERS v4l2)
endif()

if(XRT_BUILD_DRIVER_VF)
	set(VF_SOURCE_FILES
		vf/vf_driver.c
		)

	add_library(drv_vf STATIC ${VF_SOURCE_FILES})
	target_link_libraries(drv_vf PRIVATE xrt-interfaces aux_os ${GST_LIBRARIES})
	target_include_directories(drv_vf PRIVATE ${GST_INCLUDE_DIRS})
	MESSAGE("GST include ${GST_INCLUDE_DIRS}")
	list(APPEND ENABLED_DRIVERS vf)
endif()

if (XRT_BUILD_DRIVER_SURVIVE)
	set(SURVIVE_SOURCE_FILES
		survive/survive_driver.c
		survive/survive_driver.h
		survive/survive_interface.h
		survive/survive_prober.c
		)

	add_library(drv_survive STATIC ${SURVIVE_SOURCE_FILES})
	target_link_libraries(drv_survive PRIVATE xrt-interfaces aux_os aux_util aux_math aux_vive PkgConfig::SURVIVE)
	list(APPEND ENABLED_HEADSET_DRIVERS survive)
endif()

if(XRT_BUILD_DRIVER_ANDROID)
	set(ANDROID_SOURCE_FILES
		android/android_prober.c
		android/android_prober.h
		android/android_sensors.c
		android/android_sensors.h
		)

	add_library(drv_android STATIC ${ANDROID_SOURCE_FILES})
	target_link_libraries(drv_android PRIVATE xrt-interfaces aux_util aux_os aux_android ${ANDROID_LIBRARY})
	list(APPEND ENABLED_DRIVERS android)
endif()

if (XRT_BUILD_DRIVER_ILLIXR)
	set(ILLIXR_SOURCE_FILES
		illixr/illixr_device.cpp
		illixr/illixr_interface.h
		illixr/illixr_prober.c
		illixr/illixr_component.cpp
		illixr/illixr_component.h
		)

	add_library(drv_illixr STATIC ${ILLIXR_SOURCE_FILES})
	target_link_libraries(drv_illixr PUBLIC ${CMAKE_DL_LIBS} xrt-interfaces aux_util aux_os)
	target_include_directories(drv_illixr PUBLIC ${ILLIXR_PATH})
	target_compile_options(drv_illixr PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
	list(APPEND ENABLED_HEADSET_DRIVERS illixr)
endif()

set(MUlTI_SOURCE_FILES
	multi_wrapper/multi.c
	multi_wrapper/multi.h
	)
add_library(drv_multi STATIC ${MUlTI_SOURCE_FILES})
target_link_libraries(drv_multi PUBLIC xrt-interfaces aux_util)
list(APPEND ENABLED_HEADSET_DRIVERS drv_multi)

if(ENABLED_HEADSET_DRIVERS)
	set(ENABLED_DRIVERS ${ENABLED_HEADSET_DRIVERS} ${ENABLED_DRIVERS})
	list(SORT ENABLED_DRIVERS)
	string(REPLACE ";" " " ENABLED_DRIVERS "${ENABLED_DRIVERS}")
	message(STATUS "Enabled drivers: ${ENABLED_DRIVERS}")
else()
	message(FATAL_ERROR "You must enable at least one headset driver to build Monado.")
endif()
