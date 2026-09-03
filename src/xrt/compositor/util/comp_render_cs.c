// Copyright 2019-2024, Collabora, Ltd.
// Copyright 2025-2026, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Compositor (compute shader) rendering code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @author Fernando Velazquez Innella <finnella@magicleap.com>
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup comp_util
 */

#include "util/comp_layer_accum.h"
#include "xrt/xrt_compositor.h"

#include "os/os_time.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"

#include "util/u_misc.h"
#include "util/u_trace_marker.h"

#include "vk/vk_helpers.h"

#include "render/render_interface.h"

#include "layer_defines.inc.glsl"

#include "util/comp_render.h"
#include "util/comp_render_helpers.h"
#include "util/comp_base.h"


/*
 *
 * Helpers
 *
 */

static inline const struct comp_swapchain_image *
get_layer_image(const struct comp_layer *layer, uint32_t swapchain_index, uint32_t image_index)
{

	const struct comp_swapchain *sc = (struct comp_swapchain *)(comp_layer_get_swapchain(layer, swapchain_index));
	return &sc->images[image_index];
}

static inline const struct comp_swapchain_image *
get_layer_depth_image(const struct comp_layer *layer, uint32_t swapchain_index, uint32_t image_index)
{

	const struct comp_swapchain *sc =
	    (struct comp_swapchain *)(comp_layer_get_depth_swapchain(layer, swapchain_index));
	return &sc->images[image_index];
}


static inline uint32_t
xrt_layer_to_cs_layer_type(const struct xrt_layer_data *data)
{
	switch (data->type) {
	case XRT_LAYER_QUAD: return LAYER_COMP_TYPE_QUAD;
	case XRT_LAYER_CYLINDER: return LAYER_COMP_TYPE_CYLINDER;
	case XRT_LAYER_EQUIRECT2: return LAYER_COMP_TYPE_EQUIRECT2;
	case XRT_LAYER_PROJECTION:
	case XRT_LAYER_PROJECTION_DEPTH:
		if ((data->flags & XRT_LAYER_SPLIT_QUAD_VIEW_CONTEXT) != 0) {
			return LAYER_COMP_TYPE_CONTEXT;
		} else if ((data->flags & XRT_LAYER_SPLIT_QUAD_VIEW_INSET) != 0) {
			return LAYER_COMP_TYPE_INSET;
		} else {
			return LAYER_COMP_TYPE_PROJECTION;
		}
	default: U_LOG_E("Invalid layer type! %u", data->type); return LAYER_COMP_TYPE_NOOP;
	}
}


/*
 *
 * Compute layer data builders.
 *
 */

/// Data setup for a cylinder layer
static inline void
do_cs_cylinder_layer(const struct comp_layer *layer,
                     const struct xrt_matrix_4x4 *eye_view_mat,
                     const struct xrt_matrix_4x4 *world_view_mat,
                     uint32_t view_index,
                     uint32_t cur_layer,
                     uint32_t cur_image,
                     VkSampler clamp_to_edge,
                     VkSampler clamp_to_border_black,
                     VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE],
                     VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE],
                     struct render_compute_layer_ubo_data *ubo_data,
                     uint32_t *out_cur_image)
{
	const struct xrt_layer_data *layer_data = &layer->data;
	const struct xrt_layer_cylinder_data *c = &layer_data->cylinder;
	const uint32_t array_index = c->sub.array_index;
	const struct comp_swapchain_image *image = get_layer_image(layer, 0, c->sub.image_index);

	// Image to use.
	src_samplers[cur_image] = clamp_to_edge;
	src_image_views[cur_image] = get_image_view(image, layer_data->flags, array_index);

	// Used for Subimage and OpenGL flip.
	set_post_transform_rect(                           //
	    layer_data,                                    // data
	    &c->sub.norm_rect,                             // src_norm_rect
	    false,                                         // invert_flip
	    &ubo_data->layers[cur_layer].post_transforms); // out_norm_rect

	ubo_data->layers[cur_layer].cylinder_data.central_angle = c->central_angle;
	ubo_data->layers[cur_layer].cylinder_data.aspect_ratio = c->aspect_ratio;

	struct xrt_vec3 scale = {1.f, 1.f, 1.f};

	struct xrt_matrix_4x4 model;
	math_matrix_4x4_model(&c->pose, &scale, &model);

	struct xrt_matrix_4x4 model_inv;
	math_matrix_4x4_inverse(&model, &model_inv);

	const struct xrt_matrix_4x4 *v = is_layer_view_space(layer_data) ? eye_view_mat : world_view_mat;

	struct xrt_matrix_4x4 v_inv;
	math_matrix_4x4_inverse(v, &v_inv);

	math_matrix_4x4_multiply(&model_inv, &v_inv, &ubo_data->layers[cur_layer].mv_inverse);

	// Simplifies the shader.
	if (c->radius >= INFINITY) {
		ubo_data->layers[cur_layer].cylinder_data.radius = 0.f;
	} else {
		ubo_data->layers[cur_layer].cylinder_data.radius = c->radius;
	}

	ubo_data->layers[cur_layer].cylinder_data.central_angle = c->central_angle;
	ubo_data->layers[cur_layer].cylinder_data.aspect_ratio = c->aspect_ratio;

	ubo_data->layers[cur_layer].image_info.color_image_index = cur_image;
	cur_image++;

	*out_cur_image = cur_image;
}

