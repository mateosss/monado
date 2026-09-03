// Copyright 2019-2023, Collabora, Ltd.
// Copyright 2025, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  The NEW compositor rendering code header.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_render
 */

#pragma once

#include "xrt/xrt_compiler.h"
#include "xrt/xrt_defines.h"

#include "vk/vk_helpers.h"
#include "vk/vk_cmd_pool.h"

#include "shaders/render_shaders_interface.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup aux_render Compositor render code
 * @ingroup comp
 *
 * @brief Rendering helper that is used by the compositor to render.
 */

/*!
 * @addtogroup aux_render
 * @{
 */

/*
 *
 * Defines
 *
 */

/*!
 * The value `minUniformBufferOffsetAlignment` is defined by the Vulkan spec as
 * having a max value of 256. Use this value to safely figure out sizes and
 * alignment of UBO sub-allocation. It is also the max for 'nonCoherentAtomSize`
 * which if we need to do flushing is what we need to align UBOs to.
 *
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#limits-minmax
 */
#define RENDER_ALWAYS_SAFE_UBO_ALIGNMENT (256)

/*!
 * Max number of layers for layer squasher, can be different from
 * @ref XRT_MAX_LAYERS as the render module is separate from the compositor.
 * It has to match RENDER_MAX_LAYERS in the layer.comp shader.
 */
#define RENDER_MAX_LAYERS (128)

/*!
 * The maximum number samplers per view that can be used by the compute shader
 * for layer composition (layer.comp)
 */
#define RENDER_CS_MAX_SAMPLERS_PER_VIEW 2

/*!
 * Max number of images that can be given at a single time to the layer
 * squasher in a single dispatch.
 */
#define RENDER_MAX_IMAGES_SIZE (RENDER_MAX_LAYERS * RENDER_CS_MAX_SAMPLERS_PER_VIEW)

/*!
 * Maximum number of times that the layer squasher shader can run per
 * @ref render_compute. Since you run the layer squasher shader once per view
 * this is essentially the same as number of views. But if you were to do
 * two or more different compositions it is not the maximum number of views per
 * composition (which is this number divided by number of composition).
 */
#define RENDER_MAX_LAYER_RUNS_SIZE (XRT_MAX_VIEWS)
#define RENDER_MAX_LAYER_RUNS_COUNT(RENDER_RESOURCES) (RENDER_RESOURCES->view_count)

//! Distortion image dimension in pixels
#define RENDER_DISTORTION_IMAGE_DIMENSIONS (128)

//! How many distortion images we have, one for each channel (3 rgb) and per view.
#define RENDER_DISTORTION_IMAGES_SIZE (3 * XRT_MAX_VIEWS)
#define RENDER_DISTORTION_IMAGES_COUNT(RENDER_RESOURCES) (3 * RENDER_RESOURCES->view_count)

//! The binding that the layer projection and quad shader have their UBO on.
#define RENDER_BINDING_LAYER_SHARED_UBO 0

//! The binding that the shared layer fragment shader has its source on.
#define RENDER_BINDING_LAYER_SHARED_SRC 1

/*!
 * The maximum number samplers per view that can be used by the compute shader
 * for layer composition (layer.comp)
 */
#define RENDER_CS_MAX_SAMPLERS_PER_VIEW 2

/*
 *
 * Util functions.
 *
 */

/*!
 * Determines the maximum number of compositor layers supported based on Vulkan
 * device limits and the composition path being used.
 *
 * @param vk                 Vulkan bundle containing device properties
 * @param use_compute        True if using compute pipeline path, false for graphics
 * @param desired_max_layers Maximum layers requested by the compositor
 * @return                   Actual maximum layers supported, clamped by device limits (minimum 16)
 *
 */
uint32_t
render_max_layers_capable(const struct vk_bundle *vk, bool use_compute, uint32_t desired_max_layers);

/*!
 * Create a simplified projection matrix for timewarp.
 */
void
render_calc_time_warp_projection(const struct xrt_fov *fov, struct xrt_matrix_4x4 *result);

/*!
 * Calculates a timewarp matrix which takes in NDC coords and gives out results
 * in [-1, 1] space that needs a perspective divide.
 */
void
render_calc_time_warp_matrix(const struct xrt_pose *src_pose,
                             const struct xrt_fov *src_fov,
                             const struct xrt_pose *new_pose,
                             struct xrt_matrix_4x4 *matrix);

/*!
 * This function constructs a transformation in the form of a normalized rect
 * that lets you go from a UV coordinate on a projection plane to the a point on
 * the tangent plane. An example is that the UV coordinate `(0, 0)` would be
 * transformed to `(tan(angle_left), tan(fov.angle_up))`. The tangent plane (aka
 * tangent space) is really the tangent of the angle, aka length at unit distance.
 *
 * For the trivial case of an fov with 45 degrees angles, that is where the
 * tangent length are `1` (aka `tan(45)`), the transformation would go from
 * `[0 .. 1]` to `[-1 .. 1]` the expected returns are `x = -1`, `y = -1`,
 * `w = 2` and `h = 2`.
 *
 * param      fov      The fov of the projection image.
 * param[out] out_rect Transformation from UV to tangent lengths.
 */
void
render_calc_uv_to_tangent_lengths_rect(const struct xrt_fov *fov, struct xrt_normalized_rect *out_rect);


/*
 *
 * Buffer
 *
 */

/*!
 * Helper struct holding a buffer and its memory.
 */
struct render_buffer
{
	//! Backing memory.
	VkDeviceMemory memory;

	//! Buffer.
	VkBuffer buffer;

	//! Size requested for the buffer.
	VkDeviceSize size;

	//! Size of the memory allocation.
	VkDeviceSize allocation_size;

	//! Alignment of the buffer.
	VkDeviceSize alignment;

