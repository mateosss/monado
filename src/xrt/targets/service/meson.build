# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

service_deps = [pthreads, libjpeg]
if systemd.found() and not get_option('systemd').disabled()
	service_deps += systemd
endif

hack_deps = []
hack_src = [ '../openxr/oxr_sdl2_hack.c']
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
	hack_libs += lib_st_gui
	hack_incs += [
		st_include,
		drv_qwerty_include,
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
	dependencies: [pthreads, libjpeg] + hack_deps + service_deps,
	install: true,
)

service_install_path = join_paths(get_option('prefix'), 'bin', 'monado-service')
systemd_dir = join_paths(get_option('prefix'), get_option('libdir'), 'systemd', 'user')

service_conf_data = configuration_data()
service_conf_data.set('UNIT_NAME', 'monado')
service_conf_data.set('conflicts', 'monado-dev')
service_conf_data.set('exit_on_disconnect', 'off')
service_conf_data.set('extra_desc', '')
service_conf_data.set('service_path', service_install_path)
configure_file(
	input: 'monado.in.service',
	output: 'monado.service',
	configuration: service_conf_data,
	install_dir: systemd_dir
)

socket_conf_data = configuration_data()
socket_conf_data.set('conflicts', 'monado-dev')
socket_conf_data.set('extra_desc', '')
configure_file(
	input: 'monado.in.socket',
	output: 'monado.socket',
	configuration: socket_conf_data,
	install_dir: systemd_dir
)