/// Data setup for an "equirect2" layer
static inline void
do_cs_equirect2_layer(const struct comp_layer *layer,
                      const struct xrt_matrix_4x4 *eye_view_mat,
                      const struct xrt_matrix_4x4 *world_view_mat,
                      uint32_t view_index,
                      uint32_t cur_layer,
                      uint32_t cur_image,
                      VkSampler clamp_to_edge,
                      VkSampler clamp_to_border_black,
                      VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE],
                      VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE],
                      struct render_compute_layer_ubo_data *ubo_data,
                      uint32_t *out_cur_image)
{
	const struct xrt_layer_data *layer_data = &layer->data;
	const struct xrt_layer_equirect2_data *eq2 = &layer_data->equirect2;
	const uint32_t array_index = eq2->sub.array_index;
	const struct comp_swapchain_image *image = get_layer_image(layer, 0, eq2->sub.image_index);

	// Image to use.
	src_samplers[cur_image] = clamp_to_edge;
	src_image_views[cur_image] = get_image_view(image, layer_data->flags, array_index);

	// Used for Subimage and OpenGL flip.
	set_post_transform_rect(                           //
	    layer_data,                                    // data
	    &eq2->sub.norm_rect,                           // src_norm_rect
	    false,                                         // invert_flip
	    &ubo_data->layers[cur_layer].post_transforms); // out_norm_rect

	struct xrt_vec3 scale = {1.f, 1.f, 1.f};

	struct xrt_matrix_4x4 model;
	math_matrix_4x4_model(&eq2->pose, &scale, &model);

	struct xrt_matrix_4x4 model_inv;
	math_matrix_4x4_inverse(&model, &model_inv);

	const struct xrt_matrix_4x4 *v = is_layer_view_space(layer_data) ? eye_view_mat : world_view_mat;

	struct xrt_matrix_4x4 v_inv;
	math_matrix_4x4_inverse(v, &v_inv);

	math_matrix_4x4_multiply(&model_inv, &v_inv, &ubo_data->layers[cur_layer].mv_inverse);

	// Simplifies the shader.
	if (eq2->radius >= INFINITY) {
		ubo_data->layers[cur_layer].eq2_data.radius = 0.f;
	} else {
		ubo_data->layers[cur_layer].eq2_data.radius = eq2->radius;
	}

	ubo_data->layers[cur_layer].eq2_data.central_horizontal_angle = eq2->central_horizontal_angle;
	ubo_data->layers[cur_layer].eq2_data.upper_vertical_angle = eq2->upper_vertical_angle;
	ubo_data->layers[cur_layer].eq2_data.lower_vertical_angle = eq2->lower_vertical_angle;

	ubo_data->layers[cur_layer].image_info.color_image_index = cur_image;
	cur_image++;

	*out_cur_image = cur_image;
}