	void *mapped;
};

/*!
 * Initialize a buffer.
 */
VkResult
render_buffer_init(struct vk_bundle *vk,
                   struct render_buffer *buffer,
                   VkBufferUsageFlags usage_flags,
                   VkMemoryPropertyFlags memory_property_flags,
                   VkDeviceSize size);

/*!
 * Initialize a buffer, making it exportable.
 */
VkResult
render_buffer_init_exportable(struct vk_bundle *vk,
                              struct render_buffer *buffer,
                              VkBufferUsageFlags usage_flags,
                              VkMemoryPropertyFlags memory_property_flags,
                              VkDeviceSize size);

/*!
 * Frees all resources that this buffer has, but does not free the buffer itself.
 */
void
render_buffer_fini(struct vk_bundle *vk, struct render_buffer *buffer);

/*!
 * Maps the memory, sets render_buffer::mapped to the memory.
 */
VkResult
render_buffer_map(struct vk_bundle *vk, struct render_buffer *buffer);

/*!
 * Unmaps the memory.
 */
void
render_buffer_unmap(struct vk_bundle *vk, struct render_buffer *buffer);

/*!
 * Maps the buffer, and copies the given data to the buffer.
 */
VkResult
render_buffer_map_and_write(struct vk_bundle *vk, struct render_buffer *buffer, void *data, VkDeviceSize size);

/*!
 * Writes the given data to the buffer, will map it temporarily if not mapped.
 */
VkResult
render_buffer_write(struct vk_bundle *vk, struct render_buffer *buffer, void *data, VkDeviceSize size);


/*
 *
 * Sub-alloc.
 *
 */

/*!
 * Per frame sub-allocation into a buffer, used to reduce the number of UBO
 * objects we need to create. There is no way to free a sub-allocation, this is
 * done implicitly at the end of the frame when @ref render_sub_alloc_tracker is
 * zeroed out.
 *
 * @see render_sub_alloc_tracker
 */
struct render_sub_alloc
{
	/*!
	 * The buffer this is allocated from, it is the caller's responsibility
	 * to keep it alive for as long as the sub-allocation is used.
	 */
	VkBuffer buffer;

	//! Size of sub-allocation.
	VkDeviceSize size;

	//! Offset into buffer.
	VkDeviceSize offset;
};

/*!
 * A per-frame tracker of sub-allocation out of a buffer, used to reduce the
 * number of UBO objects we need to create. This code is designed with one
 * constraint in mind, that the lifetime of a sub-allocation is only for one
 * frame and is discarded at the end of it, but also alive for the entire frame.
 * This removes the need to free individual sub-allocation, or even track them
 * beyond filling the UBO data and descriptor sets.
 *
 * @see render_sub_alloc
 */
struct render_sub_alloc_tracker
{
	/*!
	 * The buffer to allocate from, it is the caller's responsibility to keep
	 * it alive for as long as the sub-allocations are in used.
	 */
	VkBuffer buffer;

	//! Start of memory, if buffer was mapped with initialised.
	void *mapped;

	//! Total size of buffer.
	VkDeviceSize total_size;

	//! Currently used memory.
	VkDeviceSize used;
};

/*!
 * Init a @ref render_sub_alloc_tracker struct from a @ref render_buffer, the
 * caller is responsible for keeping @p buffer alive while the sub allocator
 * is being used.
 */
void
render_sub_alloc_tracker_init(struct render_sub_alloc_tracker *rsat, struct render_buffer *buffer);

/*!
 * Allocate enough memory (with constraints of UBOs) of @p size, return the
 * pointer to the mapped memory or null if the buffer wasn't allocated.
 */
XRT_CHECK_RESULT VkResult
render_sub_alloc_ubo_alloc_and_get_ptr(struct vk_bundle *vk,
                                       struct render_sub_alloc_tracker *rsat,
                                       VkDeviceSize size,
                                       void **out_ptr,
                                       struct render_sub_alloc *out_rsa);

/*!
 * Allocate enough memory (with constraints of UBOs) to hold the memory in @p ptr
 * and copy that memory to the buffer using the CPU.
 */
XRT_CHECK_RESULT VkResult
render_sub_alloc_ubo_alloc_and_write(struct vk_bundle *vk,
                                     struct render_sub_alloc_tracker *rsat,
                                     const void *ptr,
                                     VkDeviceSize size,
                                     struct render_sub_alloc *out_rsa);


/*
 *
 * Resources
 *
 */

/*!
 * Holds all pools and static resources for rendering.
 */
struct render_resources
{
	//! The count of views that we are rendering to.
	uint32_t view_count;

	//! Vulkan resources.
	struct vk_bundle *vk;

	/*
	 * Loaded resources.
	 */

	//! All shaders loaded.
	struct render_shaders *shaders;


	/*
	 * Shared pools and caches.
	 */

	//! Pool used for distortion image uploads.
	struct vk_cmd_pool distortion_pool;

	//! Shared for all rendering.
	VkPipelineCache pipeline_cache;

	VkCommandPool cmd_pool;

	VkQueryPool query_pool;


	/*
	 * Static
	 */

	//! Command buffer for recording everything.
	VkCommandBuffer cmd;

	struct
	{
		//! Sampler for mock/null images.
		VkSampler mock;

		//! Sampler that repeats the texture in all directions.
		VkSampler repeat;

		//! Sampler that clamps the coordinates to the edge in all directions.
		VkSampler clamp_to_edge;

		//! Sampler that clamps color samples to black in all directions.
		VkSampler clamp_to_border_black;
	} samplers;

	struct
	{
		//! Pool for shaders that uses one ubo and sampler.
		VkDescriptorPool ubo_and_src_descriptor_pool;

