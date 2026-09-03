#include "vk/vk_mini_helpers.h"

#include "render/render_interface.h"

#include <stdio.h>

static inline struct vk_bundle *
vk_from_render(struct render_gfx *render)
{
	return render->r->vk;
}


XRT_CHECK_RESULT static VkResult
create_implicit_render_pass(struct vk_bundle *vk,
                            VkFormat format,
                            VkAttachmentLoadOp load_op,
                            VkImageLayout final_layout,
                            VkRenderPass *out_render_pass)
{
	VkResult ret;

	VkAttachmentDescription attachments[1] = {
	    {
	        .format = format,
	        .samples = VK_SAMPLE_COUNT_1_BIT,
	        .loadOp = load_op,
	        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	        .finalLayout = final_layout,
	        .flags = 0,
	    },
	};

	VkAttachmentReference color_reference = {
	    .attachment = 0,
	    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpasses[1] = {
	    {
	        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
	        .inputAttachmentCount = 0,
	        .pInputAttachments = NULL,
	        .colorAttachmentCount = 1,
	        .pColorAttachments = &color_reference,
	        .pResolveAttachments = NULL,
	        .pDepthStencilAttachment = NULL,
	        .preserveAttachmentCount = 0,
	        .pPreserveAttachments = NULL,
	    },
	};

	/*!
	 * Explicit subpass dependency required to synchronize the implicit layout
	 * transition at render pass begin with the swapchain acquire semaphore, which
	 * signals at VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT. Without this, the
	 * implicit dependency is not enough and results in a SYNC-HAZARD-WRITE-AFTER-READ
	 * (or WRITE-AFTER-WRITE with shared presentable images) validation errors.
	 */
	const VkSubpassDependency subpass_dependencies[1] = {
	    {
	        .srcSubpass = VK_SUBPASS_EXTERNAL,
	        .dstSubpass = 0,
	        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	        .srcAccessMask = 0,
	        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
	        .dependencyFlags = 0,
	    },
	};

	VkRenderPassCreateInfo render_pass_info = {
	    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
	    .attachmentCount = ARRAY_SIZE(attachments),
	    .pAttachments = attachments,
	    .subpassCount = ARRAY_SIZE(subpasses),
	    .pSubpasses = subpasses,
	    .dependencyCount = ARRAY_SIZE(subpass_dependencies),
	    .pDependencies = subpass_dependencies,
	};

	VkRenderPass render_pass = VK_NULL_HANDLE;
	ret = vk->vkCreateRenderPass( //
	    vk->device,               //
	    &render_pass_info,        //
	    NULL,                     //
	    &render_pass);            //
	VK_CHK_AND_RET(ret, "vkCreateRenderPass");

	*out_render_pass = render_pass;

	return VK_SUCCESS;
}

static void
update_ubo_and_src_descriptor_set(struct vk_bundle *vk,
                                  uint32_t ubo_binding,
                                  VkBuffer buffer,
                                  VkDeviceSize offset,
                                  VkDeviceSize size,
                                  uint32_t src_binding,
                                  VkSampler sampler,
                                  VkImageView image_view,
                                  VkDescriptorSet descriptor_set)
{
	VkDescriptorImageInfo image_info = {
	    .sampler = sampler,
	    .imageView = image_view,
	    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorBufferInfo buffer_info = {
	    .buffer = buffer,
	    .offset = offset,
	    .range = size,
	};

	VkWriteDescriptorSet write_descriptor_sets[2] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstSet = descriptor_set,
	        .dstBinding = src_binding,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .pImageInfo = &image_info,
	    },
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstSet = descriptor_set,
	        .dstBinding = ubo_binding,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .pBufferInfo = &buffer_info,
	    },
	};

	vk->vkUpdateDescriptorSets(            //
	    vk->device,                        //
	    ARRAY_SIZE(write_descriptor_sets), // descriptorWriteCount
	    write_descriptor_sets,             // pDescriptorWrites
	    0,                                 // descriptorCopyCount
	    NULL);                             // pDescriptorCopies
}