/// Data setup for a projection layer
static inline void
do_cs_projection_layer(const struct comp_layer *layer,
                       const struct xrt_pose *world_pose_scanout_begin,
                       uint32_t view_index,
                       uint32_t cur_layer,
                       uint32_t cur_image,
                       VkSampler clamp_to_edge,
                       VkSampler clamp_to_border_black,
                       VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE],
                       VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE],
                       struct render_compute_layer_ubo_data *ubo_data,
                       bool do_timewarp,
                       uint32_t *out_cur_image)
{
	const struct xrt_layer_data *layer_data = &layer->data;
	const struct xrt_layer_projection_view_data *vd = NULL;
	const struct xrt_layer_depth_data *dvd = NULL;

	if (layer_data->type == XRT_LAYER_PROJECTION) {
		view_index_to_projection_data(view_index, layer_data, &vd);
	} else {
		view_index_to_depth_data(view_index, layer_data, &vd, &dvd);
	}

	uint32_t sc_array_index = is_view_index_right(view_index) ? 1 : 0;
	uint32_t array_index = vd->sub.array_index;
	const struct comp_swapchain_image *image = get_layer_image(layer, sc_array_index, vd->sub.image_index);

	// Color
	src_samplers[cur_image] = clamp_to_border_black;
	src_image_views[cur_image] = get_image_view(image, layer_data->flags, array_index);
	ubo_data->layers[cur_layer + 0].image_info.color_image_index = cur_image++;

	// Depth
	if (layer_data->type == XRT_LAYER_PROJECTION_DEPTH) {
		uint32_t d_array_index = dvd->sub.array_index;
		const struct comp_swapchain_image *d_image =
		    get_layer_depth_image(layer, sc_array_index, dvd->sub.image_index);

		src_samplers[cur_image] = clamp_to_edge; // Edge to keep depth stable at edges.
		src_image_views[cur_image] = get_image_view(d_image, layer_data->flags, d_array_index);
		ubo_data->layers[cur_layer + 0].image_info.depth_image_index = cur_image++;
	}

	// Chroma key - per layer settings
	const struct xrt_layer_chroma_key_data *ck = (layer_data->type == XRT_LAYER_PROJECTION_DEPTH)
	                                                 ? &layer_data->depth.chroma_key
	                                                 : &layer_data->proj.chroma_key;
	ubo_data->layers[cur_layer].chroma_key.hsv_min_h = ck->hsv_min.h;
	ubo_data->layers[cur_layer].chroma_key.hsv_min_s = ck->hsv_min.s;
	ubo_data->layers[cur_layer].chroma_key.hsv_min_v = ck->hsv_min.v;
	ubo_data->layers[cur_layer].chroma_key.hsv_max_h = ck->hsv_max.h;
	ubo_data->layers[cur_layer].chroma_key.hsv_max_s = ck->hsv_max.s;
	ubo_data->layers[cur_layer].chroma_key.hsv_max_v = ck->hsv_max.v;
	ubo_data->layers[cur_layer].chroma_key.curve = ck->curve;
	ubo_data->layers[cur_layer].chroma_key.despill = ck->despill;

	set_post_transform_rect(                           //
	    layer_data,                                    // data
	    &vd->sub.norm_rect,                            // src_norm_rect
	    false,                                         // invert_flip
	    &ubo_data->layers[cur_layer].post_transforms); // out_norm_rect

	// unused if timewarp is off
	if (do_timewarp) {
		render_calc_time_warp_matrix(                          //
		    &vd->pose,                                         //
		    &vd->fov,                                          //
		    world_pose_scanout_begin,                          //
		    &ubo_data->layers[cur_layer].transforms_timewarp); //
	}

	*out_cur_image = cur_image;
}

/// Data setup for a quad layer
static inline void
do_cs_quad_layer(const struct comp_layer *layer,
                 const struct xrt_matrix_4x4 *eye_view_mat,
                 const struct xrt_matrix_4x4 *world_view_mat,
                 uint32_t view_index,
                 uint32_t cur_layer,
                 uint32_t cur_image,
                 VkSampler clamp_to_edge,
                 VkSampler clamp_to_border_black,
                 VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE],
                 VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE],
                 struct render_compute_layer_ubo_data *ubo_data,
                 uint32_t *out_cur_image)
{
	const struct xrt_layer_data *layer_data = &layer->data;
	const struct xrt_layer_quad_data *q = &layer_data->quad;
	const uint32_t array_index = q->sub.array_index;
	const struct comp_swapchain_image *image = get_layer_image(layer, 0, q->sub.image_index);

	// Image to use.
	src_samplers[cur_image] = clamp_to_edge;
	src_image_views[cur_image] = get_image_view(image, layer_data->flags, array_index);

	// Set the normalized post transform values.
	struct xrt_normalized_rect post_transform = XRT_STRUCT_INIT;

	// Used for Subimage and OpenGL flip.
	set_post_transform_rect( //
	    layer_data,          // data
	    &q->sub.norm_rect,   // src_norm_rect
	    true,                // invert_flip
	    &post_transform);    // out_norm_rect

	// Is this layer viewspace or not.
	const struct xrt_matrix_4x4 *view_mat = is_layer_view_space(layer_data) ? eye_view_mat : world_view_mat;

	// Transform quad pose into view space.
	struct xrt_vec3 quad_position = XRT_STRUCT_INIT;
	math_matrix_4x4_transform_vec3(view_mat, &layer_data->quad.pose.position, &quad_position);

	// neutral quad layer faces +z, towards the user
	struct xrt_vec3 normal = (struct xrt_vec3){.x = 0, .y = 0, .z = 1};

	// rotation of the quad normal in world space
	struct xrt_quat rotation = layer_data->quad.pose.orientation;
	math_quat_rotate_vec3(&rotation, &normal, &normal);

	/*
	 * normal is a vector that originates on the plane, not on the origin.
	 * Instead of using the inverse quad transform to transform it into view space we can
	 * simply add up vectors:
	 *
	 * combined_normal [in world space] = plane_origin [in world space] + normal [in plane
	 * space] [with plane in world space]
	 *
	 * Then combined_normal can be transformed to view space via view matrix and a new
	 * normal_view_space retrieved:
	 *
	 * normal_view_space = combined_normal [in view space] - plane_origin [in view space]
	 */
	struct xrt_vec3 normal_view_space = normal;
	math_vec3_accum(&layer_data->quad.pose.position, &normal_view_space);
	math_matrix_4x4_transform_vec3(view_mat, &normal_view_space, &normal_view_space);
	math_vec3_subtract(&quad_position, &normal_view_space);

	struct xrt_vec3 scale = {1.f, 1.f, 1.f};
	struct xrt_matrix_4x4 plane_transform_view_space, inverse_quad_transform;
	math_matrix_4x4_model(&layer_data->quad.pose, &scale, &plane_transform_view_space);
	math_matrix_4x4_multiply(view_mat, &plane_transform_view_space, &plane_transform_view_space);
	math_matrix_4x4_inverse(&plane_transform_view_space, &inverse_quad_transform);

	// Write all of the UBO data.
	ubo_data->layers[cur_layer].post_transforms = post_transform;
	ubo_data->layers[cur_layer].quad_extent.val = layer_data->quad.size;
	ubo_data->layers[cur_layer].quad_position.val = quad_position;
	ubo_data->layers[cur_layer].quad_normal.val = normal_view_space;
	ubo_data->layers[cur_layer].inverse_quad_transform = inverse_quad_transform;
	ubo_data->layers[cur_layer].image_info.color_image_index = cur_image;
	cur_image++;

	*out_cur_image = cur_image;
}