		/*!
		 * Shared UBO buffer that we sub-allocate out of, this is to
		 * have fewer buffers that the kernel needs to validate on
		 * command submission time.
		 *
		 * https://registry.khronos.org/vulkan/site/guide/latest/memory_allocation.html
		 */
		struct render_buffer shared_ubo;

		struct
		{
			struct
			{
				//! For projection and quad layer.
				VkDescriptorSetLayout descriptor_set_layout;

				//! For projection and quad layer.
				VkPipelineLayout pipeline_layout;
			} shared;
		} layer;
	} gfx;

	struct
	{
		//! The binding index for the source texture.
		uint32_t src_binding;

		//! The binding index for the UBO.
		uint32_t ubo_binding;

		//! Descriptor set layout for mesh distortion.
		VkDescriptorSetLayout descriptor_set_layout;

		//! Pipeline layout used for mesh.
		VkPipelineLayout pipeline_layout;

		struct render_buffer vbo;
		struct render_buffer ibo;

		uint32_t vertex_count;
		uint32_t index_counts[XRT_MAX_VIEWS];
		uint32_t stride;
		uint32_t index_offsets[XRT_MAX_VIEWS];
		uint32_t index_count_total;

		//! Info UBOs.
		struct render_buffer ubos[XRT_MAX_VIEWS];
	} mesh;

	/*!
	 * Used as a default image empty image when none is given or to pad
	 * out fixed sized descriptor sets.
	 */
	struct
	{
		struct
		{
			VkImage image;
			VkImageView image_view;
			VkDeviceMemory memory;
		} color;
	} mock;

	struct
	{
		//! Descriptor pool for compute work.
		VkDescriptorPool descriptor_pool;

		//! The source projection view binding point.
		uint32_t src_binding;

		//! Image storing the distortion.
		uint32_t distortion_binding;

		//! Writing the image out too.
		uint32_t target_binding;

		//! Uniform data binding.
		uint32_t ubo_binding;

		struct
		{
			//! Descriptor set layout for compute.
			VkDescriptorSetLayout descriptor_set_layout;

			//! Pipeline layout used for compute distortion.
			VkPipelineLayout pipeline_layout;

			//! Doesn't depend on target so is static.
			VkPipeline non_timewarp_pipeline;

			//! Doesn't depend on target so is static.
			VkPipeline timewarp_pipeline;

			//! Size of combined image sampler array
			uint32_t image_array_size;

			//! Target info.
			struct render_buffer ubos[RENDER_MAX_LAYER_RUNS_SIZE];
		} layer;

		struct
		{
			//! Descriptor set layout for compute distortion.
			VkDescriptorSetLayout descriptor_set_layout;

			//! Pipeline layout used for compute distortion, shared with clear.
			VkPipelineLayout pipeline_layout;

			//! Doesn't depend on target so is static.
			VkPipeline pipeline;

			//! Doesn't depend on target so is static.
			VkPipeline timewarp_pipeline;

			//! Target info.
			struct render_buffer ubo;
		} distortion;

		struct
		{
			//! Doesn't depend on target so is static.
			VkPipeline pipeline;

			//! Target info.
			struct render_buffer ubo;

			//! @todo other resources
		} clear;
	} compute;

	struct
	{
		//! Transform to go from UV to tangle angles.
		struct xrt_normalized_rect uv_to_tanangle[XRT_MAX_VIEWS];

		//! Backing memory to distortion images.
		VkDeviceMemory device_memories[RENDER_DISTORTION_IMAGES_SIZE];

		//! Distortion images.
		VkImage images[RENDER_DISTORTION_IMAGES_SIZE];

		//! The views into the distortion images.
		VkImageView image_views[RENDER_DISTORTION_IMAGES_SIZE];

		//! Whether distortion images have been pre-rotated 90 degrees.
		bool pre_rotated;
	} distortion;

		struct
	{
		struct
		{
			VkImage image;
			VkImageView image_view;
			VkDeviceMemory memory;
		} color;

		VkCommandPool cmd_pool;

		VkCommandBuffer cmd;

		//! The binding index for the source texture.
		uint32_t src_binding;

		//! The binding index for the UBO.
		uint32_t ubo_binding;

		VkDescriptorPool ubo_and_src_descriptor_pool;

		VkDescriptorSetLayout descriptor_set_layout;

		VkPipelineLayout pipeline_layout;

		VkPipeline pipeline;

		struct render_buffer vbo;
		struct render_buffer ibo;
		
		float* vertices;
		uint32_t vertex_count;
		uint32_t stride;

		uint16_t *indices;
		//! Number of indices for the triangle strips (one per view).
		uint32_t index_count;

		//! Info UBOs.
		struct render_buffer ubos[XRT_MAX_VIEWS];
	} png;
};

/*!
 * Allocate pools and static resources.
 *
 * @ingroup comp_main
 *
 * @public @memberof render_resources
 */
bool
render_resources_init(struct render_resources *r,
                      struct render_shaders *shaders,
                      struct vk_bundle *vk,
                      struct xrt_device *xdev);

/*!
 * Free all pools and static resources, does not free the struct itself.
 *
 * @public @memberof render_resources
 */
void
render_resources_fini(struct render_resources *r);

/*!
 * Creates or recreates the compute distortion textures if necessary.
 *
 * @see render_distortion_images_fini
 * @public @memberof render_resources
 */
bool
render_distortion_images_ensure(struct render_resources *r,
                                struct vk_bundle *vk,
                                struct xrt_device *xdev,
                                bool pre_rotate);

/*!
 * Free distortion images.
 *
 * @see render_distortion_images_ensure
 * @public @memberof render_resources
 */
void
render_distortion_images_fini(struct render_resources *r);

