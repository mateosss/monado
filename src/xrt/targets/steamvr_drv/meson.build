# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

plugin_dir = 'steamvr-monado'

driver_monado = shared_library(
	'driver_monado',
	files(
		'main.c'
	),
	link_whole: [
		lib_target_instance_no_comp,
		lib_st_ovrd,
	],
	include_directories: [
	openvr_include,
	st_ovrd_include,
	openvr_include,
		aux_include,
		xrt_include,
	],
	link_with : driver_libs,
	dependencies : [pthreads, libjpeg],
	# build 'driver_monado.so' instead of 'libdriver_monado.so'
	name_prefix: '',
)

copy_asset = find_program('copy_assets.py')
copy_plugin = find_program('copy_plugin.py')


run_command(copy_asset, 'FILE', 'driver.vrdrivermanifest', join_paths(plugin_dir, 'driver.vrdrivermanifest'))
run_command(copy_asset, 'DIRECTORY', 'resources', join_paths(plugin_dir, 'resources'))

plugin_archdir = ''

if host_machine.system() == 'windows'
	if host_machine.cpu_family() == 'x86'
		plugin_archdir = 'win32'
	elif host_machine.cpu_family() == 'x86_64'
		plugin_archdir = 'win64'
	endif
elif host_machine.system() == 'linux'
	if host_machine.cpu_family() == 'x86'
		plugin_archdir = 'linux32'
	elif host_machine.cpu_family() == 'x86_64'
		plugin_archdir = 'linux64'
	endif
endif

custom_target(
	'plugin_copy',
	depends : driver_monado,
	input : driver_monado,
	output : 'fake_plugin',
	command : [copy_plugin, '@INPUT@', join_paths(plugin_dir, 'bin', plugin_archdir, '@PLAINNAME@')],
	build_by_default : true
)

if meson.version().version_compare('<0.56')
	build_root = meson.build_root()
else
	build_root = meson.project_build_root()
endif
install_subdir(join_paths(build_root, plugin_dir), install_dir: join_paths(get_option('prefix'), 'share'))