static void
crc_clear_output(struct render_compute *render, const struct comp_render_dispatch_data *d)
{
	if (d->target.view_count > XRT_MAX_VIEWS) {
		U_LOG_E("Only supports max %d views!", XRT_MAX_VIEWS);
		assert(d->target.view_count <= XRT_MAX_VIEWS);
		return;
	}

	struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS];
	for (uint32_t i = 0; i < d->target.view_count; ++i) {
		target_viewport_datas[i] = d->views[i].target.viewport_data;
	}


	render_compute_clear(          //
	    render,                    //
	    d->target.cs.image,        //
	    d->target.cs.storage_view, // target_image_view
	    d->target.cs.final_layout, // final_layout
	    target_viewport_datas);    // views
}

/*
 *
 * Compute distortion helpers.
 *
 */

/// For use after squashing layers
static void
crc_distortion_after_squash(struct render_compute *render, const struct comp_render_dispatch_data *d)
{
	if (d->target.view_count > XRT_MAX_VIEWS) {
		U_LOG_E("Only supports max %d views!", XRT_MAX_VIEWS);
		assert(d->target.view_count <= XRT_MAX_VIEWS);
		return;
	}
	VkSampler clamp_to_border_black = render->r->samplers.clamp_to_border_black;

	// Data to fill in.
	VkImageView src_image_views[XRT_MAX_VIEWS];
	VkSampler src_samplers[XRT_MAX_VIEWS];
	struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS];
	struct xrt_normalized_rect src_norm_rects[XRT_MAX_VIEWS];
	struct xrt_fov src_fovs[XRT_MAX_VIEWS];
	struct xrt_pose world_poses_scanout_begin[XRT_MAX_VIEWS];
	struct xrt_pose world_poses_scanout_end[XRT_MAX_VIEWS];

	for (uint32_t i = 0; i < d->target.view_count; i++) {
		// Data to be filled in.
		VkImageView src_image_view;
		struct render_viewport_data viewport_data;
		struct xrt_normalized_rect src_norm_rect;

		// Gather data.
		src_image_view = d->views[i].squash_as_src.sample_view;
		src_norm_rect = d->views[i].squash_as_src.norm_rect;
		viewport_data = d->views[i].target.viewport_data;

		// Fill in data.
		src_image_views[i] = src_image_view;
		src_norm_rects[i] = src_norm_rect;
		src_samplers[i] = clamp_to_border_black;
		target_viewport_datas[i] = viewport_data;

		if (d->do_timewarp) {
			world_poses_scanout_begin[i] = d->views[i].world_pose_scanout_begin;
			world_poses_scanout_end[i] = d->views[i].world_pose_scanout_end;
			src_fovs[i] = d->views[i].fov;
		}
	}

	if (!d->do_timewarp) {
		render_compute_projection_no_timewarp( //
		    render,                            //
		    src_samplers,                      //
		    src_image_views,                   //
		    src_norm_rects,                    //
		    d->target.cs.image,                //
		    d->target.cs.storage_view,         // target_image_view
		    d->target.cs.final_layout,         // target_final_layout
		    target_viewport_datas);            // views
	} else {
		render_compute_projection_scanout_compensation( //
		    render,                                     //
		    src_samplers,                               //
		    src_image_views,                            //
		    src_norm_rects,                             //
		    src_fovs,                                   //
		    world_poses_scanout_begin,                  //
		    world_poses_scanout_end,                    //
		    d->target.cs.image,                         //
		    d->target.cs.storage_view,                  // target_image_view
		    d->target.cs.final_layout,                  // target_final_layout
		    target_viewport_datas);                     // views
	}
}

