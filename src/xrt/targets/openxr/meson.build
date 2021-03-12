# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

######
# Create a loadable OpenXR driver.

# runtime_bare_suffix = 'xrt'
# runtime_prefix = runtime_bare_prefix + '_'
runtime_prefix = ''

runtime_bare_suffix = 'monado'
runtime_suffix = '_' + runtime_bare_suffix

runtime_target = '@0@openxr@1@'.format(runtime_prefix, runtime_suffix)

# OpenXR 1.0
xr_api_major = 1

hack_deps = []
hack_src = [ 'oxr_sdl2_hack.c']
hack_libs = []
hack_incs = [
	glad_include,
	imgui_include,
]

if sdl2.found()
	hack_deps += sdl2
	hack_src += [
		'../../../external/imgui/imgui/cimgui_sdl.cpp',
		'../../../external/imgui/imgui/imgui_impl_sdl.cpp',
		'../../../external/imgui/imgui/imgui_impl_sdl.h',
	]
	hack_libs += [lib_st_gui, lib_drv_qwerty]
	hack_incs += [
		st_include,
		drv_qwerty_include,
	]
endif

openxr_deps = [
		libusb,
		libuvc,
		pthreads,
		xrt_config_drivers,
		xrt_config_have,
		udev,
		vulkan,
	]

if build_opengl
	openxr_deps += [opengl]
endif

if build_opengl and build_xlib
	openxr_deps += [x11]
endif

if build_xcb
	openxr_deps += [xcb]
endif

if build_xcb_xrandr_direct
	openxr_deps += [xcb_randr]
endif

libs = [
	lib_aux_vk,
	lib_aux_os,
	lib_aux_ogl,
	lib_aux_util,
	lib_aux_math,
	lib_comp,
	lib_st_oxr
] + hack_libs

if get_option('service')
	libs += lib_ipc_client
else
	libs += [
		lib_target_instance,
		driver_libs
	]
endif

if 'remote' in drivers
	libs += lib_drv_remote
endif

mapfile = 'libopenxr.version'
version_script_flag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
openxr = library(
	runtime_target,
	files(
		'target.c',
		hack_src,
	),
	link_whole: libs,
	include_directories: [
		openxr_include,
		st_include, # Sigh debian meson requires this.
		aux_include,
		common_include,
		drv_include,
		xrt_include,
	] + hack_incs,
	dependencies: openxr_deps + driver_deps + hack_deps,
	install: true,
	link_args: version_script_flag,
	link_depends: mapfile
)

manifest_in = files('openxr_monado.in.json')

manifest_conf = configuration_data()
# https://github.com/mesonbuild/meson/issues/5941
sopath = join_paths(get_option('prefix'),
	get_option('libdir'), 'libopenxr_monado.so')
manifest_conf.set('runtime_path', sopath)

manifest_path = join_paths(
	get_option('prefix'), get_option('datadir'), 'openxr', xr_api_major.to_string())

manifest_json = configure_file(
	input: manifest_in,
	output: 'openxr_monado.json',
	configuration: manifest_conf,
	install_dir: manifest_path,
)

if get_option('install-active-runtime')
	meson.add_install_script('install_active_runtime.sh', get_option('sysconfdir'),
		join_paths(manifest_path, runtime_target + '.json'), xr_api_major.to_string())
endif