/*!
 * Returns the timestamps for when the latest GPU work started and stopped that
 * was submitted using @ref render_gfx or @ref render_compute cmd buf builders.
 *
 * Returned in the same time domain as returned by @ref os_monotonic_get_ns .
 * Behaviour for this function is undefined if the GPU has not completed before
 * calling this function, so make sure to call vkQueueWaitIdle or wait on the
 * fence that the work was submitted with have fully completed. See other
 * limitation mentioned for @ref vk_convert_timestamps_to_host_ns .
 *
 * @see vk_convert_timestamps_to_host_ns
 *
 * @public @memberof render_resources
 */
bool
render_resources_get_timestamps(struct render_resources *r, uint64_t *out_gpu_start_ns, uint64_t *out_gpu_end_ns);

/*!
 * Returns the duration for the latest GPU work that was submitted using
 * @ref render_gfx or @ref render_compute cmd buf builders.
 *
 * Behaviour for this function is undefined if the GPU has not completed before
 * calling this function, so make sure to call vkQueueWaitIdle or wait on the
 * fence that the work was submitted with have fully completed.
 *
 * @public @memberof render_resources
 */
bool
render_resources_get_duration(struct render_resources *r, uint64_t *out_gpu_duration_ns);


/*
 *
 * Scratch images.
 *
 */

/*!
 * Small helper struct to hold a scratch image, intended to be used with the
 * compute pipeline where both srgb and unorm views are needed.
 */
struct render_scratch_color_image
{
	VkDeviceMemory device_memory;
	VkImage image;
	VkImageView srgb_view;
	VkImageView unorm_view;
};

/*!
 * Helper struct to hold scratch images.
 */
struct render_scratch_images
{
	VkExtent2D extent;

	struct render_scratch_color_image color[XRT_MAX_VIEWS];
};

/*!
 * Ensure that the scratch images are created and have the given extent.
 *
 * @public @memberof render_scratch_images
 */
bool
render_scratch_images_ensure(struct render_resources *r, struct render_scratch_images *rsi, VkExtent2D extent);

/*!
 * Close all resources on the given @ref render_scratch_images.
 *
 * @public @memberof render_scratch_images
 */
void
render_scratch_images_fini(struct render_resources *r, struct render_scratch_images *rsi);


/*
 *
 * Shared between both gfx and compute.
 *
 */

/*!
 *  The pure data information about a view that the renderer is rendering to.
 */
struct render_viewport_data
{
	uint32_t x, y;
	uint32_t w, h;
};

typedef struct render_viewport_data render_scissor_data_t;

/*
 *
 * Render pass
 *
 */

/*!
 * A render pass, while not depending on a @p VkFramebuffer, does depend on the
 * format of the target image(s), and other options for the render pass. These
 * are used to create a @p VkRenderPass, all @p VkFramebuffer(s) and
 * @p VkPipeline depends on the @p VkRenderPass so hang off this struct.
 */
struct render_gfx_render_pass
{
	struct render_resources *r;

	//! The format of the image(s) we are rendering to.
	VkFormat format;

	//! Sample count for this render pass.
	VkSampleCountFlagBits sample_count;

	//! Load op used on the attachment(s).
	VkAttachmentLoadOp load_op;

	//! Final layout of the target image(s).
	VkImageLayout final_layout;

	//! Render pass used for rendering.
	VkRenderPass render_pass;

	struct
	{
		//! Pipeline layout used for mesh, without timewarp.
		VkPipeline pipeline;

		//! Pipeline layout used for mesh, with timewarp.
		VkPipeline pipeline_timewarp;
	} mesh;

	struct
	{
		VkPipeline cylinder_premultiplied_alpha;
		VkPipeline cylinder_unpremultiplied_alpha;

		VkPipeline equirect2_premultiplied_alpha;
		VkPipeline equirect2_unpremultiplied_alpha;

		VkPipeline proj_premultiplied_alpha;
		VkPipeline proj_unpremultiplied_alpha;

		VkPipeline quad_premultiplied_alpha;
		VkPipeline quad_unpremultiplied_alpha;
	} layer;
};

/*!
 * Creates all resources held by the render pass.
 *
 * @public @memberof render_gfx_render_pass
 */
bool
render_gfx_render_pass_init(struct render_gfx_render_pass *rgrp,
                            struct render_resources *r,
                            VkFormat format,
                            VkAttachmentLoadOp load_op,
                            VkImageLayout final_layout);

/*!
 * Frees all resources held by the render pass, does not free the struct itself.
 *
 * @public @memberof render_gfx_render_pass
 */
void
render_gfx_render_pass_fini(struct render_gfx_render_pass *rgrp);


struct render_png_render_pass
{
	struct render_resources *r;

	//! The format of the image(s) we are rendering to.
	VkFormat format;

	//! Sample count for this render pass.
	VkSampleCountFlagBits sample_count;

	//! Load op used on the attachment(s).
	VkAttachmentLoadOp load_op;

	//! Final layout of the target image(s).
	VkImageLayout final_layout;

	//! Render pass used for rendering.
	VkRenderPass render_pass;

	VkPipeline pipeline;

};

bool
render_png_render_pass_init(struct render_png_render_pass *rprp,
                            struct render_resources *r,
                            VkFormat format,
                            VkAttachmentLoadOp load_op,
                            VkImageLayout final_layout);

void
render_png_render_pass_fini(struct render_png_render_pass *rprp);

/*
 *
 * Rendering target
 *
 */

/*!
 * Each rendering (@ref render_gfx) render to one or more targets
 * (@ref render_gfx_target_resources), the target points to one render pass and
 * its pipelines (@ref render_gfx_render_pass). It is up to the code using
 * these to do reuse of render passes and ensure they match.
 *
 * @see comp_render_gfx
 */
struct render_gfx_target_resources
{
	//! Collections of static resources.
	struct render_resources *r;

	//! Render pass.
	struct render_gfx_render_pass *rgrp;
	struct render_png_render_pass *rprp;

