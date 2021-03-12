# Copyright 2018-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.10.2)
project(XRT VERSION 21.0.0)

# CMake 3.11 introduced CMP0072 - Prefer GLVND
if(POLICY CMP0072)
	cmake_policy(SET CMP0072 NEW)
endif()

option(XRT_OPENXR_INSTALL_ABSOLUTE_RUNTIME_PATH "Use the absolute path to the runtime in the installed manifest, rather than a bare filename." ON)
option(XRT_OPENXR_INSTALL_ACTIVE_RUNTIME "Make Monado the default OpenXR runtime on install" ON)

###
# Dependencies
###
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CMakeDependentOption)
include(SPIR-V)
include(GNUInstallDirs)
include(GetGitRevisionDescription)
git_describe(GIT_DESC "--always")
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.9)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT HAS_IPO)
endif()

# Redundant mention of version is required because module defaults to looking for 2.91-compatible,
# which the config file for a 3.x says it's not compatible with.
find_package(Eigen3 3 REQUIRED)
find_package(Vulkan)
find_package(EGL)
find_package(HIDAPI)
find_package(OpenHMD)
find_package(OpenCV COMPONENTS core calib3d highgui imgproc imgcodecs features2d video CONFIG)
find_package(Libusb1)
find_package(JPEG)
find_package(realsense2 CONFIG)
find_package(SDL2 CONFIG)
find_package(ZLIB)
find_package(cJSON)
find_package(Systemd)
find_package(OpenGLES COMPONENTS V3)

# Android SDK doesn't look for 3.8 and 3.9, which is what new distros ship with.
set(Python_ADDITIONAL_VERSIONS 3.8 3.9)
if(NOT CMAKE_VERSION VERSION_LESS 3.12)
	find_package(Python3 REQUIRED Interpreter)
	set(PYTHON_EXECUTABLE Python3::Interpreter)
else()
	find_program(PYTHON_EXECUTABLE python3)
	if(PYTHON_EXECUTABLE MATCHES "WindowsApps")
		# If you hit this error, you will have to install Python 3 or try harder to tell CMake where it is.
		message(FATAL_ERROR "Found WindowsApps alias for Python. Make sure Python3 is installed, then choose 'Manage App Execution Aliases' in Start and disable the aliases for Python.")
	endif()
endif()

add_library(xrt-pthreads INTERFACE)
if(WIN32)
	find_package(pthreads_windows REQUIRED)
	target_link_libraries(xrt-pthreads INTERFACE PThreads4W::PThreads4W_CXXEXC)
else()
	set(CMAKE_THREAD_PREFER_PTHREAD ON)
	find_package(Threads)
	target_link_libraries(xrt-pthreads INTERFACE Threads::Threads)
endif()

if(NOT ANDROID)
	# @TODO Turn into a find_package LIBUVC file.
	pkg_check_modules(LIBUVC libuvc)

	# @TODO Turn into a find_package FFMPEG file.
	pkg_check_modules(FFMPEG libavcodec)
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(XRT_HAVE_LINUX YES)
	# Compositor backend
	find_package(X11)
	find_package(PkgConfig)

	find_package(udev REQUIRED)
	set(XRT_HAVE_V4L2 TRUE)
	if(PKGCONFIG_FOUND)
		pkg_check_modules(XRANDR xrandr)

		pkg_check_modules(XCB xcb xcb-randr x11-xcb)

		pkg_search_module(WAYLAND wayland-client)
		pkg_search_module(WAYLAND_SCANNER wayland-scanner)
		pkg_search_module(WAYLAND_PROTOCOLS wayland-protocols)
	endif()
	find_package(OpenGL COMPONENTS GLX)
	pkg_search_module(DBUS dbus-1)

	pkg_check_modules(GST
		gstreamer-1.0
		gstreamer-app-1.0
		gstreamer-video-1.0
	)

	pkg_check_modules(SURVIVE IMPORTED_TARGET survive)
else()
	find_package(OpenGL)
endif()

find_library(RT_LIBRARY rt)
if(ANDROID)
	find_library(ANDROID_LIBRARY android)
	find_library(ANDROID_LOG_LIBRARY log)
endif()

