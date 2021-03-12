# Copyright 2020, Collabora, Ltd. SPDX-License-Identifier: BSL-1.0

tests_main = static_library(
	'tests_main',
	files(
		'tests_main.cpp'
	),
	include_directories: [
		catch2_include,
	]
)


hack_deps = []
hack_src = [ '../src/xrt/targets/openxr/oxr_sdl2_hack.c']
hack_libs = []
hack_incs = [
	glad_include,
	imgui_include,
]

if sdl2.found()
	hack_deps += sdl2
	hack_src += [
		'../src/external/imgui/imgui/cimgui_sdl.cpp',
		'../src/external/imgui/imgui/imgui_impl_sdl.cpp',
		'../src/external/imgui/imgui/imgui_impl_sdl.h',
	]
	hack_libs += lib_st_gui
	hack_incs += [
		st_include,
		drv_qwerty_include,
	]
endif


tests_input_transform = executable(
	'tests_input_transform',
	files(
		'tests_input_transform.cpp',
		hack_src,
	),
	include_directories: [
		xrt_include,
		aux_include,
		st_include,
		openxr_include,
		catch2_include,
	] + hack_incs,
	dependencies: [pthreads, driver_deps, compositor_deps, aux_ogl, aux_vk] + hack_deps,
	link_whole: [lib_target_instance_no_comp, lib_st_oxr, lib_comp, driver_libs, tests_main] + hack_libs,
)

test('tests_input_transform', tests_input_transform)