XRT_CHECK_RESULT static VkResult
do_ubo_and_src_alloc_and_write(struct render_gfx *render,
                               uint32_t ubo_binding,
                               const void *ubo_ptr,
                               VkDeviceSize ubo_size,
                               uint32_t src_binding,
                               VkSampler src_sampler,
                               VkImageView src_image_view,
                               VkDescriptorPool descriptor_pool,
                               VkDescriptorSetLayout descriptor_set_layout,
                               VkDescriptorSet *out_descriptor_set)
{
	VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
	struct render_sub_alloc ubo = XRT_STRUCT_INIT;
	struct vk_bundle *vk = vk_from_render(render);

	VkResult ret;


	/*
	 * Allocate and upload data.
	 */
	ret = render_sub_alloc_ubo_alloc_and_write( //
	    vk,                                     //
	    &render->ubo_tracker,                   // rsat
	    ubo_ptr,                                //
	    ubo_size,                               //
	    &ubo);                                  // out_rsa
	VK_CHK_AND_RET(ret, "render_sub_alloc_ubo_alloc_and_write");


	/*
	 * Create and fill out descriptor.
	 */

	ret = vk_create_descriptor_set( //
	    vk,                         //
	    descriptor_pool,            //
	    descriptor_set_layout,      //
	    &descriptor_set);           //
	VK_CHK_AND_RET(ret, "vk_create_descriptor_set");

	update_ubo_and_src_descriptor_set( //
	    vk,                            //
	    ubo_binding,                   //
	    ubo.buffer,                    //
	    ubo.offset,                    //
	    ubo.size,                      //
	    src_binding,                   //
	    src_sampler,                   //
	    src_image_view,                //
	    descriptor_set);               //

	*out_descriptor_set = descriptor_set;

	return VK_SUCCESS;
}


XRT_CHECK_RESULT static VkResult
create_png_pipeline(struct vk_bundle *vk,
                     VkRenderPass render_pass,
                     VkPipelineLayout pipeline_layout,
                     VkPipelineCache pipeline_cache,
                     uint32_t src_binding,
                     uint32_t png_index_count_total,
                     uint32_t png_stride,
                     VkShaderModule png_vert,
                     VkShaderModule png_frag,
                     VkPipeline *out_png_pipeline)
{
	VkResult ret;

	// Might be changed to line for debugging.
	VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

	// Do we use triangle strips or triangles with indices.
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	// if (png_index_count_total > 0) {
	// 	topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	// }

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	    .topology = topology,
	    .primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineRasterizationStateCreateInfo rasterization_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	    .depthClampEnable = VK_FALSE,
	    .rasterizerDiscardEnable = VK_FALSE,
	    .polygonMode = polygonMode,
	    .cullMode = VK_CULL_MODE_NONE,
	    .frontFace = VK_FRONT_FACE_CLOCKWISE,
	    .lineWidth = 1.0f,
	};

	VkPipelineColorBlendAttachmentState blend_attachment_state = {
	    .blendEnable = VK_FALSE,
	    .colorWriteMask = 0xf,
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	    .attachmentCount = 1,
	    .pAttachments = &blend_attachment_state,
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	    .depthTestEnable = VK_FALSE,
	    .depthWriteEnable = VK_FALSE,
	    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
	    .front = {.compareOp = VK_COMPARE_OP_ALWAYS},
	    .back = {.compareOp = VK_COMPARE_OP_ALWAYS},
	};

	VkPipelineViewportStateCreateInfo viewport_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	    .viewportCount = 1,
	    .scissorCount = 1,
	};

	VkPipelineMultisampleStateCreateInfo multisample_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkDynamicState dynamic_states[] = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	    .dynamicStateCount = ARRAY_SIZE(dynamic_states),
	    .pDynamicStates = dynamic_states,
	};

	// clang-format off
	VkVertexInputAttributeDescription vertex_input_attribute_descriptions[2] = {
	    {
	        .binding = src_binding,
	        .location = 0,
	        .format = VK_FORMAT_R32G32_SFLOAT,
	        .offset = 0,
	    },
		{
	        .binding = src_binding,
	        .location = 1,
	        .format = VK_FORMAT_R32G32_SFLOAT,
	        .offset = 8,
	    },
	};

	VkVertexInputBindingDescription vertex_input_binding_description[1] = {
	    {
	        .binding = src_binding,
	        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	        .stride = png_stride,
	    },
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	    .vertexAttributeDescriptionCount = ARRAY_SIZE(vertex_input_attribute_descriptions),
	    .pVertexAttributeDescriptions = vertex_input_attribute_descriptions,
	    .vertexBindingDescriptionCount = ARRAY_SIZE(vertex_input_binding_description),
	    .pVertexBindingDescriptions = vertex_input_binding_description,
	};
	// clang-format on

	VkPipelineShaderStageCreateInfo shader_stages[2] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .stage = VK_SHADER_STAGE_VERTEX_BIT,
	        .module = png_vert,
	        .pName = "main",
	    },
	    {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	        .module = png_frag,
	        .pName = "main",
	    },
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
	    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	    .stageCount = ARRAY_SIZE(shader_stages),
	    .pStages = shader_stages,
	    .pVertexInputState = &vertex_input_state,
	    .pInputAssemblyState = &input_assembly_state,
	    .pViewportState = &viewport_state,
	    .pRasterizationState = &rasterization_state,
	    .pMultisampleState = &multisample_state,
	    .pDepthStencilState = &depth_stencil_state,
	    .pColorBlendState = &color_blend_state,
	    .pDynamicState = &dynamic_state,
	    .layout = pipeline_layout,
	    .renderPass = render_pass,
	    .basePipelineHandle = VK_NULL_HANDLE,
	    .basePipelineIndex = -1,
	};

	VkPipeline pipeline = VK_NULL_HANDLE;
	ret = vk->vkCreateGraphicsPipelines( //
	    vk->device,                      //
	    pipeline_cache,                  //
	    1,                               //
	    &pipeline_info,                  //
	    NULL,                            //
	    &pipeline);                      //
	VK_CHK_AND_RET(ret, "vkCreateGraphicsPipelines");

	*out_png_pipeline = pipeline;

	return VK_SUCCESS;
}

