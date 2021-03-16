# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0


add_executable(monado-service
	main.c
	../openxr/oxr_sdl2_hack.c
	)

target_link_libraries(monado-service PRIVATE
	aux_util
	st_prober
	ipc_server
	comp_main
	target_lists
	target_instance
	)

install(TARGETS monado-service
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)

if(XRT_HAVE_SYSTEMD)
	set(SERVICE_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/monado.in.service)
	set(SOCKET_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/monado.in.socket)
	###
	# Generate systemd unit files with absolute path to service intended for development without installing
	set(UNIT_NAME monado-dev)
	set(service_path $<TARGET_FILE:monado-service>)
	set(conflicts monado)
	set(exit_on_disconnect ON)
	set(extra_desc "in build tree")
	configure_file(${SOCKET_INPUT} ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}.socket)
	# Need this step because file(GENERATE) only evaluates generator expressions, and not what configure_file does.
	configure_file(${SERVICE_INPUT} ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}-intermediate.service)
	file(GENERATE
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}.service"
		INPUT ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}-intermediate.service)

	configure_file(${SOCKET_INPUT} ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}.socket @ONLY)

	# Make a custom target to link those in.
	add_custom_target(link-systemd-dev-units
		COMMAND systemctl --user link ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}.socket
		COMMAND systemctl --user link ${CMAKE_CURRENT_BINARY_DIR}/${UNIT_NAME}.service
		COMMAND systemctl --user daemon-reload
		VERBATIM
		COMMENT "Linking monado-dev.{socket,service} into your local systemd unit directory."
	)
endif()

if(XRT_INSTALL_SYSTEMD_UNIT_FILES)
	set(UNIT_NAME monado)
	set(conflicts monado-dev)
	set(exit_on_disconnect OFF)

	# Try to ask where to install it
	pkg_get_variable(PC_SYSTEMD_USERUNITDIR systemd systemduserunitdir)
	pkg_get_variable(PC_SYSTEMD_PREFIX systemd prefix)
	if(NOT DEFINED XRT_SYSTEMD_UNIT_INSTALL_DIR)
		# Fallback dest
		set(XRT_SYSTEMD_UNIT_INSTALL_DIR lib/systemd/user)
		if(PC_SYSTEMD_USERUNITDIR AND PC_SYSTEMD_PREFIX)
			# Strip prefix
			string(REGEX REPLACE "^${PC_SYSTEMD_PREFIX}/" "" XRT_SYSTEMD_UNIT_INSTALL_DIR "${PC_SYSTEMD_USERUNITDIR}")
		endif()
		set(XRT_SYSTEMD_UNIT_INSTALL_DIR "${XRT_SYSTEMD_UNIT_INSTALL_DIR}" CACHE STRING "The (absolute, or CMAKE_INSTALL_PREFIX-relative) path to install the systemd user unit files.")
		mark_as_advanced(XRT_SYSTEMD_UNIT_INSTALL_DIR)
	endif()
	if(XRT_SYSTEMD_UNIT_INSTALL_DIR MATCHES "^/")
		# Destination is absolute: prepend only destdir (implicitly) at install time
		set(UNIT_DIR "${XRT_SYSTEMD_UNIT_INSTALL_DIR}")
	else()
		# Destination is relative: prepend destdir (implicitly) and install prefix at install time
		set(UNIT_DIR "\${CMAKE_INSTALL_PREFIX}/${XRT_SYSTEMD_UNIT_INSTALL_DIR}")
	endif()
	configure_file(configure_and_install_units.cmake ${CMAKE_CURRENT_BINARY_DIR}/configure_and_install_units.cmake @ONLY)

	# This script will configure the units and install them at install time.
	install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/configure_and_install_units.cmake)
endif()

if(XRT_HAVE_SDL2)
	target_link_libraries(monado-service PRIVATE
		st_gui
		imgui_impl_sdl
		${SDL2_LIBRARIES}
		aux_ogl
		drv_qwerty_includes
		)
	target_include_directories(monado-service PRIVATE
		${SDL2_INCLUDE_DIRS}
		)
endif()