/// Fast path
static void
crc_distortion_fast_path(struct render_compute *render,
                         const struct comp_render_dispatch_data *d,
                         const struct comp_layer *layer,
                         const struct xrt_layer_projection_view_data *vds[XRT_MAX_VIEWS])
{
	if (d->target.view_count > XRT_MAX_VIEWS) {
		U_LOG_E("Only supports max %d views!", XRT_MAX_VIEWS);
		assert(d->target.view_count <= XRT_MAX_VIEWS);
		return;
	}

	// Fetch from this data.
	const struct xrt_layer_data *data = &layer->data;

	VkSampler clamp_to_border_black = render->r->samplers.clamp_to_border_black;

	// Data to fill in.
	VkImageView src_image_views[XRT_MAX_VIEWS];
	VkSampler src_samplers[XRT_MAX_VIEWS];
	struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS];
	struct xrt_normalized_rect src_norm_rects[XRT_MAX_VIEWS];
	struct xrt_fov src_fovs[XRT_MAX_VIEWS];
	struct xrt_pose src_poses[XRT_MAX_VIEWS];
	struct xrt_pose world_poses_scanout_begin[XRT_MAX_VIEWS];
	struct xrt_pose world_poses_scanout_end[XRT_MAX_VIEWS];

	for (uint32_t i = 0; i < d->target.view_count; i++) {
		// Data to be filled in.
		VkImageView src_image_view;
		struct render_viewport_data viewport_data;
		struct xrt_normalized_rect src_norm_rect;
		struct xrt_fov src_fov;
		struct xrt_pose src_pose;
		struct xrt_pose world_pose_scanout_begin;
		struct xrt_pose world_pose_scanout_end;
		uint32_t array_index = vds[i]->sub.array_index;
		const struct comp_swapchain_image *image = get_layer_image(layer, i, vds[i]->sub.image_index);

		// Gather data.
		src_image_view = get_image_view(image, data->flags, array_index);
		src_norm_rect = vds[i]->sub.norm_rect;
		viewport_data = d->views[i].target.viewport_data;
		src_fov = vds[i]->fov;
		src_pose = vds[i]->pose;
		world_pose_scanout_begin = d->views[i].world_pose_scanout_begin;
		world_pose_scanout_end = d->views[i].world_pose_scanout_end;

		// No layer squasher has handled this for us already
		if (data->flip_y) {
			src_norm_rect.y += src_norm_rect.h;
			src_norm_rect.h = -src_norm_rect.h;
		}

		// Fill in data.
		src_image_views[i] = src_image_view;
		src_norm_rects[i] = src_norm_rect;
		src_samplers[i] = clamp_to_border_black;
		target_viewport_datas[i] = viewport_data;
		src_fovs[i] = src_fov;
		src_poses[i] = src_pose;
		world_poses_scanout_begin[i] = world_pose_scanout_begin;
		world_poses_scanout_end[i] = world_pose_scanout_end;
	}

	if (!d->do_timewarp) {
		render_compute_projection_no_timewarp( //
		    render,                            //
		    src_samplers,                      //
		    src_image_views,                   //
		    src_norm_rects,                    //
		    d->target.cs.image,                //
		    d->target.cs.storage_view,         //
		    d->target.cs.final_layout,         //
		    target_viewport_datas);            //
	} else {
		render_compute_projection_timewarp( //
		    render,                         //
		    src_samplers,                   //
		    src_image_views,                //
		    src_norm_rects,                 //
		    src_poses,                      //
		    src_fovs,                       //
		    world_poses_scanout_begin,      //
		    world_poses_scanout_end,        //
		    d->target.cs.image,             //
		    d->target.cs.storage_view,      //
		    d->target.cs.final_layout,      //
		    target_viewport_datas);         //
	}
}


/*
 *
 * 'Exported' function(s).
 *
 */