bool
render_png_render_pass_init(struct render_png_render_pass *rprp,
                            struct render_resources *r,
                            VkFormat format,
                            VkAttachmentLoadOp load_op,
                            VkImageLayout final_layout)
{
	struct vk_bundle *vk = r->vk;
	VkResult ret;

	ret = create_implicit_render_pass( //
	    vk,                            //
	    format,                        // target_format
	    load_op,                       //
	    final_layout,                  //
	    &rprp->render_pass);           // out_render_pass
	VK_CHK_WITH_RET(ret, "create_implicit_render_pass", false);
	VK_NAME_RENDER_PASS(vk, rprp->render_pass, "render_png_render_pass render pass");

	ret = create_png_pipeline(    //
	    vk,                        //
	    rprp->render_pass,         //
	    r->png.pipeline_layout,   //
	    r->pipeline_cache,         //
	    r->png.src_binding,       //
	    r->png.index_count, //
	    r->png.stride,            //
	    r->shaders->png_vert,     //
	    r->shaders->png_frag,     //
	    &rprp->pipeline);     //
	VK_CHK_WITH_RET(ret, "create_png_pipeline", false);
	VK_NAME_PIPELINE(vk, rprp->pipeline, "render_png_render_pass png pipeline");

	// Set fields.
	rprp->r = r;
	rprp->format = format;
	rprp->sample_count = VK_SAMPLE_COUNT_1_BIT;
	rprp->load_op = load_op;
	rprp->final_layout = final_layout;

	return true;
}

void
render_png_render_pass_fini(struct render_png_render_pass *rprp)
{
	struct vk_bundle *vk = rprp->r->vk;

	D(RenderPass, rprp->render_pass);
	D(Pipeline, rprp->pipeline);

	U_ZERO(rprp);
}


XRT_CHECK_RESULT static VkResult
create_framebuffer(struct vk_bundle *vk,
                   VkImageView image_view,
                   VkRenderPass render_pass,
                   uint32_t width,
                   uint32_t height,
                   VkFramebuffer *out_external_framebuffer)
{
	VkResult ret;

	VkImageView attachments[1] = {image_view};

	VkFramebufferCreateInfo frame_buffer_info = {
	    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
	    .renderPass = render_pass,
	    .attachmentCount = ARRAY_SIZE(attachments),
	    .pAttachments = attachments,
	    .width = width,
	    .height = height,
	    .layers = 1,
	};

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	ret = vk->vkCreateFramebuffer( //
	    vk->device,                //
	    &frame_buffer_info,        //
	    NULL,                      //
	    &framebuffer);             //
	VK_CHK_AND_RET(ret, "vkCreateFramebuffer");

	*out_external_framebuffer = framebuffer;

	return VK_SUCCESS;
}

