// Copyright 2019-2024, Collabora, Ltd.
// Copyright 2025, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Compositor render implementation.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup comp_util
 */

#pragma once

#include "xrt/xrt_defines.h"
#include "xrt/xrt_vulkan_includes.h" // IWYU pragma: keep

#include "render/render_interface.h"
#include "util/u_misc.h"

#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

struct comp_layer;
struct render_compute;
struct render_gfx;
struct render_gfx_target_resources;


/*!
 * @defgroup comp_render
 * @brief Renders, aka "layer squashers" and distortion application.
 *
 * Two parallel implementations of the render module exist:
 *
 * - one uses graphics shaders (aka GFX, @ref comp_render_gfx, @ref comp_render_gfx.c)
 * - the other uses compute shaders (aka CS, @ref comp_render_cs, @ref comp_render_cs.c)
 *
 * Their abilities are effectively equivalent, although the graphics version disregards depth
 * data, while the compute shader does use it somewhat.
 *
 * @note In general this module requires that swapchains in your supplied @ref comp_layer layers
 * implement @ref comp_swapchain in addition to just @ref xrt_swapchain.
 */

/*
 *
 * Input data struct.
 *
 */

/*!
 * @name Input data structs
 * @{
 */

/*!
 * The input data needed for a single view, shared between both GFX and CS
 * paths.
 *
 * To fully render a single view two "rendering" might be needed, the
 * first being the layer squashing, and the second is the distortion step. The
 * target for the layer squashing is referred to as "layer" or "scratch" and
 * prefixed with `layer` if needs be. The other final step is referred to as
 * "distortion target" or just "target", and is prefixed with `target`.
 *
 * @ingroup comp_render
 */
struct comp_render_view_data
{
	//! New world pose of this view at the beginng of scanout.
	struct xrt_pose world_pose_scanout_begin;

	//! New world pose of this view at the end of scanout.
	struct xrt_pose world_pose_scanout_end;

	//! New eye pose of this view.
	struct xrt_pose eye_pose;

	/*!
	 * New fov of this view, used for the layer scratch image. Needs to
	 * match distortion parameters if distortion is used.
	 */
	struct xrt_fov fov;

	/*!
	 * Go from UV to tanangle for both the target and layer image since
	 * they share the same fov, this needs to match @p fov.
	 */
	struct xrt_normalized_rect pre_transform;

	struct
	{
		/*!
		 * The layer image for this view (aka scratch image),
		 * used for barrier operations.
		 */
		VkImage image;

		/*!
		 * Pre-view layer target viewport_data (where in the image we
		 * should render the view).
		 */
		struct render_viewport_data viewport_data;

		struct
		{
			//! Per-view layer target resources.
			struct render_gfx_target_resources *rtr;
		} gfx;

		struct
		{
			/*!
			 * View into layer image (aka scratch image), for used
			 * as a storage tagert of the CS (write) path.
			 */
			VkImageView storage_view;
		} cs;
	} squash; // When used as a destination.

	struct
	{
		/*!
		 * When sampling from the layer image (aka scratch image), how
		 * should we transform it to get to the pixels correctly.
		 *
		 * Ignored when doing a fast path and reading directly from
		 * the app projection layer.
		 */
		struct xrt_normalized_rect norm_rect;

		/*!
		 * View into layer image (aka scratch image) when sampling the
		 * image, used for both GFX (read) and CS (read) paths.
		 *
		 * Ignored when doing a fast path and reading directly from
		 * the app projection layer.
		 */
		VkImageView sample_view;

	} squash_as_src; // The input to the target/distortion shaders.

	struct
	{
		// Distortion target viewport data (aka target).
		struct render_viewport_data viewport_data;

		/*!
		 * Distortion target scissor data (aka target).
		 *
		 * Typically the scissor rect will be equal to @ref viewport_data.
		 */
		render_scissor_data_t scissor_data;

		struct
		{
			//! Distortion target vertex rotation information.
			struct xrt_matrix_2x2 vertex_rot;
		} gfx;
	} target; // When used as a destination.
};

/*!
 * The input data needed for a complete layer squashing distortion rendering
 * to a target. This struct is shared between GFX and CS paths.
 *
 * @ingroup comp_render
 */
struct comp_render_dispatch_data
{
	struct comp_render_view_data views[XRT_MAX_VIEWS];

	/*!
	 * The number of squash views currently in this dispatch data.
	 */
	uint32_t squash_view_count;


	//! Fast path can be disabled for mirroing so needs to be an argument.
	bool fast_path;

	//! Very often true, can be disabled for debugging.
	bool do_timewarp;