void
comp_render_cs_layer(struct render_compute *render,
                     uint32_t view_index,
                     const struct comp_layer *layers,
                     const uint32_t layer_count,
                     const struct xrt_normalized_rect *pre_transform,
                     const struct xrt_pose *world_pose_scanout_begin,
                     const struct xrt_pose *world_pose_scanout_end,
                     const struct xrt_pose *eye_pose,
                     const VkImage target_image,
                     const VkImageView target_image_view,
                     const struct render_viewport_data *target_view,
                     bool do_timewarp)
{
	VkSampler clamp_to_edge = render->r->samplers.clamp_to_edge;
	VkSampler clamp_to_border_black = render->r->samplers.clamp_to_border_black;

	// Not the transform of the views, but the inverse: actual view matrices.
	struct xrt_matrix_4x4 world_view_mat_scanout_begin, eye_view;
	math_matrix_4x4_view_from_pose(world_pose_scanout_begin, &world_view_mat_scanout_begin);
	math_matrix_4x4_view_from_pose(eye_pose, &eye_view);

	struct render_buffer *ubo = &render->r->compute.layer.ubos[view_index];
	struct render_compute_layer_ubo_data *ubo_data = ubo->mapped;

	// Tightly pack layers in data struct.
	uint32_t cur_layer = 0;

	// Tightly pack color and optional depth images.
	uint32_t cur_image = 0;
	VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE];
	VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE];

	ubo_data->view = *target_view;
	ubo_data->pre_transform = *pre_transform;

	// Initialize chroma key settings for all layers to zero
	for (uint32_t i = 0; i < RENDER_MAX_LAYERS; i++) {
		ubo_data->layers[i].chroma_key = (struct render_chroma_key_info){0};
	}

	for (uint32_t c_layer_i = 0; c_layer_i < layer_count; c_layer_i++) {
		const struct comp_layer *layer = &layers[c_layer_i];
		const struct xrt_layer_data *data = &layer->data;

		if (!is_layer_view_visible(data, view_index)) {
			continue;
		}

		/*!
		 * Stop compositing layers if device's sampled image limit is
		 * reached. For most hardware this isn't a problem, most have
		 * well over 32 max samplers. But notably the RPi4 only have 16
		 * which is a limit we may run into. But if you got 16+ layers
		 * on a RPi4 you have more problems then max samplers.
		 */
		uint32_t required_image_samplers;
		switch (data->type) {
		case XRT_LAYER_CYLINDER: required_image_samplers = 1; break;
		case XRT_LAYER_EQUIRECT2: required_image_samplers = 1; break;
		case XRT_LAYER_PROJECTION: required_image_samplers = 1; break;
		case XRT_LAYER_PROJECTION_DEPTH: required_image_samplers = 2; break;
		case XRT_LAYER_QUAD: required_image_samplers = 1; break;
		default:
			VK_ERROR(render->r->vk, "Skipping layer #%u, unknown type: %u", c_layer_i, data->type);
			continue; // Skip this layer if don't know about it.
		}

		//! Exit loop if shader cannot receive more image samplers
		if (cur_image + required_image_samplers > render->r->compute.layer.image_array_size) {
			break;
		}

		switch (data->type) {
		case XRT_LAYER_CYLINDER:
			do_cs_cylinder_layer(              //
			    layer,                         // layer
			    &eye_view,                     // eye_view_mat
			    &world_view_mat_scanout_begin, // world_view_mat
			    view_index,                    // view_index
			    cur_layer,                     // cur_layer
			    cur_image,                     // cur_image
			    clamp_to_edge,                 // clamp_to_edge
			    clamp_to_border_black,         // clamp_to_border_black
			    src_samplers,                  // src_samplers
			    src_image_views,               // src_image_views
			    ubo_data,                      // ubo_data
			    &cur_image);                   // out_cur_image
			break;
		case XRT_LAYER_EQUIRECT2:
			do_cs_equirect2_layer(             //
			    layer,                         // layer
			    &eye_view,                     // eye_view_mat
			    &world_view_mat_scanout_begin, // world_view_mat
			    view_index,                    // view_index
			    cur_layer,                     // cur_layer
			    cur_image,                     // cur_image
			    clamp_to_edge,                 // clamp_to_edge
			    clamp_to_border_black,         // clamp_to_border_black
			    src_samplers,                  // src_samplers
			    src_image_views,               // src_image_views
			    ubo_data,                      // ubo_data
			    &cur_image);                   // out_cur_image
			break;
		case XRT_LAYER_PROJECTION_DEPTH:
		case XRT_LAYER_PROJECTION: {
			do_cs_projection_layer(       //
			    layer,                    // layer
			    world_pose_scanout_begin, // world_pose_scanout_begin
			    view_index,               // view_index
			    cur_layer,                // cur_layer
			    cur_image,                // cur_image
			    clamp_to_edge,            // clamp_to_edge
			    clamp_to_border_black,    // clamp_to_border_black
			    src_samplers,             // src_samplers
			    src_image_views,          // src_image_views
			    ubo_data,                 // ubo_data
			    do_timewarp,              // do_timewarp
			    &cur_image);              // out_cur_image
		} break;
		case XRT_LAYER_QUAD: {
			do_cs_quad_layer(                  //
			    layer,                         // layer
			    &eye_view,                     // eye_view_mat
			    &world_view_mat_scanout_begin, // world_view_mat_scanout_begin
			    view_index,                    // view_index
			    cur_layer,                     // cur_layer
			    cur_image,                     // cur_image
			    clamp_to_edge,                 // clamp_to_edge
			    clamp_to_border_black,         // clamp_to_border_black
			    src_samplers,                  // src_samplers
			    src_image_views,               // src_image_views
			    ubo_data,                      // ubo_data
			    &cur_image);                   // out_cur_image
		} break;
		default:
			// Should not get here!
			assert(false);
			VK_ERROR(render->r->vk, "Should not get here!");
			continue;
		}

		// Shared with shader source, no enums there.
		uint32_t cs_layer_type = xrt_layer_to_cs_layer_type(data);

		// Context should always have a layer after it.
		assert((cs_layer_type != LAYER_COMP_TYPE_CONTEXT || cur_layer < (layer_count - 1)) &&
		       "Context should always have a layer after it.");

		// Inset should always have a layer before it.
		assert((cs_layer_type != LAYER_COMP_TYPE_INSET || cur_layer >= 1) &&
		       "Inset should always have a layer before it.");

		ubo_data->layers[cur_layer].layer_data.layer_type = cs_layer_type;
		ubo_data->layers[cur_layer].layer_data.unpremultiplied_alpha = is_layer_unpremultiplied(data);
		ubo_data->layers[cur_layer].layer_data.inverted_alpha = is_layer_alpha_inverted(data);

		apply_bias_and_scale_from_layer(data, &ubo_data->layers[cur_layer].color_scale,
		                                &ubo_data->layers[cur_layer].color_bias);

		// Finally okay to increment the current layer.
		cur_layer++;
	}

	// Set the number of layers.
	ubo_data->layer_count.value = cur_layer;

	for (uint32_t i = cur_layer; i < RENDER_MAX_LAYERS; i++) {
		ubo_data->layers[i].layer_data.layer_type = LAYER_COMP_TYPE_NOOP; // Explicit no-op.
	}

	//! @todo: If Vulkan 1.2, use VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT and skip this
	while (cur_image < render->r->compute.layer.image_array_size) {
		src_samplers[cur_image] = clamp_to_edge;
		src_image_views[cur_image] = render->r->mock.color.image_view;
		cur_image++;
	}

	VkDescriptorSet descriptor_set = render->layer_descriptor_sets[view_index];

	render_compute_layers( //
	    render,            //
	    descriptor_set,    //
	    ubo->buffer,       //
	    src_samplers,      //
	    src_image_views,   //
	    cur_image,         //
	    target_image_view, //
	    target_view,       //
	    do_timewarp);      //
}