static void
begin_render_pass(struct vk_bundle *vk,
                  VkCommandBuffer command_buffer,
                  VkRenderPass render_pass,
                  VkFramebuffer framebuffer,
                  const VkRect2D *render_area,
                  const VkClearColorValue *color)
{
	VkClearValue clear_color[1] = {{
	    .color = *color,
	}};

	VkRenderPassBeginInfo render_pass_begin_info = {
	    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
	    .renderPass = render_pass,
	    .framebuffer = framebuffer,
	    .renderArea = *render_area,
	    .clearValueCount = ARRAY_SIZE(clear_color),
	    .pClearValues = clear_color,
	};

	vk->vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

bool
render_png_target_resources_init(struct render_gfx_target_resources *rtr,
                                 struct render_resources *r,
                                 struct render_png_render_pass *rprp,
                                 VkImageView target,
                                 struct render_viewport_data viewport_data)
{
	struct vk_bundle *vk = r->vk;
	VkResult ret;
	rtr->r = r;

	ret = create_framebuffer( //
	    vk,                   //
	    target,               // image_view
	    rprp->render_pass,    //
	    viewport_data.w,         //
	    viewport_data.h,        //
	    &rtr->framebuffer);   // out_external_framebuffer
	VK_CHK_WITH_RET(ret, "create_framebuffer", false);
	VK_NAME_FRAMEBUFFER(vk, rtr->framebuffer, "render_png_target_resources framebuffer");

	// Set fields.
	rtr->rprp = rprp;
	rtr->render_area = (VkRect2D){
	    .offset = {viewport_data.x, viewport_data.y},
	    .extent = {viewport_data.w, viewport_data.h},
	};

	return true;
}

bool
render_png_begin(struct render_gfx *render)
{
	struct vk_bundle *vk = vk_from_render(render);
	VkResult ret;

	ret = vk->vkResetCommandPool(vk->device, render->r->png.cmd_pool, 0);
	VK_CHK_WITH_RET(ret, "vkResetCommandPool", false);


	VkCommandBufferBeginInfo begin_info = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	ret = vk->vkBeginCommandBuffer( //
	    render->r->png.cmd,             //
	    &begin_info);               //
	VK_CHK_WITH_RET(ret, "vkResetCommandPool", false);

	vk->vkCmdResetQueryPool(   //
	    render->r->png.cmd,        //
	    render->r->query_pool, //
	    0,                     // firstQuery
	    2);                    // queryCount

	vk->vkCmdWriteTimestamp(               //
	    render->r->png.cmd,                    //
	    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // pipelineStage
	    render->r->query_pool,             //
	    0);                                // query

	return true;
}

bool
render_png_end(struct render_gfx *render)
{
	struct vk_bundle *vk = vk_from_render(render);
	VkResult ret;

	vk->vkCmdWriteTimestamp(                  //
	    render->r->png.cmd,                       //
	    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // pipelineStage
	    render->r->query_pool,                //
	    1);                                   // query

	ret = vk->vkEndCommandBuffer(render->r->png.cmd);
	VK_CHK_WITH_RET(ret, "vkEndCommandBuffer", false);

	return true;
}

bool
render_png_begin_target(struct render_gfx *render,
                        struct render_gfx_target_resources *rtr,
                        const VkClearColorValue *color)
{
	struct vk_bundle *vk = vk_from_render(render);

	assert(render->rtr == NULL);
	render->rtr = rtr;

	VkRenderPass render_pass = rtr->rprp->render_pass;
	VkFramebuffer framebuffer = rtr->framebuffer;
	const VkRect2D *render_area = &rtr->render_area;

	begin_render_pass(  //
	    vk,             //
	    render->r->png.cmd, //
	    render_pass,    //
	    framebuffer,    //
	    render_area,    //
	    color);         //

	return true;
}

void
render_png_end_target(struct render_gfx *render)
{
	struct vk_bundle *vk = vk_from_render(render);

	assert(render->rtr != NULL);
	render->rtr = NULL;

	// Stop the [shared] render pass.
	vk->vkCmdEndRenderPass(render->r->png.cmd);
}

void
render_png_begin_view(struct render_gfx *render,
                      uint32_t view,
                      const struct render_viewport_data *viewport_data,
                      const render_scissor_data_t *scissor_data)
{
	struct vk_bundle *vk = vk_from_render(render);

	// We currently only support two views.
	assert(view == 0 || view == 1);
	assert(render->rtr != NULL);
	assert(viewport_data != NULL);
	assert(scissor_data != NULL);

	/*
	 * Viewport
	 */

	const VkViewport viewport = {
	    .x = (float)viewport_data->x,
	    .y = (float)viewport_data->y,
	    .width = (float)viewport_data->w,
	    .height = (float)viewport_data->h,
	    .minDepth = 0.0f,
	    .maxDepth = 1.0f,
	};

	vk->vkCmdSetViewport(render->r->png.cmd, //
	                     0,              // firstViewport
	                     1,              // viewportCount
	                     &viewport);     //

	/*
	 * Scissor
	 */

	const VkRect2D scissor = {
	    .offset =
	        {
	            .x = scissor_data->x,
	            .y = scissor_data->y,
	        },
	    .extent =
	        {
	            .width = scissor_data->w,
	            .height = scissor_data->h,
	        },
	};

	vk->vkCmdSetScissor(render->r->png.cmd, //
	                    0,              // firstScissor
	                    1,              // scissorCount
	                    &scissor);      //
}

void
render_png_end_view(struct render_gfx *render)
{
	//! Must have a current target.
	assert(render->rtr != NULL);
}

XRT_CHECK_RESULT VkResult
render_png_alloc_and_write(struct render_gfx *render,
                                const struct render_png_ubo_data *data,
                                VkSampler src_sampler,
                                VkImageView src_image_view,
                                VkDescriptorSet *out_descriptor_set)
{
	struct render_resources *r = render->r;

	return do_ubo_and_src_alloc_and_write(  //
	    render,                             //
	    r->png.ubo_binding,                //
	    data,                               // ubo_ptr
	    sizeof(*data),                      // ubo_size
	    r->png.src_binding,                //
	    src_sampler,                        //
	    src_image_view,                     //
	    r->png.ubo_and_src_descriptor_pool, //
	    r->png.descriptor_set_layout,      //
	    out_descriptor_set);                //
}

void
render_png_draw(struct render_gfx *render, VkDescriptorSet descriptor_set) {
	struct vk_bundle *vk = vk_from_render(render);
	struct render_resources *r = render->r;


	/*
	 * Descriptors and pipeline.
	 */

	VkDescriptorSet descriptor_sets[1] = {descriptor_set};
	vk->vkCmdBindDescriptorSets(         //
	    r->png.cmd,                          //
	    VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
	    r->png.pipeline_layout,         // layout
	    0,                               // firstSet
	    ARRAY_SIZE(descriptor_sets),     // descriptorSetCount
	    descriptor_sets,                 // pDescriptorSets
	    0,                               // dynamicOffsetCount
	    NULL);                           // pDynamicOffsets

	// Select which pipeline we want.
	VkPipeline pipeline = render->rtr->rprp->pipeline;

	vk->vkCmdBindPipeline(               //
	    r->png.cmd,                          //
	    VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
	    pipeline);                       // pipeline

	VkDescriptorImageInfo image_info = {
	    .sampler = r->samplers.clamp_to_border_black,
	    .imageView = r->png.color.image_view,
	    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet write_descriptor_sets[1] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstSet = descriptor_set,
	        .dstBinding = r->png.src_binding,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .pImageInfo = &image_info,
	    }
	};

	vk->vkUpdateDescriptorSets(            //
	    vk->device,                        //
	    ARRAY_SIZE(write_descriptor_sets), // descriptorWriteCount
	    write_descriptor_sets,             // pDescriptorWrites
	    0,                                 // descriptorCopyCount
	    NULL);                             // pDescriptorCopies
	
	VkBuffer buffers[1] = {r->png.vbo.buffer};
	VkDeviceSize offsets[1] = {0};
	assert(ARRAY_SIZE(buffers) == ARRAY_SIZE(offsets));

	vk->vkCmdBindVertexBuffers( //
	    r->png.cmd,                 //
	    0,                      // firstBinding
	    ARRAY_SIZE(buffers),    // bindingCount
	    buffers,                // pBuffers
	    offsets);               // pOffsets

	vk->vkCmdBindIndexBuffer(
		r->png.cmd,
		r->png.ibo.buffer,
		offsets[0],
		VK_INDEX_TYPE_UINT16);

	vk->vkCmdDraw(            //
		r->png.cmd,               //
		r->png.vertex_count, // vertexCount
		1,                    // instanceCount
		0,                    // firstVertex
		0);                   // firstInstance
	vk->vkCmdDrawIndexed(                  //
		r->png.cmd,                            //
		r->png.index_count,  // indexCount
		1,                                 // instanceCount
		0, // firstIndex
		0,                                 // vertexOffset
		0);                                // firstInstance
}