	struct
	{
		//! Has this struct been setup to use the target.
		bool initialized;

		/*!
		 * The number of target views currently, when calling dispatch
		 * this has to be either zero or the same number as
		 * squash_view_count, see also the target.initialized field.
		 */
		uint32_t view_count;

		//! Members used only by GFX @ref comp_render_gfx
		struct
		{
			//! The resources needed for the target.
			struct render_gfx_target_resources *rtr;
		} gfx;

		//! Members used only by CS @ref comp_render_cs
		struct
		{
			//! Target image for distortion, used for barrier.
			VkImage image;

			//! Target image view for distortion.
			VkImageView storage_view;

			//! Target image layout for distortion, the final layout to transition to.
			VkImageLayout final_layout;
		} cs;
	} target;
};

/*!
 * Initialize structure for use without the target step.
 *
 * @param[out] data Common render dispatch data. Will be zeroed and initialized.
 * @param fast_path Whether we will use the "fast path" avoiding layer squashing.
 * @param do_timewarp Whether timewarp (reprojection) will be performed.
 */
static inline void
comp_render_initial_init(struct comp_render_dispatch_data *data, bool fast_path, bool do_timewarp)
{
	U_ZERO(data);

	data->fast_path = fast_path;
	data->do_timewarp = do_timewarp;
}

/*!
 * Shared implementation setting up common view params between GFX and CS.
 *
 * Private implementation method, do not use outside of more-specific add_view calls!
 *
 * @param data Common render dispatch data, will be updated
 * @param world_pose New world pose of this view.
 *        Populates @ref comp_render_view_data::world_pose
 * @param eye_pose New eye pose of this view
 *        Populates @ref comp_render_view_data::eye_pose
 * @param fov Assigned to fov in the view data, and used to compute @ref comp_render_view_data::target_pre_transform
 *        Populates @ref comp_render_view_data::fov
 * @param squash_image Scratch image for this view
 *        Populates @ref comp_render_view_data::squash::image
 * @param squash_viewport_data Where in the image to render the view
 *        Populates @ref comp_render_view_data::squash::viewport_data
 * @param squash_as_src_sample_view The image view into the scratch image for sampling.
 *        Populates @ref comp_render_view_data::squash_as_src::sample_view
 * @param squash_as_src_norm_rect How to transform when sampling from the scratch image.
 *        Populates @ref comp_render_view_data::squash_as_src::norm_rect
 * @param target_viewport_data Distortion target viewport data (aka target)
 *        Populates @ref comp_render_view_data::target::viewport_data

 * @return Pointer to the @ref comp_render_view_data we have been populating, for additional setup.
 */
static inline struct comp_render_view_data *
comp_render_dispatch_add_squash_view(struct comp_render_dispatch_data *data,
                                     const struct xrt_pose *world_pose_scanout_begin,
                                     const struct xrt_pose *world_pose_scanout_end,
                                     const struct xrt_pose *eye_pose,
                                     const struct xrt_fov *fov,
                                     VkImage squash_image,
                                     const struct render_viewport_data *squash_viewport_data)
{
	uint32_t i = data->squash_view_count++;

	assert(i < ARRAY_SIZE(data->views));

	struct comp_render_view_data *view = &data->views[i];

	render_calc_uv_to_tangent_lengths_rect(fov, &view->pre_transform);

	// Common
	view->world_pose_scanout_begin = *world_pose_scanout_begin;
	view->world_pose_scanout_end = *world_pose_scanout_end;
	view->eye_pose = *eye_pose;
	view->fov = *fov;

	// When writing into the squash (aka scratch) image.
	view->squash.image = squash_image;
	view->squash.viewport_data = *squash_viewport_data;

	return view;
}

static inline struct comp_render_view_data *
comp_render_dispatch_add_target_view(struct comp_render_dispatch_data *data,
                                     VkImageView squash_as_src_sample_view,
                                     const struct xrt_normalized_rect *squash_as_src_norm_rect,
                                     const struct render_viewport_data *target_viewport_data,
                                     const render_scissor_data_t *target_scissor_data)
{
	uint32_t i = data->target.view_count++;

	assert(i < data->squash_view_count);
	assert(i < ARRAY_SIZE(data->views));

	struct comp_render_view_data *view = &data->views[i];

	// When using the squash (aka scratch) image as a source.
	view->squash_as_src.sample_view = squash_as_src_sample_view;
	view->squash_as_src.norm_rect = *squash_as_src_norm_rect;

	// When writing into the target.
	view->target.viewport_data = *target_viewport_data;
	view->target.scissor_data = *target_scissor_data;

	return view;
}