	//! The offset & extents of the framebuffer.
	VkRect2D render_area;

	//! Framebuffer for this target, depends on given VkImageView.
	VkFramebuffer framebuffer;
};

/*!
 * Init a target resource struct, caller has to keep target alive until closed.
 *
 * @public @memberof render_gfx_target_resources
 */
bool
render_gfx_target_resources_init(struct render_gfx_target_resources *rtr,
                                 struct render_resources *r,
                                 struct render_gfx_render_pass *rgrp,
                                 VkImageView target,
                                 VkExtent2D extent);

bool
render_png_target_resources_init(struct render_gfx_target_resources *rtr,
                                 struct render_resources *r,
                                 struct render_png_render_pass *rgrp,
                                 VkImageView target,
                                 struct render_viewport_data viewport_data);

/*!
 * Frees all resources held by the target, does not free the struct itself.
 *
 * @public @memberof render_gfx_target_resources
 */
void
render_gfx_target_resources_fini(struct render_gfx_target_resources *rtr);


/*
 *
 * Rendering
 *
 */

/*!
 * The low-level resources and operations to perform layer squashing and/or
 * mesh distortion for a single frame using graphics shaders.
 *
 * It uses a two-stage process to render a frame. This means
 * consumers iterate layers (or other operations) **twice**, within each target and view.
 * There is a preparation stage, where the uniform buffer is sub-allocated and written.
 * This must be completed for all layers before the actual draw stage begins.
 * The second stage is recording the draw commands into a command buffer.
 *
 * You must make equivalent calls in the same order between the two stages. The second stage
 * additionally has @ref render_gfx_begin_target, @ref render_gfx_end_target,
 * @ref render_gfx_begin_view, and @ref render_gfx_end_view lacked by the first stage,
 * but if you exclude those functions, the others must line up.
 *
 * Furthermore, the struct needs to be kept alive until the work has been waited on,
 * or you get validation warnings. Either wait on the `VkFence` for the submit, or call
 * `vkDeviceWaitIdle`/`vkQueueWaitIdle` on the device/queue.
 *
 * @see comp_render_gfx
 */
struct render_gfx
{
	//! Resources that we are based on.
	struct render_resources *r;

	//! Shared buffer that we sub-allocate UBOs from.
	struct render_sub_alloc_tracker ubo_tracker;

	//! The current target we are rendering to, can change during command building.
	struct render_gfx_target_resources *rtr;
};

/*!
 * Init struct and create resources needed for rendering.
 *
 * @public @memberof render_gfx
 */
bool
render_gfx_init(struct render_gfx *render, struct render_resources *r);

/*!
 * Begins the rendering, takes the vk_bundle's pool lock and leaves it locked.
 *
 * @public @memberof render_gfx
 */
bool
render_gfx_begin(struct render_gfx *render);

/*!
 * Frees any unneeded resources and ends the command buffer so it can be used,
 * also unlocks the vk_bundle's pool lock that was taken by begin.
 *
 * @public @memberof render_gfx
 */
bool
render_gfx_end(struct render_gfx *render);

struct render_png_ubo_data
{
	struct xrt_matrix_4x4 mvp;
};

bool
render_png_begin(struct render_gfx *render);

bool
render_png_end(struct render_gfx *render);

/*!
 * Frees all resources held by the rendering, does not free the struct itself.
 *
 * @public @memberof render_gfx
 */
void
render_gfx_fini(struct render_gfx *render);


/*
 *
 * Drawing
 *
 */

/*!
 * UBO data that is sent to the mesh shaders.
 *
 * @relates render_gfx
 */
struct render_gfx_mesh_ubo_data
{
	struct xrt_matrix_2x2 vertex_rot;
	struct xrt_normalized_rect post_transform;

	// Only used for timewarp.
	struct xrt_normalized_rect pre_transform;
	struct xrt_matrix_4x4 transform_scanout_begin;
	struct xrt_matrix_4x4 transform_scanout_end;
};

/*!
 * UBO data that is sent to the layer cylinder shader.
 *
 * @relates render_gfx
 */
struct render_gfx_layer_cylinder_data
{
	struct xrt_normalized_rect post_transform;
	struct xrt_matrix_4x4 mvp;
	float radius;
	float central_angle;
	float aspect_ratio;
	float _pad;
	struct xrt_colour_rgba_f32 color_scale;
	struct xrt_colour_rgba_f32 color_bias;
};

/*!
 * UBO data that is sent to the layer equirect2 shader.
 *
 * @relates render_gfx
 */
struct render_gfx_layer_equirect2_data
{
	struct xrt_normalized_rect post_transform;
	struct xrt_matrix_4x4 mv_inverse;

	//! See @ref render_calc_uv_to_tangent_lengths_rect.
	struct xrt_normalized_rect to_tangent;

	float radius;
	float central_horizontal_angle;
	float upper_vertical_angle;
	float lower_vertical_angle;
	struct xrt_colour_rgba_f32 color_scale;
	struct xrt_colour_rgba_f32 color_bias;
};

/*!
 * UBO data that is sent to the layer projection shader.
 *
 * @relates render_gfx
 */
struct render_gfx_layer_projection_data
{
	struct xrt_normalized_rect post_transform;
	struct xrt_normalized_rect to_tangent;
	struct xrt_matrix_4x4 mvp;
	struct xrt_colour_rgba_f32 color_scale;
	struct xrt_colour_rgba_f32 color_bias;
};

/*!
 * UBO data that is sent to the layer quad shader.
 *
 * @relates render_gfx
 */
struct render_gfx_layer_quad_data
{
	struct xrt_normalized_rect post_transform;
	struct xrt_matrix_4x4 mvp;
	struct xrt_colour_rgba_f32 color_scale;
	struct xrt_colour_rgba_f32 color_bias;
};

