// Copyright 2019-2024, Collabora, Ltd.
// Copyright 2024-2025, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Higher level interface for rendering a frame.
 * @author Jakob Bornecrantz <tbornecrantz@nvidia.com>
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @author Fernando Velazquez Innella <finnella@magicleap.com>
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup comp_util
 */

#pragma once


#include "render/render_interface.h"

#include "comp_render.h"
#include "comp_high_level_scratch.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * Encapsulates all the needed state to run the layer squasher and distortion
 * passes, state object needs to be kept alive until the GPU work finishes.
 * None of the functions are thread safe so make sure to synchronize access.
 *
 * The lifetime of this struct is one frame, but as stated above it needs to
 * live for long enough that the GPU work has been finished.
 *
 * @comp_util
 */
struct chl_frame_state
{
	struct chl_scratch *scratch;

	uint32_t view_count;

	struct chl_scratch_state scratch_state;

	struct comp_render_dispatch_data data;

	struct render_compute cs;
};


/*
 *
 * Shared functions.
 *
 */

/*!
 * Create the Vulkan resources using the given @p render_resources and the
 * @p vk_bundle it refers to. Is used for both graphics and compute paths,
 * also manages the scratch state.
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_init(struct chl_frame_state *frame_state,
                     struct render_resources *rr,
                     uint32_t view_count,
                     bool do_timewarp,
                     bool fast_path,
                     struct chl_scratch *scratch);

/*!
 * Frees all resources that this frame state tracks and manages the scratch
 * images state. Must be called after the GPU work has finished and has been
 * waited on (or the validation layer gets upset).
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_fini(struct chl_frame_state *state);


/*
 *
 * Graphics.
 *
 */

/*!
 * Sets all the needed state to run the layer squasher to the scratch images,
 * this is the graphics version.
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_gfx_set_views(struct chl_frame_state *frame_state,
                              const struct xrt_pose world_pose_scanout_begin[XRT_MAX_VIEWS],
                              const struct xrt_pose world_pose_scanout_end[XRT_MAX_VIEWS],
                              const struct xrt_pose eye_pose[XRT_MAX_VIEWS],
                              const struct xrt_fov fov[XRT_MAX_VIEWS],
                              uint32_t layer_count);

/*!
 * Adds the needed information to also perform a distortion step, reuses some
 * information from the _set_views call and as such this needs to be called
 * before calling this function. This is the graphics version.
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_gfx_set_target(struct chl_frame_state *frame_state,
                               struct render_gfx_target_resources *target_rtr,
                               const struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS],
                               const render_scissor_data_t target_scissor_datas[XRT_MAX_VIEWS],
                               const struct xrt_matrix_2x2 vertex_rots[XRT_MAX_VIEWS]);

/*!
 * A single do all function, runs the default graphics pipeline.
 *
 * @memberof chl_frame_state
 */
static inline void
chl_frame_state_gfx_default_pipeline(struct chl_frame_state *frame_state,
                                     struct render_gfx *render,
                                     const struct comp_layer *layers,
                                     uint32_t layer_count,
                                     const struct xrt_pose world_poses_scanout_begin[XRT_MAX_VIEWS],
                                     const struct xrt_pose world_poses_scanout_end[XRT_MAX_VIEWS],
                                     const struct xrt_pose eye_poses[XRT_MAX_VIEWS],
                                     const struct xrt_fov fovs[XRT_MAX_VIEWS],
                                     struct render_gfx_target_resources *target_rtr,
                                     const struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS],
                                     const struct xrt_matrix_2x2 vertex_rots[XRT_MAX_VIEWS])
{
	chl_frame_state_gfx_set_views( //
	    frame_state,               //
	    world_poses_scanout_begin, //
	    world_poses_scanout_end,   //
	    eye_poses,                 //
	    fovs,                      //
	    layer_count);              //

	chl_frame_state_gfx_set_target( //
	    frame_state,                //
	    target_rtr,                 //
	    target_viewport_datas,      //
	    target_viewport_datas,      //
	    vertex_rots);               //

	// Start the compute pipeline.
	render_gfx_begin(render);

	// Build the command buffer.
	comp_render_gfx_dispatch( //
	    render,               //
	    layers,               //
	    layer_count,          //
	    &frame_state->data);  //

	// Make the command buffer submittable.
	render_gfx_end(render);
}

static inline void
chl_frame_state_png_default_pipeline(struct render_png_render_pass *render_pass,
                                     struct render_gfx *render,
                                     const struct comp_layer *layers,
                                     uint32_t layer_count)
{
	// Start the compute pipeline.
	render_png_begin(render);

	comp_render_png_dispatch(render, layers, layer_count, render_pass);

	// Make the command buffer submittable.
	render_png_end(render);
}


/*
 *
 * Compute
 *
 */

/*!
 * Sets all the needed state to run the layer squasher to the scratch images,
 * this is the compute version.
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_cs_set_views(struct chl_frame_state *frame_state,
                             const struct xrt_pose world_pose_scanout_begin[XRT_MAX_VIEWS],
                             const struct xrt_pose world_pose_scanout_end[XRT_MAX_VIEWS],
                             const struct xrt_pose eye_pose[XRT_MAX_VIEWS],
                             const struct xrt_fov fov[XRT_MAX_VIEWS],
                             uint32_t layer_count);

/*!
 * Adds the needed information to also perform a distortion step, reuses some
 * information from the _set_views call and as such this needs to be called
 * before calling this function. This is the compute version.
 *
 * @memberof chl_frame_state
 */
void
chl_frame_state_cs_set_target(struct chl_frame_state *frame_state,
                              VkImage target_image,
                              VkImageView target_storage_view,
                              VkImageLayout target_final_layout,
                              const struct render_viewport_data views[XRT_MAX_VIEWS]);

/*!
 * A single do all function, runs the default compute pipeline.
 *
 * @memberof chl_frame_state
 */
static inline void
chl_frame_state_cs_default_pipeline(struct chl_frame_state *frame_state,
                                    struct render_compute *render,
                                    const struct comp_layer *layers,
                                    uint32_t layer_count,
                                    const struct xrt_pose world_poses_scanout_begin[XRT_MAX_VIEWS],
                                    const struct xrt_pose world_poses_scanout_end[XRT_MAX_VIEWS],
                                    const struct xrt_pose eye_poses[XRT_MAX_VIEWS],
                                    const struct xrt_fov fovs[XRT_MAX_VIEWS],
                                    VkImage target_image,
                                    VkImageView target_storage_view,
                                    VkImageLayout target_final_layout,
                                    const struct render_viewport_data target_viewport_datas[XRT_MAX_VIEWS])
{
	chl_frame_state_cs_set_views(  //
	    frame_state,               //
	    world_poses_scanout_begin, //
	    world_poses_scanout_end,   //
	    eye_poses,                 //
	    fovs,                      //
	    layer_count);              //

	chl_frame_state_cs_set_target( //
	    frame_state,               //
	    target_image,              //
	    target_storage_view,       //
	    target_final_layout,       //
	    target_viewport_datas);    //

	// Start the compute pipeline.
	render_compute_begin(render);

	// Build the command buffer.
	comp_render_cs_dispatch( //
	    render,              //
	    layers,              //
	    layer_count,         //
	    &frame_state->data); //

	// Make the command buffer submittable.
	render_compute_end(render);
}

#ifdef __cplusplus
}
#endif
