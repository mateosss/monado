#include "xrt/xrt_compositor.h"
#include "util/comp_swapchain.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"

#include "util/u_trace_marker.h"

#include "vk/vk_helpers.h"

#include "render/render_interface.h"

#include "util/comp_render.h"
#include "util/comp_render_helpers.h"

static const VkClearColorValue background_color = {
    .float32 = {0.0f, 0.0f, 0.0f, 1.0f},
};

static inline const struct comp_swapchain_image *
get_layer_image(const struct comp_layer *layer, uint32_t swapchain_index, uint32_t image_index)
{

	const struct comp_swapchain *sc = (struct comp_swapchain *)(comp_layer_get_swapchain(layer, swapchain_index));
	return &sc->images[image_index];
}

void
comp_render_png_dispatch(struct render_gfx *render,
                         const struct comp_layer *layers,
                         const uint32_t layer_count,
                         struct render_png_render_pass *render_pass)
{
	const struct comp_layer *layer = &layers[0];

	if (layer_count > 0) {
		const struct xrt_layer_data *ldata = &layer->data;

		const struct xrt_layer_projection_view_data *png_vds[XRT_MAX_VIEWS];
		const struct xrt_layer_depth_data *png_dvds[XRT_MAX_VIEWS];
		for (uint32_t view = 0; view < XRT_MAX_VIEWS; ++view) {
			if (ldata->type == XRT_LAYER_PROJECTION) {
				view_index_to_projection_data(view, ldata, &png_vds[view]);
			} else {
				view_index_to_depth_data(view, ldata, &png_vds[view], &png_dvds[view]);
			}
		}

		VkDescriptorSet png_descriptor_sets[XRT_MAX_VIEWS];

		struct render_gfx_target_resources png_rtr[XRT_MAX_VIEWS];

		for (uint32_t i = 0; i < XRT_MAX_VIEWS; i++) {
			const uint32_t array_index = png_vds[i]->sub.array_index;

			const struct comp_swapchain_image *image = get_layer_image(layer, i, png_vds[i]->sub.image_index);

			const VkImageView layer_image_view = get_image_view(image, ldata->flags, array_index);

			struct render_png_ubo_data data = XRT_STRUCT_INIT;

			struct xrt_rect rect = png_vds[i]->sub.rect;

			// Convenience.
			const struct render_viewport_data viewport_data = {
				.x = rect.offset.w,
				.y = rect.offset.h,
				.w = rect.extent.w,
				.h = rect.extent.h
			};

			render_png_target_resources_init( //
				&png_rtr[i],                          //
				render->r,                       //
				render_pass,       //
				layer_image_view,                   //
				viewport_data);                      //

			struct xrt_pose png_pose = XRT_POSE_IDENTITY;

			struct xrt_pose view_pose = png_vds[i]->pose;
			struct xrt_fov fov = png_vds[i]->fov;

			// Model
			struct xrt_matrix_4x4 m;
			struct xrt_vec3 sz = {1.f, 1.f, 1.f};
			math_matrix_4x4_model(&png_pose, &sz, &m);

			// correct fov & src_rect
			if (ldata->flip_y) {
				fov.angle_up *= -1;
				fov.angle_down *= -1;
			}

			// Projection
			struct xrt_matrix_4x4 p;
			if (ldata->type == XRT_LAYER_PROJECTION) {
				math_matrix_4x4_projection_vulkan_infinite_reverse(&fov, 0.1, &p);
			} else {
				math_matrix_4x4_projection(fov, png_dvds[i]->near_z, png_dvds[i]->far_z, &p);
			}

			struct xrt_matrix_4x4 v;
			math_matrix_4x4_view_from_pose(&view_pose, &v);
			
			struct xrt_matrix_4x4 vp;
			math_matrix_4x4_multiply(&p, &v, &vp);

			math_matrix_4x4_multiply(&vp, &m, &data.mvp);
			
			VkResult ret = render_png_alloc_and_write( //
				render,                            //
				&data,                             //
				render->r->samplers.clamp_to_border_black,          //
				render->r->png.color.image_view,       //
				&png_descriptor_sets[i]);           //

			render_png_begin_target(       //
				render,                    //
				&png_rtr[i],         //
				&background_color); //

			render_png_begin_view( //
				render,            //
				i,                 // view_index
				&viewport_data,     //
				&viewport_data);     //

			render_png_draw(      //
				render,                //
				png_descriptor_sets[i] );

			render_png_end_view(render);
			render_png_end_target(render);

			render_gfx_target_resources_fini(&png_rtr[i]);
		}
	}
}