/*! @} */

/*
 *
 * Gfx functions.
 *
 */

/*!
 *
 * @defgroup comp_render_gfx
 *
 * GFX renderer control and dispatch - uses graphics shaders.
 *
 * Depends on the common @ref comp_render_dispatch_data, as well as the resources
 * @ref render_gfx_target_resources (often called `rtr`), and @ref render_gfx.
 *
 * @ingroup comp_render
 * @{
 */

/*!
 * Initialize structure for use of the GFX renderer.
 *
 * @param[in,out] data Common render dispatch data.
 * @param target_rtr GFX-specific resources for the entire framebuffer. Must be populated before call.
 */
static inline void
comp_render_gfx_add_target(struct comp_render_dispatch_data *data, struct render_gfx_target_resources *target_rtr)
{
	// Error tracking.
	data->target.initialized = true;

	// When writing into the target.
	data->target.gfx.rtr = target_rtr;
}

/*!
 * Add view to the common data, as required by the GFX renderer.
 *
 * @param[in,out] data Common render dispatch data, will be updated
 * @param world_pose New world pose of this view.
 *        Populates @ref comp_render_view_data::world_pose
 * @param eye_pose New eye pose of this view
 *        Populates @ref comp_render_view_data::eye_pose
 * @param fov Assigned to fov in the view data, and used to
 *        compute @ref comp_render_view_data::pre_transform - also
 *        populates @ref comp_render_view_data::fov
 * @param layer_image Scratch image for this view
 *        Populates @ref comp_render_view_data::squash::image
 * @param later_rtr Will be associated with this view. GFX-specific
 * @param layer_viewport_data Where in the image to render the view
 *        Populates @ref comp_render_view_data::squash::viewport_data
 * @param squash_as_src_norm_rect How to transform when sampling from the scratch image.
 *        Populates @ref comp_render_view_data::squash_as_src::norm_rect
 * @param squash_as_src_sample_view The image view into the scratch image for sampling.
 *        Populates @ref comp_render_view_data::squash_as_src::sample_view
 * @param target_vertex_rot
 *        Populates @ref comp_render_view_data::target.gfx.vertex_rot
 * @param target_viewport_data Distortion target viewport data (aka target)
 *        Populates @ref comp_render_view_data::target.viewport_data
 */
static inline void
comp_render_gfx_add_squash_view(struct comp_render_dispatch_data *data,
                                const struct xrt_pose *world_pose_scanout_begin,
                                const struct xrt_pose *world_pose_scanout_end,
                                const struct xrt_pose *eye_pose,
                                const struct xrt_fov *fov,
                                VkImage squash_image,
                                struct render_gfx_target_resources *squash_rtr,
                                const struct render_viewport_data *layer_viewport_data)
{
	struct comp_render_view_data *view = comp_render_dispatch_add_squash_view( //
	    data,                                                                  //
	    world_pose_scanout_begin,                                              //
	    world_pose_scanout_end,                                                //
	    eye_pose,                                                              //
	    fov,                                                                   //
	    squash_image,                                                          //
	    layer_viewport_data);                                                  //

	// When writing into the squash (aka scratch) image.
	view->squash.gfx.rtr = squash_rtr;
}

static inline void
comp_render_gfx_add_target_view(struct comp_render_dispatch_data *data,
                                VkImageView squash_as_src_sample_view,
                                const struct xrt_normalized_rect *squash_as_src_norm_rect,
                                const struct xrt_matrix_2x2 *target_vertex_rot,
                                const struct render_viewport_data *target_viewport_data,
                                const render_scissor_data_t *target_scissor_data)
{
	struct comp_render_view_data *view = comp_render_dispatch_add_target_view( //
	    data,                                                                  //
	    squash_as_src_sample_view,                                             //
	    squash_as_src_norm_rect,                                               //
	    target_viewport_data,                                                  //
	    target_scissor_data);                                                  //

	// When writing into the target.
	view->target.gfx.vertex_rot = *target_vertex_rot;
}

/*!
 * Dispatch the (graphics pipeline) layer squasher, on any number of views.
 *
 * All source layer images needs to be in the correct image layout, no barrier
 * is inserted for them. The target images are barriered from undefined to general
 * so they can be written to, then to the layout defined by @p transition_to.
 *
 * Expected layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target images: Any
 *
 * After call layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target images: @p transition_to
 *
 * @note Swapchains in the @p layers must implement @ref comp_swapchain in
 * addition to just @ref xrt_swapchain, as this function downcasts to @ref comp_swapchain !
 *
 * @param render Graphics renderer object
 * @param[in] layers Layers to render, see note.
 * @param[in] layer_count Number of elements in @p layers array.
 * @param[in] d Common render dispatch data
 * @param[in] transition_to Desired image layout for target images
 */
