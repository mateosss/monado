// Copyright 2019-2023, Collabora, Ltd.
// Copyright 2025, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Shader loading interface.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_render
 */

#pragma once

#include "vk/vk_helpers.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * Holds all shaders.
 */
struct render_shaders
{
	VkShaderModule blit_comp;
	VkShaderModule blit_ms_comp;
	VkShaderModule clear_comp;
	VkShaderModule layer_comp;
	VkShaderModule distortion_comp;

	VkShaderModule mesh_vert;
	VkShaderModule mesh_frag;

	VkShaderModule png_vert;
	VkShaderModule png_frag;


	/*
	 * New layer renderer.
	 */

	VkShaderModule layer_cylinder_vert;
	VkShaderModule layer_cylinder_frag;

	VkShaderModule layer_equirect2_vert;
	VkShaderModule layer_equirect2_frag;

	VkShaderModule layer_projection_vert;
	VkShaderModule layer_projection_frag;

	VkShaderModule layer_quad_vert;
	VkShaderModule layer_quad_frag;
};

/*!
 * Loads all of the shaders that the compositor uses.
 */
bool
render_shaders_load(struct render_shaders *s, struct vk_bundle *vk);

/*!
 * Unload and cleanup shaders.
 */
void
render_shaders_fini(struct render_shaders *s, struct vk_bundle *vk);


#ifdef __cplusplus
}
#endif