# This one is named differently because that's what CTest uses
option(BUILD_TESTING "Enable building of the test suite?" ON)

option(XRT_FEATURE_COLOR_LOG "Enable logging in color on supported platforms" ON)
option(XRT_FEATURE_TRACING "Enable debug tracing on supported platforms" OFF)

cmake_dependent_option(CMAKE_INTERPROCEDURAL_OPTIMIZATION "Enable inter-procedural (link-time) optimization" OFF "HAS_IPO" OFF)
cmake_dependent_option(XRT_HAVE_WAYLAND "Enable Wayland support" ON "WAYLAND_FOUND AND WAYLAND_SCANNER_FOUND AND WAYLAND_PROTOCOLS_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_XLIB "Enable xlib support" ON "X11_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_XRANDR "Enable xlib-xrandr support" ON "XRANDR_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_XCB "Enable xcb support" ON "XCB_FOUND" OFF)

cmake_dependent_option(XRT_HAVE_VULKAN "Enable Vulkan Graphics API support (also needed for compositor)" ON "VULKAN_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_OPENGL "Enable OpenGL Graphics API support" ON "OPENGL_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_OPENGLES "Enable OpenGL-ES Graphics API support" ON "OpenGLES_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_EGL "Enable OpenGL on EGL Graphics API support" ON "EGL_FOUND; XRT_HAVE_OPENGL OR XRT_HAVE_OPENGLES" OFF)
cmake_dependent_option(XRT_HAVE_DBUS "Enable dbus support (for BLE support)" ON "DBUS_FOUND" OFF)
cmake_dependent_option(XRT_FEATURE_COMPOSITOR_MAIN "Build main compositor host functionality" ON "XRT_HAVE_VULKAN; XRT_HAVE_WAYLAND OR XRT_HAVE_XCB OR ANDROID OR WIN32" OFF)
cmake_dependent_option(XRT_FEATURE_OPENXR "Build OpenXR runtime target" ON "XRT_FEATURE_COMPOSITOR_MAIN" OFF)
cmake_dependent_option(XRT_FEATURE_SERVICE "Enable separate service module for OpenXR runtime" ON "NOT WIN32 AND XRT_FEATURE_OPENXR" OFF)
cmake_dependent_option(XRT_HAVE_SYSTEMD "Enable systemd support (for socket activation of service)" ON "Systemd_FOUND AND XRT_FEATURE_SERVICE" OFF)
cmake_dependent_option(XRT_INSTALL_SYSTEMD_UNIT_FILES "Install user unit files for systemd socket activation on installation" ON "XRT_HAVE_SYSTEMD" OFF)
cmake_dependent_option(XRT_INSTALL_ABSOLUTE_SYSTEMD_UNIT_FILES "Use an absolute path to monado-system in installed user unit files for systemd socket activation" ON "XRT_INSTALL_SYSTEMD_UNIT_FILES" OFF)
cmake_dependent_option(XRT_FEATURE_STEAMVR_PLUGIN "Build SteamVR plugin" ON "NOT ANDROID" OFF)

if(NOT DEFINED XRT_FEATURE_OPENXR_LAYER_DEPTH)
	set(XRT_FEATURE_OPENXR_LAYER_DEPTH ON)
endif()
if(NOT DEFINED XRT_FEATURE_OPENXR_LAYER_CUBE)
	set(XRT_FEATURE_OPENXR_LAYER_CUBE OFF)
endif()
if(NOT DEFINED XRT_FEATURE_OPENXR_LAYER_CYLINDER)
	set(XRT_FEATURE_OPENXR_LAYER_CYLINDER ON)
endif()
if(NOT DEFINED XRT_FEATURE_OPENXR_LAYER_EQUIRECT2)
	set(XRT_FEATURE_OPENXR_LAYER_EQUIRECT2 ON)
endif()
if(NOT DEFINED XRT_FEATURE_OPENXR_LAYER_EQUIRECT1)
	set(XRT_FEATURE_OPENXR_LAYER_EQUIRECT1 ON)
endif()

# Most users won't touch these.
mark_as_advanced(XRT_FEATURE_COMPOSITOR_MAIN XRT_FEATURE_OPENXR)

# ILLIXR
set(ILLIXR_PATH "" CACHE PATH "Path to ILLIXR headers")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(XRT_HAVE_LIBUDEV ON)
	set(XRT_HAVE_INTERNAL_HID ON)
else()
	cmake_dependent_option(XRT_HAVE_LIBUDEV "Enable libudev (used for device probing on Linux)" ON "UDEV_FOUND" OFF)
endif()
cmake_dependent_option(XRT_HAVE_LIBUSB "Enable libusb (used for most drivers)" ON "LIBUSB1_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_JPEG "Enable jpeg code (used for some video drivers)" ON "JPEG_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_OPENCV "Enable OpenCV backend" ON "OpenCV_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_LIBUVC "Enable libuvc video driver" ON "LIBUVC_FOUND AND XRT_HAVE_LIBUSB" OFF)
cmake_dependent_option(XRT_HAVE_FFMPEG "Enable ffmpeg testing video driver" ON "FFMPEG_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_SDL2 "Enable use of SDL2" ON "SDL2_FOUND AND XRT_HAVE_OPENGL" OFF)
cmake_dependent_option(XRT_HAVE_SYSTEM_CJSON "Enable cJSON from system, instead of bundled source" ON "CJSON_FOUND" OFF)
cmake_dependent_option(XRT_HAVE_GST "Enable gstreamer" ON "GST_FOUND" OFF)


cmake_dependent_option(XRT_BUILD_DRIVER_PSVR "Enable PSVR HMD driver" ON "HIDAPI_FOUND" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_RS "Enable RealSense device driver" ON "realsense2_FOUND" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_VIVE "Enable driver for HTC Vive, Vive Pro, Valve Index, and their controllers" ON "ZLIB_FOUND AND XRT_HAVE_LINUX" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_OHMD "Enable OpenHMD driver" ON "OPENHMD_FOUND" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_HANDTRACKING "Enable Camera Hand Tracking driver" ON "XRT_HAVE_V4L2" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_DAYDREAM "Enable the Google Daydream View controller driver (BLE, via D-Bus)" ON "XRT_HAVE_DBUS" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_ARDUINO "Enable Arduino input device with BLE via via D-Bus" ON "XRT_HAVE_DBUS" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_ILLIXR "Enable ILLIXR driver" ON "ILLIXR_PATH" OFF)

option(XRT_BUILD_DRIVER_DUMMY "Enable dummy driver" ON)
cmake_dependent_option(XRT_BUILD_DRIVER_REMOTE "Enable remote debugging driver" ON "XRT_HAVE_LINUX OR ANDROID" OFF)

# These all use the Monado internal hid wrapper.
cmake_dependent_option(XRT_BUILD_DRIVER_HDK "Enable HDK driver" ON "XRT_HAVE_INTERNAL_HID" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_PSMV "Enable Playstation Move driver" ON "XRT_HAVE_INTERNAL_HID" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_HYDRA "Enable Hydra driver" ON "XRT_HAVE_INTERNAL_HID" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_NS "Enable North Star driver" ON "XRT_HAVE_INTERNAL_HID" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_VF "Build video frame driver (for video file support, uses gstreamer)" ON "XRT_HAVE_GST" OFF)

# This one defaults to off, even if we find the deps.
cmake_dependent_option(XRT_BUILD_DRIVER_SURVIVE "Enable libsurvive driver" ON "SURVIVE_FOUND" OFF)

cmake_dependent_option(XRT_BUILD_DRIVER_ANDROID "Enable Android sensors driver" ON "ANDROID" OFF)
cmake_dependent_option(XRT_BUILD_DRIVER_QWERTY "Enable Qwerty driver" ON "XRT_HAVE_SDL2" OFF)

# You can set this from a superproject to add a driver
# All drivers must be listed in here to be included in the generated header!
list(APPEND AVAILABLE_DRIVERS
	"ANDROID"
	"ARDUINO"
	"DAYDREAM"
	"DUMMY"
	"HANDTRACKING"
	"HDK"
	"HYDRA"
	"ILLIXR"
	"NS"
	"OHMD"
	"PSMV"
	"PSVR"
	"RS"
	"REMOTE"
	"SURVIVE"
	"V4L2"
	"VF"
	"VIVE"
	"QWERTY"
	)


# Package name needs to be known by the native code itself.
# Can be overridden from outside/command line
if(ANDROID AND NOT XRT_ANDROID_PACKAGE)
	if(XRT_FEATURE_SERVICE)
		set(XRT_ANDROID_PACKAGE "org.freedesktop.monado.openxr_runtime.out_of_process")
	else()
		set(XRT_ANDROID_PACKAGE "org.freedesktop.monado.openxr_runtime.in_process")
	endif()
endif()

###
# Flags
###

if(XRT_HAVE_XLIB AND NOT XRT_HAVE_XRANDR)
	message(WARNING "XRT_HAVE_XLIB requires XRT_HAVE_XRANDR but XRT_HAVE_XRANDR is disabled")
endif()

if(XRT_HAVE_OPENGLES AND NOT XRT_HAVE_EGL)
	message(FATAL_ERROR "XRT_HAVE_OPENGLES requires XRT_HAVE_EGL but XRT_HAVE_EGL is disabled")
endif()

if(XRT_HAVE_SDL2)
	if(NOT DEFINED SDL2_LIBRARIES)
		if(TARGET SDL2::SDL2-static)
			set(SDL2_LIBRARIES SDL2::SDL2-static)
		elseif(TARGET SDL2::SDL2)
			set(SDL2_LIBRARIES SDL2::SDL2)
		endif()
	endif()
endif()

# Vulkan flags for the shared Vulkan code.
if(XRT_HAVE_XCB)
	set(VK_USE_PLATFORM_XCB_KHR TRUE)
endif()
if(XRT_HAVE_XCB AND XRT_HAVE_XLIB AND XRT_HAVE_XRANDR)
	set(VK_USE_PLATFORM_XLIB_XRANDR_EXT TRUE)
endif()
if(XRT_HAVE_WAYLAND)
	set(VK_USE_PLATFORM_WAYLAND_KHR TRUE)
endif()
if(ANDROID)
	set(VK_USE_PLATFORM_ANDROID_KHR TRUE)
endif()
if(WIN32)
	set(VK_USE_PLATFORM_WIN32_KHR TRUE)
endif()
if (XRT_HAVE_VULKAN AND NOT ANDROID)
	set(VK_USE_PLATFORM_DISPLAY_KHR TRUE)
endif()

if(NOT MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -Wno-unused-parameter")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror-implicit-function-declaration -Werror=incompatible-pointer-types")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=int-conversion")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wno-unused-parameter")

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
endif()

# Default to PIC code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Describe IPO setting
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
	message(STATUS "Inter-procedural optimization enabled")
endif()

###
# Decend into madness.
###

add_subdirectory(src)
add_subdirectory(doc)

if(BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()

message(STATUS "#####----- Config -----#####")
message(STATUS "#    GIT_DESC:     ${GIT_DESC}")
message(STATUS "#")
message(STATUS "#    WAYLAND:      ${XRT_HAVE_WAYLAND}")
message(STATUS "#    XLIB:         ${XRT_HAVE_XLIB}")
message(STATUS "#    XRANDR:       ${XRT_HAVE_XRANDR}")
message(STATUS "#    XCB:          ${XRT_HAVE_XCB}")
message(STATUS "#    VULKAN:       ${XRT_HAVE_VULKAN}")
message(STATUS "#    OPENGL:       ${XRT_HAVE_OPENGL}")
message(STATUS "#    OPENGLES:     ${XRT_HAVE_OPENGLES}")
message(STATUS "#    EGL:          ${XRT_HAVE_EGL}")
message(STATUS "#    DBUS:         ${XRT_HAVE_DBUS}")
message(STATUS "#    VF:           ${XRT_HAVE_VF}")
message(STATUS "#    SYSTEMD:      ${XRT_HAVE_SYSTEMD}")
message(STATUS "#    LIBUSB:       ${XRT_HAVE_LIBUSB}")
message(STATUS "#    JPEG:         ${XRT_HAVE_JPEG}")
message(STATUS "#    OPENCV:       ${XRT_HAVE_OPENCV}")
message(STATUS "#    LIBUVC:       ${XRT_HAVE_LIBUVC}")
message(STATUS "#    FFMPEG:       ${XRT_HAVE_FFMPEG}")
message(STATUS "#    SDL2:         ${XRT_HAVE_SDL2}")
message(STATUS "#    SYSTEM_CJSON: ${XRT_HAVE_SYSTEM_CJSON}")
message(STATUS "#")
message(STATUS "#    FEATURE_COMPOSITOR_MAIN:              ${XRT_FEATURE_COMPOSITOR_MAIN}")
message(STATUS "#    FEATURE_SERVICE:                      ${XRT_FEATURE_SERVICE}")
message(STATUS "#    FEATURE_OPENXR:                       ${XRT_FEATURE_OPENXR}")
message(STATUS "#    FEATURE_OPENXR_LAYER_DEPTH:           ${XRT_FEATURE_OPENXR_LAYER_DEPTH}")
message(STATUS "#    FEATURE_OPENXR_LAYER_CUBE:            ${XRT_FEATURE_OPENXR_LAYER_CUBE}")
message(STATUS "#    FEATURE_OPENXR_LAYER_CYLINDER:        ${XRT_FEATURE_OPENXR_LAYER_CYLINDER}")
message(STATUS "#    FEATURE_OPENXR_LAYER_EQUIRECT2:       ${XRT_FEATURE_OPENXR_LAYER_EQUIRECT2}")
message(STATUS "#    FEATURE_OPENXR_LAYER_EQUIRECT1:       ${XRT_FEATURE_OPENXR_LAYER_EQUIRECT1}")
message(STATUS "#    FEATURE_STEAMVR_PLUGIN:               ${XRT_FEATURE_STEAMVR_PLUGIN}")
message(STATUS "#    FEATURE_COLOR_LOG:                    ${XRT_FEATURE_COLOR_LOG}")
message(STATUS "#    FEATURE_TRACING:                      ${XRT_FEATURE_TRACING}")
message(STATUS "#")
message(STATUS "#    DRIVER_ANDROID:      ${XRT_BUILD_DRIVER_ANDROID}")
message(STATUS "#    DRIVER_ARDUINO:      ${XRT_BUILD_DRIVER_ARDUINO}")
message(STATUS "#    DRIVER_DAYDREAM:     ${XRT_BUILD_DRIVER_DAYDREAM}")
message(STATUS "#    DRIVER_DUMMY:        ${XRT_BUILD_DRIVER_DUMMY}")
message(STATUS "#    DRIVER_HANDTRACKING: ${XRT_BUILD_DRIVER_HANDTRACKING}")
message(STATUS "#    DRIVER_HDK:          ${XRT_BUILD_DRIVER_HDK}")
message(STATUS "#    DRIVER_HYDRA:        ${XRT_BUILD_DRIVER_HYDRA}")
message(STATUS "#    DRIVER_ILLIXR:       ${XRT_BUILD_DRIVER_ILLIXR}")
message(STATUS "#    DRIVER_NS:           ${XRT_BUILD_DRIVER_NS}")
message(STATUS "#    DRIVER_OHMD:         ${XRT_BUILD_DRIVER_OHMD}")
message(STATUS "#    DRIVER_PSMV:         ${XRT_BUILD_DRIVER_PSMV}")
message(STATUS "#    DRIVER_PSVR:         ${XRT_BUILD_DRIVER_PSVR}")
message(STATUS "#    DRIVER_RS:           ${XRT_BUILD_DRIVER_RS}")
message(STATUS "#    DRIVER_REMOTE:       ${XRT_BUILD_DRIVER_REMOTE}")
message(STATUS "#    DRIVER_SURVIVE:      ${XRT_BUILD_DRIVER_SURVIVE}")
message(STATUS "#    DRIVER_VF:           ${XRT_BUILD_DRIVER_VF}")
message(STATUS "#    DRIVER_VIVE:         ${XRT_BUILD_DRIVER_VIVE}")
message(STATUS "#    DRIVER_QWERTY:       ${XRT_BUILD_DRIVER_QWERTY}")
message(STATUS "#####----- Config -----#####")

if(XRT_FEATURE_SERVICE AND NOT XRT_FEATURE_OPENXR)
	message(FATAL_ERROR "XRT_FEATURE_SERVICE requires XRT_FEATURE_OPENXR to be enabled")
endif()