void
comp_render_gfx_layers(struct render_gfx *render,
                       const struct comp_layer *layers,
                       uint32_t layer_count,
                       const struct comp_render_dispatch_data *d,
                       VkImageLayout transition_to);

/*!
 * Writes the needed commands to the @ref render_gfx to do a full composition with distortion.
 *
 * Takes a set of layers, new device poses, scratch
 * images with associated @ref render_gfx_target_resources and writes the needed
 * commands to the @ref render_gfx to do a full composition with distortion.
 * The scratch images are optionally used to squash layers should it not be
 * possible to do a @p comp_render_dispatch_data::fast_path. Will use the render
 * passes of the targets which set the layout.
 *
 * The render passes of @p comp_render_dispatch_data::views::rtr must be created
 * with a final_layout of `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` or there will
 * be validation errors.
 *
 * Expected layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Scratch images: Any (as per the @ref render_gfx_render_pass)
 * - Target image: Any (as per the @ref render_gfx_render_pass)
 *
 * After call layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Scratch images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target image: What the render pass of @p rtr specifies.
 *
 * @note Swapchains in the @p layers must implement @ref comp_swapchain in
 * addition to just @ref xrt_swapchain, as this function downcasts to @ref comp_swapchain !
 *
 * @param render GFX render object
 * @param[in] layers Layers to render, see note.
 * @param[in] layer_count Number of elements in @p layers array.
 * @param[in] d Common render dispatch data
 */
void
comp_render_gfx_dispatch(struct render_gfx *render,
                         const struct comp_layer *layers,
                         const uint32_t layer_count,
                         const struct comp_render_dispatch_data *d);

/* end of comp_render_gfx group */

/*! @} */

void
comp_render_png_dispatch(struct render_gfx *render,
			const struct comp_layer *layers,
			const uint32_t layer_count,
			struct render_png_render_pass *render_pass);


/*
 *
 * CS functions.
 *
 */

/*!
 *
 * @defgroup comp_render_cs
 *
 * CS renderer control and dispatch - uses compute shaders
 *
 * Depends on @ref render_compute
 *
 * @ingroup comp_render
 * @{
 */

/*!
 * Add the target info, as required by the CS renderer.
 *
 * @param[in,out] data Common render dispatch data.
 * @param target_image Image to render into
 * @param target_storage_view Corresponding image view
 */
static inline void
comp_render_cs_add_target(struct comp_render_dispatch_data *data,
                          VkImage target_image,
                          VkImageView target_storage_view,
                          VkImageLayout target_final_layout)
{
	// Error tracking.
	data->target.initialized = true;

	// When writing into the target.
	data->target.cs.image = target_image;
	data->target.cs.storage_view = target_storage_view;
	data->target.cs.final_layout = target_final_layout;
}

/*!
 * Add view to the common data, as required by the CS renderer.
 *
 * @param[in,out] data Common render dispatch data, will be updated
 * @param world_pose_scanout_begin New world pose of this view.
 *        Populates @ref comp_render_view_data::world_pose
 * @param world_pose_scanout_end New world pose of this view.
 *        Populates @ref comp_render_view_data::world_pose
 * @param eye_pose New eye pose of this view
 *        Populates @ref comp_render_view_data::eye_pose_scanout_end
 * @param fov Assigned to fov in the view data, and used to compute @ref comp_render_view_data::pre_transform.
 *        Populates @ref comp_render_view_data::fov
 * @param squash_image Scratch image for this view
 *        Populates @ref comp_render_view_data::squash::image
 * @param squash_storage_view Image view into the scratch image for storage, CS specific
 * @param squash_viewport_data Where in the image to render the view
 *        Populates @ref comp_render_view_data::squash::viewport_data
 * @param squash_as_src_sample_view The image view into the scratch image for sampling.
 *        Populates @ref comp_render_view_data::squash_as_src::sample_view
 * @param squash_as_src_norm_rect How to transform when sampling from the scratch image.
 *        Populates @ref comp_render_view_data::squash_as_src::norm_rect
 * @param target_viewport_data Distortion target viewport data (aka target)
 *        Populates @ref comp_render_view_data::target::viewport_data
 */
