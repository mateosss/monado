# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

drv_include = include_directories('.')

lib_drv_dummy = static_library(
	'drv_dummy',
	files(
		'dummy/dummy_hmd.c',
		'dummy/dummy_interface.h',
		'dummy/dummy_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'dummy' in drivers,
)

lib_drv_hdk = static_library(
	'drv_hdk',
	files(
		'hdk/hdk_device.cpp',
		'hdk/hdk_device.h',
		'hdk/hdk_interface.h',
		'hdk/hdk_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'hdk' in drivers,
)

lib_drv_hydra = static_library(
	'drv_hydra',
	files(
		'hydra/hydra_driver.c',
		'hydra/hydra_interface.h',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'hydra' in drivers,
)

lib_drv_ns = static_library(
	'drv_ns',
	files(
		'north_star/distortion/utility_northstar.h',
		'north_star/distortion/deformation_northstar.h',
		'north_star/distortion/deformation_northstar.cpp',
		'north_star/ns_hmd.h',
		'north_star/ns_hmd.c',
		'north_star/ns_interface.h',
		'north_star/ns_prober.c',
	),
	include_directories: [
		xrt_include,
		cjson_include,
	],
	dependencies: [aux, xrt_config_drivers],
	build_by_default: 'ns' in drivers,
)

lib_drv_ht = static_library(
	'drv_ht',
	files(
		'ht/ht_driver.c',
		'ht/ht_driver.h',
		'ht/ht_interface.h',
		'ht/ht_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'handtracking' in drivers,
)

lib_drv_ohmd = static_library(
	'drv_ohmd',
	files(
		'ohmd/oh_device.c',
		'ohmd/oh_device.h',
		'ohmd/oh_interface.h',
		'ohmd/oh_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux, openhmd],
	build_by_default: 'ohmd' in drivers,
)

lib_drv_psmv = static_library(
	'drv_psmv',
	files(
		'psmv/psmv_driver.c',
		'psmv/psmv_interface.h',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'psmv' in drivers,
)

lib_drv_psvr = static_library(
	'drv_psvr',
	files(
		'psvr/psvr_device.c',
		'psvr/psvr_device.h',
		'psvr/psvr_interface.h',
		'psvr/psvr_packet.c',
		'psvr/psvr_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux, hidapi],
	build_by_default: 'psvr' in drivers,
)

lib_drv_rs = static_library(
	'drv_rs',
	files(
		'realsense/rs_6dof.c',
		'realsense/rs_interface.h',
		'realsense/rs_prober.c',
	),
	include_directories: xrt_include,
	dependencies: [aux, rs],
	build_by_default: 'rs' in drivers,
)

lib_drv_remote = static_library(
	'drv_remote',
	files(
		'remote/r_device.c',
		'remote/r_hmd.c',
		'remote/r_hub.c',
		'remote/r_interface.h',
		'remote/r_internal.h',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'remote' in drivers,
)

lib_drv_vf = static_library(
	'drv_vf',
	files(
		'vf/vf_driver.c'
	),
	include_directories: xrt_include,
	dependencies: [aux, gst, gst_app, gst_video],
	build_by_default: 'vf' in drivers,
)

drv_vf = declare_dependency(
	include_directories: drv_include,
	link_with: lib_drv_vf,
)

lib_drv_v4l2 = static_library(
	'drv_v4l2',
	files(
		'v4l2/v4l2_driver.c',
	),
	include_directories: xrt_include,
	dependencies: [aux],
	build_by_default: 'v4l2' in drivers,
)

lib_drv_vive = static_library(
	'drv_vive',
	files(
		'vive/vive_device.c',
		'vive/vive_device.h',
		'vive/vive_protocol.c',
		'vive/vive_protocol.h',
		'vive/vive_prober.h',
		'vive/vive_prober.c',
		'vive/vive_controller.c',
		'vive/vive_controller.h',
		'vive/vive_lighthouse.c',
		'vive/vive_lighthouse.h',
	),
	include_directories: [
		xrt_include,
		aux_include,
		cjson_include,
	],
	dependencies: [aux, zlib, aux_vive],
	build_by_default: 'vive' in drivers,
)

lib_drv_survive = static_library(
	'drv_survive',
	files(
		'survive/survive_driver.c',
		'survive/survive_driver.h',
		'survive/survive_interface.h',
		'survive/survive_prober.c',
	),
	include_directories: [
		xrt_include,
		aux_include,
		cjson_include,
	],
	dependencies: [aux, zlib, survive, aux_vive],
	build_by_default: 'survive' in drivers,
)

lib_drv_daydream = static_library(
	'drv_daydream',
	files(
		'daydream/daydream_device.c',
		'daydream/daydream_device.h',
		'daydream/daydream_interface.h',
		'daydream/daydream_prober.c',
	),
	include_directories: [
		xrt_include,
	],
	dependencies: [dbus, aux],
	build_by_default: 'daydream' in drivers,
)

lib_drv_arduino = static_library(
	'drv_arduino',
	files(
		'arduino/arduino_device.c',
		'arduino/arduino_interface.h',
		'arduino/arduino_prober.c',
	),
		include_directories: [
		xrt_include,
	],
	dependencies: [dbus, aux],
	build_by_default: 'arduino' in drivers,
)

lib_drv_multi = static_library(
	'drv_multi',
	files(
		'multi_wrapper/multi.c',
		'multi_wrapper/multi.h'
	),
	include_directories: [
		xrt_include,
	],
	dependencies: [aux],
	build_by_default: true,
)

lib_drv_qwerty = static_library(
	'drv_qwerty',
	files(
		'qwerty/qwerty_device.c',
		'qwerty/qwerty_device.h',
		'qwerty/qwerty_interface.h',
		'qwerty/qwerty_prober.c',
		'qwerty/qwerty_sdl.c',
	),
	include_directories: xrt_include,
	dependencies: [aux, sdl2],
	build_by_default: 'qwerty' in drivers,
)

drv_qwerty_include = include_directories('qwerty')