void
comp_render_cs_layers(struct render_compute *render,
                      const struct comp_layer *layers,
                      const uint32_t layer_count,
                      const struct comp_render_dispatch_data *d,
                      VkImageLayout transition_to)
{
	cmd_barrier_view_squash_images(            //
	    render->r->vk,                         //
	    d,                                     //
	    render->r->cmd,                        // cmd
	    0,                                     // src_access_mask
	    VK_ACCESS_SHADER_WRITE_BIT,            // dst_access_mask
	    VK_IMAGE_LAYOUT_UNDEFINED,             // transition_from
	    VK_IMAGE_LAYOUT_GENERAL,               // transition_to
	    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,    // src_stage_mask
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); // dst_stage_mask

	for (uint32_t view_index = 0; view_index < d->squash_view_count; view_index++) {
		const struct comp_render_view_data *view = &d->views[view_index];

		comp_render_cs_layer(                //
		    render,                          //
		    view_index,                      //
		    layers,                          //
		    layer_count,                     //
		    &view->pre_transform,            //
		    &view->world_pose_scanout_begin, //
		    &view->world_pose_scanout_end,   //
		    &view->eye_pose,                 //
		    view->squash.image,              //
		    view->squash.cs.storage_view,    //
		    &view->squash.viewport_data,     //
		    d->do_timewarp);                 //
	}

	cmd_barrier_view_squash_images(            //
	    render->r->vk,                         //
	    d,                                     //
	    render->r->cmd,                        // cmd
	    VK_ACCESS_SHADER_WRITE_BIT,            // src_access_mask
	    VK_ACCESS_MEMORY_READ_BIT,             // dst_access_mask
	    VK_IMAGE_LAYOUT_GENERAL,               // transition_from
	    transition_to,                         //
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // src_stage_mask
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); // dst_stage_mask
}

