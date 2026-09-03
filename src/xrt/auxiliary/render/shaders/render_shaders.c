// Copyright 2019-2023, Collabora, Ltd.
// Copyright 2025, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Shader loading code.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_render
 */

#include "vk/vk_mini_helpers.h"

#include "render_shaders_interface.h"


/*
 *
 * Shader headers.
 *
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif

#include "blit.comp.h"
#include "blit_ms.comp.h"
#include "clear.comp.h"
#include "layer.comp.h"
#include "distortion.comp.h"
#include "layer_cylinder.frag.h"
#include "layer_cylinder.vert.h"
#include "layer_equirect2.frag.h"
#include "layer_equirect2.vert.h"
#include "layer_projection.vert.h"
#include "layer_projection.frag.h"
#include "layer_quad.vert.h"
#include "layer_quad.frag.h"
#include "mesh.frag.h"
#include "mesh.vert.h"
#include "png.vert.h"
#include "png.frag.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


/*
 *
 * Helpers
 *
 */

#define LOAD(SHADER)                                                                                                   \
	do {                                                                                                           \
		const uint32_t *code = SHADER;                                                                         \
		size_t size = sizeof(SHADER);                                                                          \
		VkResult ret = shader_load(vk,          /* vk_bundle */                                                \
		                           code,        /* code      */                                                \
		                           size,        /* size      */                                                \
		                           &s->SHADER); /* out       */                                                \
		if (ret != VK_SUCCESS) {                                                                               \
			VK_ERROR(vk, "Failed to load shader '" #SHADER "'");                                           \
			render_shaders_fini(s, vk);                                                                    \
			return false;                                                                                  \
		}                                                                                                      \
		VK_NAME_SHADER_MODULE(vk, s->SHADER, #SHADER);                                                         \
	} while (false)


/*
 *
 * Functions.
 *
 */

XRT_CHECK_RESULT static VkResult
shader_load(struct vk_bundle *vk, const uint32_t *code, size_t size, VkShaderModule *out_module)
{
	VkResult ret;

	VkShaderModuleCreateInfo info = {
	    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	    .codeSize = size,
	    .pCode = code,
	};

	VkShaderModule module;
	ret = vk->vkCreateShaderModule( //
	    vk->device,                 //
	    &info,                      //
	    NULL,                       //
	    &module);                   //
	VK_CHK_AND_RET(ret, "vkCreateShaderModule");

	*out_module = module;

	return VK_SUCCESS;
}

bool
render_shaders_load(struct render_shaders *s, struct vk_bundle *vk)
{
	LOAD(blit_comp);
	LOAD(blit_ms_comp);

	LOAD(clear_comp);

	LOAD(layer_comp);

	LOAD(distortion_comp);

	LOAD(mesh_vert);
	LOAD(mesh_frag);

	LOAD(png_vert);
	LOAD(png_frag);

	LOAD(layer_cylinder_frag);
	LOAD(layer_cylinder_vert);
	LOAD(layer_equirect2_frag);
	LOAD(layer_equirect2_vert);
	LOAD(layer_projection_vert);
	LOAD(layer_projection_frag);
	LOAD(layer_quad_vert);
	LOAD(layer_quad_frag);

	VK_DEBUG(vk, "Shaders loaded!");

	return true;
}

void
render_shaders_fini(struct render_shaders *s, struct vk_bundle *vk)
{
	D(ShaderModule, s->blit_comp);
	D(ShaderModule, s->blit_ms_comp);
	D(ShaderModule, s->clear_comp);
	D(ShaderModule, s->distortion_comp);
	D(ShaderModule, s->layer_comp);
	D(ShaderModule, s->mesh_vert);
	D(ShaderModule, s->mesh_frag);

	D(ShaderModule, s->png_vert);
	D(ShaderModule, s->png_frag);

	D(ShaderModule, s->layer_cylinder_frag);
	D(ShaderModule, s->layer_cylinder_vert);
	D(ShaderModule, s->layer_equirect2_frag);
	D(ShaderModule, s->layer_equirect2_vert);
	D(ShaderModule, s->layer_projection_vert);
	D(ShaderModule, s->layer_projection_frag);
	D(ShaderModule, s->layer_quad_vert);
	D(ShaderModule, s->layer_quad_frag);

	VK_DEBUG(vk, "Shaders destroyed!");
}
