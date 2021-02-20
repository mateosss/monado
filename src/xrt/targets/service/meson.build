# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

hack_deps = []
hack_src = [ '../openxr/oxr_sdl2_hack.c']
hack_libs = []
hack_incs = [
	glad_include,
	imgui_include,
]

# hack_incs += drv_include
hack_incs += include_directories('../../drivers/qwerty')

if sdl2.found()
	hack_deps += sdl2
	hack_src += [
		'../../../external/imgui/imgui/cimgui_sdl.cpp',
		'../../../external/imgui/imgui/imgui_impl_sdl.cpp',
		'../../../external/imgui/imgui/imgui_impl_sdl.h',
	]
	hack_libs += lib_st_gui
	hack_incs += [
		st_include,
	]
endif

service = executable(
	'monado-service',
	files(
		'main.c',
		hack_src
	),
	link_whole: [
		lib_st_prober,
		lib_ipc_server,
		lib_comp,
		lib_target_lists,
		lib_target_instance,
		lib_aux_util,
	] + driver_libs + hack_libs,
	include_directories: [
		aux_include,
		common_include,
		xrt_include,
	] + hack_incs,
	dependencies: [pthreads, libjpeg] + hack_deps,
	install: true,
)