/*!
 * @name Preparation functions - first stage
 * @{
 */

/*!
 * Allocate needed resources for one mesh shader dispatch, will also update the
 * descriptor set, UBO will be filled out with the given @p data argument.
 *
 * Uses the @ref render_sub_alloc_tracker of the @ref render_gfx and the
 * descriptor pool of @ref render_resources, both of which will be reset once
 * closed, so don't save any reference to these objects beyond the frame.
 *
 * @public @memberof render_gfx
 */
XRT_CHECK_RESULT VkResult
render_gfx_mesh_alloc_and_write(struct render_gfx *render,
                                const struct render_gfx_mesh_ubo_data *data,
                                VkSampler src_sampler,
                                VkImageView src_image_view,
                                VkDescriptorSet *out_descriptor_set);

/*!
 * Allocate and write a UBO and descriptor_set to be used for cylinder layer
 * rendering, the content of @p data need to be valid at the time of the call.
 *
 * @public @memberof render_gfx
 */
XRT_CHECK_RESULT VkResult
render_gfx_layer_cylinder_alloc_and_write(struct render_gfx *render,
                                          const struct render_gfx_layer_cylinder_data *data,
                                          VkSampler src_sampler,
                                          VkImageView src_image_view,
                                          VkDescriptorSet *out_descriptor_set);

/*!
 * Allocate and write a UBO and descriptor_set to be used for equirect2 layer
 * rendering, the content of @p data need to be valid at the time of the call.
 *
 * @public @memberof render_gfx
 */
XRT_CHECK_RESULT VkResult
render_gfx_layer_equirect2_alloc_and_write(struct render_gfx *render,
                                           const struct render_gfx_layer_equirect2_data *data,
                                           VkSampler src_sampler,
                                           VkImageView src_image_view,
                                           VkDescriptorSet *out_descriptor_set);

/*!
 * Allocate and write a UBO and descriptor_set to be used for projection layer
 * rendering, the content of @p data need to be valid at the time of the call.
 *
 * @public @memberof render_gfx
 */
XRT_CHECK_RESULT VkResult
render_gfx_layer_projection_alloc_and_write(struct render_gfx *render,
                                            const struct render_gfx_layer_projection_data *data,
                                            VkSampler src_sampler,
                                            VkImageView src_image_view,
                                            VkDescriptorSet *out_descriptor_set);

/*!
 * Allocate and write a UBO and descriptor_set to be used for quad layer
 * rendering, the content of @p data need to be valid at the time of the call.
 *
 * @public @memberof render_gfx
 */
XRT_CHECK_RESULT VkResult
render_gfx_layer_quad_alloc_and_write(struct render_gfx *render,
                                      const struct render_gfx_layer_quad_data *data,
                                      VkSampler src_sampler,
                                      VkImageView src_image_view,
                                      VkDescriptorSet *out_descriptor_set);


/*!
 * @}
 */

/*!
 * @name Drawing functions - second stage
 * @{
 */

/*!
 * This function allocates everything to start a single rendering. This is the
 * first function you call when you start the drawiing stage, you follow up with a call
 * to @ref render_gfx_begin_view.
 *
 * @public @memberof render_gfx
 */
bool
render_gfx_begin_target(struct render_gfx *render,
                        struct render_gfx_target_resources *rtr,
                        const VkClearColorValue *color);

/*!
 * @pre successful @ref render_gfx_begin_target call,
 *   no @ref render_gfx_begin_view without matching @ref render_gfx_end_view
 * @public @memberof render_gfx
 */
void
render_gfx_end_target(struct render_gfx *render);

/*!
 * @pre successful @ref render_gfx_begin_target call
 * @public @memberof render_gfx
 */
void
render_gfx_clear_color_attachment(struct render_gfx *render, const VkClearColorValue *color);

/*!
 * @pre successful @ref render_gfx_begin_target call
 * @public @memberof render_gfx
 */
void
render_gfx_begin_view(struct render_gfx *render,
                      uint32_t view,
                      const struct render_viewport_data *viewport_data,
                      const render_scissor_data_t *scissor_data);

/*!
 * @pre successful @ref render_gfx_begin_view call without a matching call to this function
 * @public @memberof render_gfx
 */
void
render_gfx_end_view(struct render_gfx *render);

/*!
 * Dispatch one mesh shader instance, using the give @p mesh_index as source for
 * mesh geometry, timewarp selectable via @p do_timewarp.
 *
 * Must have successfully called @ref render_gfx_mesh_alloc_and_write
 * before @ref render_gfx_begin_target to allocate @p descriptor_set and UBO.
 *
 * @pre successful @ref render_gfx_mesh_alloc_and_write call, successful @ref render_gfx_begin_view call
 * @public @memberof render_gfx
 */
void
render_gfx_mesh_draw(struct render_gfx *render, uint32_t mesh_index, VkDescriptorSet descriptor_set, bool do_timewarp);

bool
render_png_begin_target(struct render_gfx *render,
                        struct render_gfx_target_resources *rtr,
                        const VkClearColorValue *color);

void
render_png_end_target(struct render_gfx *render);

void
render_png_begin_view(struct render_gfx *render,
                      uint32_t view,
                      const struct render_viewport_data *viewport_data,
                      const render_scissor_data_t *scissor_data);

void
render_png_end_view(struct render_gfx *render);

void
render_png_draw(struct render_gfx *render, VkDescriptorSet descriptor_set);

XRT_CHECK_RESULT VkResult
render_png_alloc_and_write(struct render_gfx *render,
                                const struct render_png_ubo_data *data,
                                VkSampler src_sampler,
                                VkImageView src_image_view,
                                VkDescriptorSet *out_descriptor_set);