static inline void
comp_render_cs_add_squash_view(struct comp_render_dispatch_data *data,
                               const struct xrt_pose *world_pose_scanout_begin,
                               const struct xrt_pose *world_pose_scanout_end,
                               const struct xrt_pose *eye_pose,
                               const struct xrt_fov *fov,
                               VkImage squash_image,
                               VkImageView squash_storage_view,
                               const struct render_viewport_data *squash_viewport_data)
{
	struct comp_render_view_data *view = comp_render_dispatch_add_squash_view( //
	    data,                                                                  //
	    world_pose_scanout_begin,                                              //
	    world_pose_scanout_end,                                                //
	    eye_pose,                                                              //
	    fov,                                                                   //
	    squash_image,                                                          //
	    squash_viewport_data);                                                 //

	// When writing into the squash (aka scratch) image.
	view->squash.cs.storage_view = squash_storage_view;
}

static inline void
comp_render_cs_add_target_view(struct comp_render_dispatch_data *data,
                               VkImageView squash_as_src_sample_view,
                               const struct xrt_normalized_rect *squash_as_src_norm_rect,
                               const struct render_viewport_data *target_viewport_data,
                               const render_scissor_data_t *target_scissor_data)
{
	struct comp_render_view_data *view = comp_render_dispatch_add_target_view( //
	    data,                                                                  //
	    squash_as_src_sample_view,                                             //
	    squash_as_src_norm_rect,                                               //
	    target_viewport_data,                                                  //
	    target_scissor_data);                                                  //
	(void)view;
}

/*!
 * Dispatch the layer squasher for a single view.
 *
 * All source layer images and target image needs to be in the correct image
 * layout, no barrier is inserted at all. The @p view_index argument is needed
 * to grab a pre-allocated UBO from the @ref render_resources and to correctly
 * select left/right data from various layers.
 *
 * Expected layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target images: `VK_IMAGE_LAYOUT_GENERAL`
 *
 * @note Swapchains in the @p layers must implement @ref comp_swapchain in
 * addition to just @ref xrt_swapchain, as this function downcasts to @ref comp_swapchain !
 *
 * @param render Compute renderer object
 * @param view_index Index of the view
 * @param layers Layers to render, see note.
 * @param layer_count Number of elements in @p layers array.
 * @param pre_transform
 * @param world_pose
 * @param eye_pose
 * @param target_image
 * @param target_image_view
 * @param target_view
 * @param do_timewarp
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
                     bool do_timewarp);

/*!
 * Dispatch the layer squasher, on any number of views.
 *
 * All source layer images needs to be in the correct image layout, no barrier
 * is inserted for them. The target images are barriered from undefined to general
 * so they can be written to, then to the layout defined by @p transition_to.
 *
 * Expected layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target images: Any
 *
 * After call layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target images: @p transition_to
 *
 * @note Swapchains in the @p layers must implement @ref comp_swapchain in
 * addition to just @ref xrt_swapchain, as this function downcasts to @ref comp_swapchain !
 *
 * @param render Compute renderer object
 * @param[in] layers Layers to render, see note.
 * @param[in] layer_count Number of elements in @p layers array.
 * @param[in] d Common render dispatch data
 * @param[in] transition_to Desired image layout for target images
 */
void
comp_render_cs_layers(struct render_compute *render,
                      const struct comp_layer *layers,
                      const uint32_t layer_count,
                      const struct comp_render_dispatch_data *d,
                      VkImageLayout transition_to);

/*!
 * Write commands to @p render to do a full composition with distortion.
 *
 * Helper function that takes a set of layers, new device poses, a scratch
 * images and writes the needed commands to the @ref render_compute to do a full
 * composition with distortion. The scratch images are optionally used to squash
 * layers should it not be possible to do a fast_path. Will insert barriers to
 * change the scratch images and target images to the needed layout.
 *
 *
 * Expected layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Scratch images: Any
 * - Target image: Any
 *
 * After call layouts:
 *
 * - Layer images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Scratch images: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
 * - Target image: @ref comp_render_dispatch_data::target::cs::final_layout
 *
 * @note Swapchains in the @p layers must implement @ref comp_swapchain in
 * addition to just @ref xrt_swapchain, as this function downcasts to @ref comp_swapchain !
 *
 * @param render Compute renderer object
 * @param[in] layers Layers to render, see note.
 * @param[in] layer_count Number of elements in @p layers array.
 * @param[in] d Common render dispatch data
 *
 */
void
comp_render_cs_dispatch(struct render_compute *render,
                        const struct comp_layer *layers,
                        const uint32_t layer_count,
                        const struct comp_render_dispatch_data *d);

/* end of comp_render_cs group */
/*! @} */

#ifdef __cplusplus
}
#endif
