// Copyright 2019-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Matrix function for creating projection matrices.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_math
 */

#include "m_mathinclude.h"
#include "xrt/xrt_defines.h"


/*
 *
 * Helpers.
 *
 */

template <typename Scalar, typename ResultType>
void
calc_vulkan_projection_infinite_reverse(const xrt_fov &fov, Scalar near_plane, ResultType &result)
{
	const Scalar tan_left = static_cast<Scalar>(tan(fov.angle_left));
	const Scalar tan_right = static_cast<Scalar>(tan(fov.angle_right));

	const Scalar tan_down = static_cast<Scalar>(tan(fov.angle_down));
	const Scalar tan_up = static_cast<Scalar>(tan(fov.angle_up));

	// This is here for clarity.
	const bool vulkan_projection_space_y = true;

	const Scalar tan_width = tan_right - tan_left;
	const Scalar tan_height = vulkan_projection_space_y  // Projection space y direction:
	                              ? (tan_down - tan_up)  // Vulkan Y down
	                              : (tan_up - tan_down); // OpenGL Y up

	const Scalar a11 = 2 / tan_width;
	const Scalar a22 = 2 / tan_height;

	const Scalar a31 = (tan_right + tan_left) / tan_width;
	const Scalar a32 = (tan_up + tan_down) / tan_height;


	/*
	 * Vulkan's Z clip space is [0 .. 1] (OpenGL [-1 .. 0 .. 1]).
	 * We are using reverse depth for better floating value.
	 *  - Near is 1
	 *  - Far is 0 (infinite)
	 *
	 * https://vincent-p.github.io/posts/vulkan_perspective_matrix/
	 */

	const Scalar a33 = 0; // vulkan = 0, opengl = -1
	const Scalar a34 = -1;
	const Scalar a43 = near_plane; // Reverse depth and half vs opengl, `-2 * near_plane`.

	// clang-format off
	result = ResultType({
		a11,   0,   0,   0,
		  0, a22,   0,   0,
		a31, a32, a33, a34,
		  0,   0, a43,   0,
	});
	// clang-format on
}


/*
 *
 * 'Exported' functions.
 *
 */

extern "C" void
math_matrix_4x4_projection_vulkan_infinite_reverse(const struct xrt_fov *fov,
                                                   float near_plane,
                                                   struct xrt_matrix_4x4 *result)
{
	calc_vulkan_projection_infinite_reverse<float, xrt_matrix_4x4>(*fov, near_plane, *result);
}

extern "C" void
math_matrix_4x4_projection(const struct xrt_fov fov,
						   float near_plane,
					 	   float far_plane,
						   struct xrt_matrix_4x4 *result)
{
	const float tan_left = static_cast<float>(tan(fov.angle_left));
	const float tan_right = static_cast<float>(tan(fov.angle_right));

	const float tan_down = static_cast<float>(tan(fov.angle_down));
	const float tan_up = static_cast<float>(tan(fov.angle_up));

	// This is here for clarity.
	const bool vulkan_projection_space_y = true;

	const float tan_width = tan_right - tan_left;
	const float tan_height = vulkan_projection_space_y  // Projection space y direction:
	                              ? (tan_down - tan_up)  // Vulkan Y down
	                              : (tan_up - tan_down); // OpenGL Y up

	const float a11 = 2 / tan_width;
	const float a22 = 2 / tan_height;

	const float a31 = (tan_right + tan_left) / tan_width;
	const float a32 = (tan_up + tan_down) / tan_height;


	/*
	 * Vulkan's Z clip space is [0 .. 1] (OpenGL [-1 .. 0 .. 1]).
	 * We are using reverse depth for better floating value.
	 *  - Near is 1
	 *  - Far is 0 (infinite)
	 *
	 * https://vincent-p.github.io/posts/vulkan_perspective_matrix/
	 */

	const float a33 = near_plane/(far_plane-near_plane);
	const float a34 = -1;
	const float a43 = (near_plane*far_plane)/(far_plane-near_plane); // Reverse depth and half vs opengl, `-2 * near_plane`.

	// clang-format off
	*result = (struct xrt_matrix_4x4){
		a11,   0,   0,   0,
		  0, a22,   0,   0,
		a31, a32, a33, a34,
		  0,   0, a43,   0,
	};
	// clang-format on
}