/*!
 * Dispatch a cylinder layer shader into the current target and view.
 *
 * Must have successfully called @ref render_gfx_layer_cylinder_alloc_and_write
 * before @ref render_gfx_begin_target to allocate @p descriptor_set and UBO.
 *
 * @public @memberof render_gfx
 */
void
render_gfx_layer_cylinder(struct render_gfx *render, bool premultiplied_alpha, VkDescriptorSet descriptor_set);

/*!
 * Dispatch a equirect2 layer shader into the current target and view.
 *
 * Must have successfully called @ref render_gfx_layer_equirect2_alloc_and_write
 * before @ref render_gfx_begin_target to allocate @p descriptor_set and UBO.
 *
 * @public @memberof render_gfx
 */
void
render_gfx_layer_equirect2(struct render_gfx *render, bool premultiplied_alpha, VkDescriptorSet descriptor_set);

/*!
 * Dispatch a projection layer shader into the current target and view.
 *
 * Must have successfully called @ref render_gfx_layer_projection_alloc_and_write
 * before @ref render_gfx_begin_target to allocate @p descriptor_set and UBO.
 *
 * @public @memberof render_gfx
 */
void
render_gfx_layer_projection(struct render_gfx *render, bool premultiplied_alpha, VkDescriptorSet descriptor_set);

/*!
 * Dispatch a quad layer shader into the current target and view.
 *
 * Must have successfully called @ref render_gfx_layer_quad_alloc_and_write
 * before @ref render_gfx_begin_target to allocate @p descriptor_set and UBO.
 *
 * @public @memberof render_gfx
 */
void
render_gfx_layer_quad(struct render_gfx *render, bool premultiplied_alpha, VkDescriptorSet descriptor_set);

/*!
 * @}
 */


/*
 *
 * Compute distortion.
 *
 */

/*!
 * The semi-low level resources and operations required to squash layers and/or
 * apply distortion for a single frame using compute shaders.
 *
 * Unlike @ref render_gfx, this is a single stage process, and you pass all layers at a single time.
 *
 * @see comp_render_cs
 */
struct render_compute
{
	//! Shared resources.
	struct render_resources *r;

	//! Layer descriptor set.
	VkDescriptorSet layer_descriptor_sets[RENDER_MAX_LAYER_RUNS_SIZE];

	/*!
	 * Shared descriptor set, used for the clear and distortion shaders. It
	 * is used in the functions @ref render_compute_projection_timewarp,
	 * @ref render_compute_projection, and @ref render_compute_clear.
	 */
	VkDescriptorSet shared_descriptor_set;
};

/*!
 * Push data that is sent to the blit shader.
 *
 * @relates render_compute
 */
struct render_compute_blit_push_data
{
	struct xrt_normalized_rect source_rect;
	struct xrt_rect target_rect;
};

/*!
 * Color mode for blit/resolve operations, used to select the correct shader variant for the source and target
 * image formats.
 *
 * @relates render_compute
 */
enum render_compute_blit_resolve_color_mode
{
	//! Source is UNORM/linear but the submitted bytes are gamma-encoded.
	RENDER_BLIT_RESOLVE_COLOR_MODE_GAMMA_IN_LINEAR_FORMAT = 1,
	//! Source is sRGB but the submitted bytes are already linear.
	RENDER_BLIT_RESOLVE_COLOR_MODE_LINEAR_IN_SRGB_FORMAT = 2,
	RENDER_BLIT_RESOLVE_COLOR_MODE_COUNT = 2,
};

/*!
 * Specialization constants for the blit shader, used to select the correct shader variant for the source and target
 * image formats.
 *
 * @relates render_compute
 */
struct render_compute_blit_specialization_constants
{
	//! See @ref render_compute_blit_resolve_color_mode.
	uint32_t color_transform_mode;
};

/*!
 * Chroma key parameters for std140 layout, using HSV min/max range.
 *
 * @relates render_compute
 */
struct render_chroma_key_info
{
	float hsv_min_h;
	float hsv_min_s;
	float hsv_min_v;
	float hsv_max_h;
	float hsv_max_s;
	float hsv_max_v;
	float curve;
	float despill;
};

/*!
 * UBO data that is sent to the compute layer shaders.
 *
 * @relates render_compute
 */
struct render_compute_layer_ubo_data
{
	struct render_viewport_data view;

	struct
	{
		uint32_t value;
		uint32_t padding0; // Padding up to a vec4.
		uint32_t padding1;
		uint32_t padding2;
	} layer_count;

	struct xrt_normalized_rect pre_transform;

	struct
	{
		struct xrt_normalized_rect post_transforms;

		/*!
		 * Corresponds to enum xrt_layer_type and unpremultiplied alpha.
		 *
		 * std140 uvec2, because it is an array it gets padded to vec4.
		 */
		struct
		{
			uint32_t layer_type;
			uint32_t unpremultiplied_alpha;
			uint32_t inverted_alpha;
			uint32_t _padding0;
		} layer_data;

		/*!
		 * Which image/sampler(s) correspond to each layer.
		 *
		 * std140 uvec2, because it is an array it gets padded to vec4.
		 */
		struct
		{
			uint32_t color_image_index;
			uint32_t depth_image_index;

			//! @todo Implement separated samplers and images (and change to samplers[2])
			uint32_t _padding0;
			uint32_t _padding1;
		} image_info;

		//! Shared between cylinder and equirect2.
		struct xrt_matrix_4x4 mv_inverse;


		/*!
		 * For cylinder layer
		 */
		struct
		{
			float radius;
			float central_angle;
			float aspect_ratio;
			float padding;
		} cylinder_data;


		/*!
		 * For equirect2 layers
		 */
		struct
		{
			float radius;
			float central_horizontal_angle;
			float upper_vertical_angle;
			float lower_vertical_angle;
		} eq2_data;


		/*!
		 * For projection layers
		 */