void
comp_render_cs_dispatch(struct render_compute *render,
                        const struct comp_layer *layers,
                        const uint32_t layer_count,
                        const struct comp_render_dispatch_data *d)
{
	if (!d->target.initialized) {
		VK_ERROR(render->r->vk, "Target hasn't been initialized, not rendering anything.");
		assert(d->target.initialized);
		return;
	}

	// Convenience.
	bool fast_path = d->fast_path;

	// Only used if fast_path is true.
	const struct comp_layer *layer = &layers[0];

	// Consistency check.
	assert(!fast_path || layer_count >= 1);

	// We want to read from the images afterwards.
	VkImageLayout transition_to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (fast_path && layer->data.type == XRT_LAYER_PROJECTION) {
		// Fast path.
		const struct xrt_layer_projection_data *proj = &layer->data.proj;
		const struct xrt_layer_projection_view_data *vds[XRT_MAX_VIEWS];
		for (uint32_t view = 0; view < d->target.view_count; ++view) {
			vds[view] = &proj->v[view];
		}
		crc_distortion_fast_path( //
		    render,               //
		    d,                    //
		    layer,                //
		    vds);                 //

	} else if (fast_path && layer->data.type == XRT_LAYER_PROJECTION_DEPTH) {
		// Fast path.
		const struct xrt_layer_projection_depth_data *depth = &layer->data.depth;
		const struct xrt_layer_projection_view_data *vds[XRT_MAX_VIEWS];
		for (uint32_t view = 0; view < d->target.view_count; ++view) {
			vds[view] = &depth->v[view];
		}
		crc_distortion_fast_path( //
		    render,               //
		    d,                    //
		    layer,                //
		    vds);                 //

	} else if (layer_count > 0) {
		// Compute layer squasher
		if (fast_path) {
			U_LOG_W("Wanted fast path but no projection layer, falling back to layer squasher.");
		}

		/*
		 * Layer squashing.
		 */
		comp_render_cs_layers( //
		    render,            //
		    layers,            //
		    layer_count,       //
		    d,                 //
		    transition_to);    //

		/*
		 * Distortion.
		 */
		crc_distortion_after_squash( //
		    render,                  //
		    d);                      //

	} else {
		// Just clear the screen
		crc_clear_output( //
		    render,       //
		    d);           //
	}

	// struct vk_bundle* vk = render->r->vk;
	// struct render_resources* rr = render->r;

	// VkImageLayout target_final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// VkImageSubresourceRange range = {
	//     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	//     .baseMipLevel = 0,
	//     .levelCount = 1,
	//     .baseArrayLayer = 0,
	//     .layerCount = 1,
	// };

	// // Barrier to make source a source
	// vk_cmd_image_barrier_locked(                       //
	//     vk,                                            // vk_bundle
	//     rr->cmd,                                        // cmdbuffer
	//     rr->mock.color.image,                                           // image
	//     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
	//     VK_ACCESS_TRANSFER_READ_BIT,                   // dstAccessMask
	//     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,      // oldImageLayout
	//     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,          // newImageLayout
	//     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
	//     VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
	//     range);                                        // subresourceRange

	// for (uint32_t view_index = 0; view_index < d->squash_view_count; view_index++) {
	// 	VkImage target_image = d->views[view_index].squash.image;


	// 	// Barrier to make destination a destination
	// 	vk_cmd_image_barrier_locked(              //
	// 		vk,                                   // vk_bundle
	// 		rr->cmd,                               // cmdbuffer
	// 		target_image,                                  // image
	// 		0,                                    // srcAccessMask
	// 		VK_ACCESS_TRANSFER_WRITE_BIT,         // dstAccessMask
	// 		VK_IMAGE_LAYOUT_UNDEFINED,            // oldImageLayout
	// 		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
	// 		VK_PIPELINE_STAGE_TRANSFER_BIT,       // srcStageMask
	// 		VK_PIPELINE_STAGE_TRANSFER_BIT,       // dstStageMask
	// 		range);                               // subresourceRange

	// 	VkImageBlit blit = {
	// 		.srcSubresource =
	// 			{
	// 				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	// 				.layerCount = 1,
	// 			},
	// 		.dstSubresource =
	// 			{
	// 				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	// 				.layerCount = 1,
	// 			},
	// 	};

	// 	blit.srcOffsets[1].x = 200;
	// 	blit.srcOffsets[1].y = 200;
	// 	blit.srcOffsets[1].z = 1;

	// 	blit.dstOffsets[1].x = 500;
	// 	blit.dstOffsets[1].y = 500;
	// 	blit.dstOffsets[1].z = 1;

	// 	vk->vkCmdBlitImage(                       //
	// 		rr->cmd,                              // commandBuffer
	// 		rr->mock.color.image,                 // srcImage
	// 		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // srcImageLayout
	// 		target_image,                                  // dstImage
	// 		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // dstImageLayout
	// 		1,                                    // regionCount
	// 		&blit,                                // pRegions
	// 		VK_FILTER_LINEAR                      // filter
	// 	);

	// 	// Reset destination
	// 	vk_cmd_image_barrier_locked(              //
	// 		vk,                                   // vk_bundle
	// 		rr->cmd,                               // cmdbuffer
	// 		target_image,                                  // image
	// 		VK_ACCESS_TRANSFER_WRITE_BIT,         // srcAccessMask
	// 		0,                                    // dstAccessMask
	// 		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
	// 		target_final_layout,            // newImageLayout
	// 		VK_PIPELINE_STAGE_TRANSFER_BIT,       // srcStageMask
	// 		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
	// 		range);                               // subresourceRange
	// }

	// // Reset src
	// vk_cmd_image_barrier_locked(                  //
	//     vk,                                       // vk_bundle
	//     rr->cmd,                                  // cmdbuffer
	//     rr->mock.color.image,                     // image
	//     VK_ACCESS_TRANSFER_READ_BIT,              // srcAccessMask
	//     0,                                        // dstAccessMask
	//     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,     // oldImageLayout
	//     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // newImageLayout
	//     VK_PIPELINE_STAGE_TRANSFER_BIT,           // srcStageMask
	//     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,        // dstStageMask
	//     range);                                   // subresourceRange

}
