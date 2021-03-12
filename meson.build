# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

project(
	'xrt',
	['c', 'cpp'],
	version: '21.0.0',
	license: 'BSL-1.0',
	meson_version: '>=0.49.0',
	default_options: [
		'c_std=c11',
		'warning_level=3',
	],
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

add_project_arguments(cc.get_supported_arguments([
	'-D_XOPEN_SOURCE=700',
	'-Wno-unused-parameter',
	'-Werror-implicit-function-declaration',
	'-Werror=incompatible-pointer-types'
]), language: 'c')

add_project_arguments(cpp.get_supported_arguments([
	'-D_XOPEN_SOURCE=700',
	'-Wno-unused-parameter',
	'-Wno-deprecated-copy', # Eigen
	'-Wno-c11-extensions' # clang OpenCV
]), language: 'cpp')


#
# Pre-setting these variables
#

build_tracking = false

v4l2_required = false
hidapi_required = false
openhmd_required = false


#
# Adding dependencies
#

# When docs are disabled, doxygen will always appear as "not found"
doxygen = find_program('doxygen', required: get_option('docs'))
build_docs = doxygen.found()

glslangValidator = find_program('glslangValidator')

pthreads = cc.find_library('pthread', required: true)
rt = cc.find_library('rt', required: true)

avcodec  = dependency('libavcodec', required: false)
egl      = dependency('egl', required: get_option('egl'))
egl      = egl.partial_dependency(includes: true)
eigen3   = dependency('eigen3')
libjpeg  = dependency('libjpeg', required: false)
libusb   = dependency('libusb-1.0', required: false)
opengl   = dependency('gl', required: get_option('opengl'))
opengles = dependency('glesv2', required: get_option('opengles'))
rs       = dependency('realsense2', required: false)
sdl2     = dependency('sdl2', required: get_option('gui'))
udev     = dependency('libudev', required: false)
libuvc   = dependency('libuvc', required: false)
vulkan   = dependency('vulkan', required: true)
zlib     = dependency('zlib', required: false)
survive  = dependency('survive', required: false)
dbus     = dependency('dbus-1', required: get_option('dbus'))
systemd  = dependency('libsystemd', required: get_option('systemd'))
gst      = dependency('gstreamer-1.0', required: false)
gst_app  = dependency('gstreamer-app-1.0', required: false)
gst_video= dependency('gstreamer-video-1.0', required: false)

gst_found = gst.found() and gst_app.found() and gst_video.found()

opencv = dependency('opencv4', required: false)
if not opencv.found()
	opencv = dependency('opencv', required: get_option('tracking'))
endif

if get_option('tracking').enabled() or get_option('tracking').auto()
	build_tracking = opencv.found()
endif

# TODO: make these behave well when not present
x11       = dependency('x11', required: get_option('xlib'))
x11_xcb   = dependency('x11-xcb', required: get_option('xlib'))
xcb       = dependency('xcb', required: get_option('xcb'))
xcb_randr = dependency('xcb-randr', required: get_option('xcb'))

wayland         = dependency('wayland-client', required: get_option('wayland'))
wayland_protos  = dependency('wayland-protocols', required: get_option('wayland'))
wayland_scanner = dependency('wayland-scanner', required: get_option('wayland'))

if wayland_scanner.found()
	wayland_scanner = find_program(
		wayland_scanner.get_pkgconfig_variable('wayland_scanner'),
		native: true,
	)
endif

build_opengl = false
if get_option('opengl').enabled() or get_option('opengl').auto()
        build_opengl = opengl.found()
endif

build_opengles = false
if get_option('opengles').enabled() or get_option('opengles').auto()
        build_opengles = opengles.found() and egl.found()
endif


build_egl = false
if get_option('egl').enabled() or get_option('egl').auto()
        build_egl = opengl.found() and egl.found()
endif

build_xlib = false
if get_option('xlib').enabled() or get_option('xlib').auto()
        build_xlib = x11.found()
endif

build_xcb = false
if get_option('xcb').enabled() or get_option('xcb').auto()
        build_xcb = xcb.found()
endif

build_xcb_xrandr_direct = build_xcb and build_xlib and xcb_randr.found() and x11_xcb.found()

# requires only vulkan
build_vk_khr_display = true

build_wayland = false
if get_option('wayland').enabled() or get_option('wayland').auto()
        build_wayland = wayland.found() and wayland_protos.found() and wayland_scanner.found()
endif

# For now required on Linux
if target_machine.system() == 'linux'
	v4l2_required = true
endif

drivers = get_option('drivers')
if 'ohmd' in drivers
	openhmd_required = true
endif
if 'psvr' in drivers
	hidapi_required = true
endif
if 'v4l2' in drivers
	v4l2_required = true
endif

if 'auto' in drivers
	drivers += ['dummy', 'hdk', 'hydra', 'ns', 'psmv', 'remote']
endif

openhmd = dependency('openhmd', required: openhmd_required)
hidapi  = dependency('hidapi-libusb', required: hidapi_required)

has_v4l2_header =  cc.has_header('linux/v4l2-common.h')

if openhmd.found() and ('auto' in drivers or 'ohmd' in drivers)
	if 'ohmd' not in drivers
		drivers += ['ohmd']
	endif
endif

if has_v4l2_header and ('auto' in drivers or 'handtracking' in drivers)
	if 'handtracking' not in drivers
		drivers += ['handtracking']
	endif
endif

if hidapi.found() and ('auto' in drivers or 'psvr' in drivers or 'hdk' in drivers)
	if 'psvr' not in drivers
		drivers += ['psvr']
	endif
endif

if zlib.found() and ('auto' in drivers or 'vive' in drivers)
	if 'vive' not in drivers
		drivers += ['vive']
	endif
endif

if rs.found() and ('auto' in drivers or 'rs' in drivers)
	if 'rs' not in drivers
		drivers += ['rs']
	endif
endif

if has_v4l2_header and ('auto' in drivers or 'v4l2' in drivers)
	if 'v4l2' not in drivers
		drivers += ['v4l2']
	endif
endif

if gst_found and ('auto' in drivers or 'vf' in drivers)
	if 'vf' not in drivers
		drivers += ['vf']
	endif
endif

if survive.found() and ('auto' in drivers and 'survive' not in drivers)
	drivers += ['survive']
endif

if not get_option('dbus').disabled() and dbus.found()
	if 'auto' in drivers and 'daydream' not in drivers
		drivers += ['daydream']
	endif

	if 'auto' in drivers and 'arduino' not in drivers
		drivers += ['arduino']
	endif
endif

if sdl2.found() and ('auto' in drivers and 'qwerty' not in drivers)
	drivers += ['qwerty']
endif

if drivers.length() == 0 or drivers == ['auto']
	error('You must enable at least one driver.')
endif

#
# Go down sub directories
#

subdir('src')

if build_docs
	subdir('doc')
endif

if not get_option('tests').disabled()
	subdir('tests')
endif

#
# Final bits
#

# This is here so that it gets emitted in the top-level build directory
manifest_devconf = configuration_data()
# https://github.com/mesonbuild/meson/issues/5940
manifest_devconf.set('runtime_path', openxr.full_path())

manifest_dev_json = configure_file(
	input: manifest_in,
	output: 'openxr_monado-dev.json',
	configuration: manifest_devconf,
)

message('Configuration done!')
message('    drivers:  ' + ', '.join(drivers))

if build_docs
	message('    docs:     yes')
else
	message('    docs:     no')
endif

if build_tracking
	message('    tracking: yes')
else
	message('    tracking: no')
endif

if get_option('steamvr_plugin')
	message('steamvr plugin: yes')
else
	message('steamvr plugin: no')
endif

if build_opengl
	message('      opengl: yes')
else
	message('      opengl: no')
endif

if build_opengles
	message('    opengles: yes')
else
	message('    opengles: no')
endif

if build_egl
	message('         egl: yes')
else
	message('         egl: no')
endif

if build_xlib
	message('        xlib: yes')
else
	message('        xlib: no')
endif

if build_xcb
	message('         xcb: yes')
else
	message('         xcb: no')
endif

if build_wayland
	message('     wayland: yes')
else
	message('     wayland: no')
endif

if not get_option('systemd').disabled() and systemd.found()
	message('     systemd: yes')
else
	message('     systemd: no')
endif

if not get_option('dbus').disabled() and dbus.found()
	message('        dbus: yes')
else
	message('        dbus: no')
endif