		//! Timewarp matrices
		struct xrt_matrix_4x4 transforms_timewarp;

		//! Chroma key parameters (per layer)
		struct render_chroma_key_info chroma_key;

		/*!
		 * For quad layers
		 */

		//! All quad transforms and coordinates are in view space
		struct
		{
			struct xrt_vec3 val;
			float padding;
		} quad_position;
		struct
		{
			struct xrt_vec3 val;
			float padding;
		} quad_normal;
		struct xrt_matrix_4x4 inverse_quad_transform;

		//! Quad extent in world scale
		struct
		{
			struct xrt_vec2 val;
			float padding0;
			float padding1;
		} quad_extent;

		/*!
		 * Color scale and bias for all layers
		 */
		struct xrt_colour_rgba_f32 color_scale;
		struct xrt_colour_rgba_f32 color_bias;
	} layers[RENDER_MAX_LAYERS];
};

/*!
 * UBO data that is sent to the compute distortion shaders.
 *
 * @relates render_compute
 */
struct render_compute_distortion_ubo_data
{
	struct render_viewport_data views[XRT_MAX_VIEWS];
	struct xrt_normalized_rect pre_transforms[XRT_MAX_VIEWS];
	struct xrt_normalized_rect post_transforms[XRT_MAX_VIEWS];
	struct xrt_matrix_4x4 transform_timewarp_scanout_begin[XRT_MAX_VIEWS];
	struct xrt_matrix_4x4 transform_timewarp_scanout_end[XRT_MAX_VIEWS];
};

/*!
 * Init struct and create resources needed for compute rendering.
 *
 * @public @memberof render_compute
 */
bool
render_compute_init(struct render_compute *render, struct render_resources *r);

/*!
 * Frees all resources held by the compute rendering, does not free the struct itself.
 *
 * @public @memberof render_compute
 */
void
render_compute_fini(struct render_compute *render);

/*!
 * Begin the compute command buffer building, takes the vk_bundle's pool lock
 * and leaves it locked.
 *
 * @public @memberof render_compute
 */
bool
render_compute_begin(struct render_compute *render);

/*!
 * Frees any unneeded resources and ends the command buffer so it can be used,
 * also unlocks the vk_bundle's pool lock that was taken by begin.
 *
 * @public @memberof render_compute
 */
bool
render_compute_end(struct render_compute *render);

/*!
 * Updates the given @p descriptor_set and dispatches the layer shader. Unlike
 * other dispatch functions below this function doesn't do any layer barriers
 * before or after dispatching, this is to allow the callee to batch any such
 * image transitions.
 *
 * Expected layouts:
 * * Source images: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 * * Target image: VK_IMAGE_LAYOUT_GENERAL
 *
 * @public @memberof render_compute
 */
void
render_compute_layers(struct render_compute *render,
                      VkDescriptorSet descriptor_set,
                      VkBuffer ubo,
                      VkSampler src_samplers[RENDER_MAX_IMAGES_SIZE],
                      VkImageView src_image_views[RENDER_MAX_IMAGES_SIZE],
                      uint32_t num_srcs,
                      VkImageView target_image_view,
                      const struct render_viewport_data *view,
                      bool timewarp);

/*!
 * @public @memberof render_compute
 */
void
render_compute_projection_timewarp(struct render_compute *render,
                                   VkSampler src_samplers[XRT_MAX_VIEWS],
                                   VkImageView src_image_views[XRT_MAX_VIEWS],
                                   const struct xrt_normalized_rect src_rects[XRT_MAX_VIEWS],
                                   const struct xrt_pose src_poses[XRT_MAX_VIEWS],
                                   const struct xrt_fov src_fovs[XRT_MAX_VIEWS],
                                   const struct xrt_pose new_poses_scanout_begin[XRT_MAX_VIEWS],
                                   const struct xrt_pose new_poses_scanout_end[XRT_MAX_VIEWS],
                                   VkImage target_image,
                                   VkImageView target_image_view,
                                   VkImageLayout target_final_layout,
                                   const struct render_viewport_data views[XRT_MAX_VIEWS]);

/*!
 * @public @memberof render_compute
 */
void
render_compute_projection_scanout_compensation(struct render_compute *render,
                                               VkSampler src_samplers[XRT_MAX_VIEWS],
                                               VkImageView src_image_views[XRT_MAX_VIEWS],
                                               const struct xrt_normalized_rect src_rects[XRT_MAX_VIEWS],
                                               const struct xrt_fov src_fovs[XRT_MAX_VIEWS],
                                               const struct xrt_pose new_poses_scanout_begin[XRT_MAX_VIEWS],
                                               const struct xrt_pose new_poses_scanout_end[XRT_MAX_VIEWS],
                                               VkImage target_image,
                                               VkImageView target_image_view,
                                               VkImageLayout target_final_layout,
                                               const struct render_viewport_data views[XRT_MAX_VIEWS]);

/*!
 * @public @memberof render_compute
 */
void
render_compute_projection_no_timewarp(struct render_compute *render,
                                      VkSampler src_samplers[XRT_MAX_VIEWS],
                                      VkImageView src_image_views[XRT_MAX_VIEWS],
                                      const struct xrt_normalized_rect src_rects[XRT_MAX_VIEWS],
                                      VkImage target_image,
                                      VkImageView target_image_view,
                                      VkImageLayout target_final_layout,
                                      const struct render_viewport_data views[XRT_MAX_VIEWS]);

/*!
 * @public @memberof render_compute
 */
void
render_compute_clear(struct render_compute *render,
                     VkImage target_image,
                     VkImageView target_image_view,
                     VkImageLayout target_final_layout,
                     const struct render_viewport_data views[XRT_MAX_VIEWS]);



/*!
 * @}
 */


#ifdef __cplusplus
}
#endif
