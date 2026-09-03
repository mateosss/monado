# Copyright 2019-2024, Collabora, Ltd.
# Copyright 2025, NVIDIA CORPORATION.
# SPDX-License-Identifier: BSL-1.0

##
# Render library
#

add_library(
	aux_render STATIC
	render_buffer.c
	render_compute.c
	render_distortion.c
	render_gfx.c
	render_interface.h
	render_resources.c
	render_sub_alloc.c
	render_util.c
	render_png.c
	)
# The aux_vk library needs to be public to include Vulkan.
target_link_libraries(
	aux_render
	PUBLIC xrt-interfaces aux_vk comp_includes aux_shaders xrt-external-stb
	PRIVATE aux_util aux_os
	)

# Shaders library
if(XRT_MODULE_AUX_SHADERS)
	add_subdirectory(shaders)
endif()
