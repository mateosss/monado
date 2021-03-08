// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header defining an xrt HMD device.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#define XRT_DEVICE_NAME_LEN 256

#include "xrt/xrt_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xrt_tracking;


/*!
 * A per-lens view information.
 *
 * @ingroup xrt_iface
 */
struct xrt_view
{
	/*!
	 * Viewport position on the screen, in absolute screen coordinates on
	 * an unrotated display, like the HMD presents it to the OS.
	 * This field is only used by @ref comp to setup the device rendering.
	 *
	 * If the view is being rotated by xrt_view.rot 90° right in the
	 * distortion shader then `display.w_pixels == viewport.h_pixels` and
	 * `display.h_pixels == viewport.w_pixels`.
	 */
	struct
	{
		uint32_t x_pixels;
		uint32_t y_pixels;
		uint32_t w_pixels;
		uint32_t h_pixels;
	} viewport;

	/*!
	 * Physical properties of this display (or the part of a display that
	 * covers this view), not in absolute screen coordinates but like the
	 * clients see them i.e. after rotation is applied by xrt_view::rot.
	 * This field is only used for the clients' swapchain setup.
	 *
	 * The xrt_view::display::w_pixels and xrt_view::display::h_pixels
	 * become the recommended image size for this view, after being scaled
	 * by XRT_COMPOSITOR_SCALE_PERCENTAGE.
	 */
	struct
	{
		uint32_t w_pixels;
		uint32_t h_pixels;
		float w_meters;
		float h_meters;
	} display;

	/*!
	 * Rotation 2d matrix used to rotate the position of the output of the
	 * distortion shaders onto the screen. If the distortion shader is
	 * based on mesh, then this matrix rotates the vertex positions.
	 */
	struct xrt_matrix_2x2 rot;

	/*!
	 * FoV expressed as in OpenXR.
	 */
	struct xrt_fov fov;
};

/*!
 * All of the device components that deals with interfacing to a users head.
 *
 * HMD is probably a bad name for the future but for now will have to do.
 *
 * @ingroup xrt_iface
 */
struct xrt_hmd_parts
{
	/*!
	 * The hmd screen as an unrotated display, like the HMD presents it to
	 * the OS.
	 *
	 * This field is used by @ref comp to setup the extended mode window.
	 */
	struct
	{
		int w_pixels;
		int h_pixels;
		//! Nominal frame interval
		uint64_t nominal_frame_interval_ns;
	} screens[1];

	/*!
	 * Display information.
	 *
	 * For now hardcoded display to two.
	 */
	struct xrt_view views[2];

	/*!
	 * Supported blend modes, a bitfield.
	 */
	enum xrt_blend_mode blend_mode;

	/*!
	 * Distortion information.
	 */
	struct
	{
		//! Supported distortion models, a bitfield.
		enum xrt_distortion_model models;
		//! Preferred disortion model, single value.
		enum xrt_distortion_model preferred;

		struct
		{
			//! Data.
			float *vertices;
			//! Number of vertices.
			size_t num_vertices;
			//! Stride of vertices
			size_t stride;
			//! 1 or 3 for (chromatic aberration).
			size_t num_uv_channels;

			//! Indices, for triangle strip.
			int *indices;
			//! Number of indices for the triangle strip.
			size_t num_indices[2];
			//! Offsets for the indices.
			size_t offset_indices[2];
			//! Total number of indices.
			size_t total_num_indices;
		} mesh;
	} distortion;
};

/*!
 * A single named input, that sits on a @ref xrt_device.
 *
 * @ingroup xrt_iface
 */
struct xrt_input
{
	//! Is this input active.
	bool active;

	int64_t timestamp;

	enum xrt_input_name name;

	union xrt_input_value value;
};

/*!
 * A single named output, that sits on a @ref xrt_device.
 *
 * @ingroup xrt_iface
 */
struct xrt_output
{
	enum xrt_output_name name;
};


/*!
 * A binding pair, going @p from a binding point to a @p device input.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_input_pair
{
	enum xrt_input_name from;   //!< From which name.
	enum xrt_input_name device; //!< To input on the device.
};

/*!
 * A binding pair, going @p from a binding point to a @p device output.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_output_pair
{
	enum xrt_output_name from;   //!< From which name.
	enum xrt_output_name device; //!< To output on the device.
};

/*!
 * A binding profile, has lists of binding pairs to goes from device in @p name
 * to the device it hangs off on.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_profile
{
	//! Device this binding emulates.
	enum xrt_device_name name;

	struct xrt_binding_input_pair *inputs;
	size_t num_inputs;
	struct xrt_binding_output_pair *outputs;
	size_t num_outputs;
};

/*!
 * @interface xrt_device
 *
 * A single HMD or input device.
 *
 * @ingroup xrt_iface
 */
struct xrt_device
{
	//! Enum identifier of the device.
	enum xrt_device_name name;
	enum xrt_device_type device_type;

	//! A string describing the device.
	char str[XRT_DEVICE_NAME_LEN];

	//! A unique identifier. Persistent across configurations, if possible.
	char serial[XRT_DEVICE_NAME_LEN];

	//! Null if this device does not interface with the users head.
	struct xrt_hmd_parts *hmd;

	//! Always set, pointing to the tracking system for this device.
	struct xrt_tracking_origin *tracking_origin;

	//! Number of bindings.
	size_t num_binding_profiles;
	// Array of alternative binding profiles.
	struct xrt_binding_profile *binding_profiles;

	//! Number of inputs.
	size_t num_inputs;
	//! Array of input structs.
	struct xrt_input *inputs;

	//! Number of outputs.
	size_t num_outputs;
	//! Array of output structs.
	struct xrt_output *outputs;

	bool orientation_tracking_supported;
	bool position_tracking_supported;
	bool hand_tracking_supported;

	/*!
	 * Update any attached inputs.
	 *
	 * @param[in] xdev        The device.
	 */
	void (*update_inputs)(struct xrt_device *xdev);

	/*!
	 * Get relationship of a tracked device to the tracking origin space as
	 * the base space. It is the responsibility of the device driver to do
	 * any prediction, there are helper functions available for this.
	 *
	 * The timestamps are system monotonic timestamps, such as returned by
	 * os_monotonic_get_ns().
	 *
	 * @param[in] xdev           The device.
	 * @param[in] name           Some devices may have multiple poses on
	 *                           them, select the one using this field. For
	 *                           HMDs use @p XRT_INPUT_GENERIC_HEAD_POSE.
	 * @param[in] at_timestamp_ns If the device can predict or has a history
	 *                            of positions, this is when the caller
	 *                            wants the pose to be from.
	 * @param[out] out_relation The relation read from the device.
	 *
	 * @see xrt_input_name
	 */
	void (*get_tracked_pose)(struct xrt_device *xdev,
	                         enum xrt_input_name name,
	                         uint64_t at_timestamp_ns,
	                         struct xrt_space_relation *out_relation);

	/*!
	 * Get relationship of hand joints to the tracking origin space as
	 * the base space. It is the responsibility of the device driver to do
	 * any prediction, there are helper functions available for this.
	 *
	 * The timestamps are system monotonic timestamps, such as returned by
	 * os_monotonic_get_ns().
	 *
	 * @param[in] xdev           The device.
	 * @param[in] name           Some devices may have multiple poses on
	 *                           them, select the one using this field. For
	 *                           hand tracking use @p
	 * XRT_INPUT_GENERIC_HAND_TRACKING_DEFAULT_SET.
	 * @param[in] at_timestamp_ns If the device can predict or has a history
	 *                            of positions, this is when the caller
	 *                            wants the pose to be from.
	 * @param[out] out_relation The relation read from the device.
	 *
	 * @see xrt_input_name
	 */
	void (*get_hand_tracking)(struct xrt_device *xdev,
	                          enum xrt_input_name name,
	                          uint64_t at_timestamp_ns,
	                          struct xrt_hand_joint_set *out_value);
	/*!
	 * Set a output value.
	 *
	 * @param[in] xdev           The device.
	 * @param[in] name           The output component name to set.
	 * @param[in] value          The value to set the output to.
	 *                           @todo make this param a pointer to const.
	 * @see xrt_output_name
	 */
	void (*set_output)(struct xrt_device *xdev, enum xrt_output_name name, union xrt_output_value *value);

	/*!
	 * Get the per view pose in relation to the view space. Does not do any
	 * device level tracking, use get_tracked_pose for that.
	 *
	 * @param[in] xdev         The device.
	 * @param[in] eye_relation The interpupillary relation as a 3D position.
	 *                         Most simple stereo devices would just want to
	 *                         set `out_pose->position.[x|y|z] = ipd.[x|y|z]
	 *                         / 2.0f` and adjust for left vs right view.
	 *                         Not to be confused with IPD that is absolute
	 *                         distance, this is a full 3D translation.
	 *                         @todo make this param a pointer to const.
	 * @param[in] view_index   Index of view.
	 * @param[out] out_pose    Output pose. See eye_relation argument for
	 *                         sample position. Be sure to also set
	 *                         orientation: most likely identity
	 *                         orientation unless you have canted screens.
	 */
	void (*get_view_pose)(struct xrt_device *xdev,
	                      struct xrt_vec3 *eye_relation,
	                      uint32_t view_index,
	                      struct xrt_pose *out_pose);

	bool (*compute_distortion)(struct xrt_device *xdev, int view, float u, float v, struct xrt_uv_triplet *result);

	/*!
	 * Destroy device.
	 */
	void (*destroy)(struct xrt_device *xdev);
};

/*!
 * Helper function for @ref xrt_device::update_inputs.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_update_inputs(struct xrt_device *xdev)
{
	xdev->update_inputs(xdev);
}

/*!
 * Helper function for @ref xrt_device::get_tracked_pose.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_tracked_pose(struct xrt_device *xdev,
                            enum xrt_input_name name,
                            uint64_t requested_timestamp_ns,
                            struct xrt_space_relation *out_relation)
{
	xdev->get_tracked_pose(xdev, name, requested_timestamp_ns, out_relation);
}

/*!
 * Helper function for @ref xrt_device::get_hand_tracking.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_hand_tracking(struct xrt_device *xdev,
                             enum xrt_input_name name,
                             uint64_t requested_timestamp_ns,
                             struct xrt_hand_joint_set *out_value)
{
	xdev->get_hand_tracking(xdev, name, requested_timestamp_ns, out_value);
}

/*!
 * Helper function for @ref xrt_device::set_output.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_set_output(struct xrt_device *xdev, enum xrt_output_name name, union xrt_output_value *value)
{
	xdev->set_output(xdev, name, value);
}

/*!
 * Helper function for @ref xrt_device::get_view_pose.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_view_pose(struct xrt_device *xdev,
                         struct xrt_vec3 *eye_relation,
                         uint32_t view_index,
                         struct xrt_pose *out_pose)
{
	xdev->get_view_pose(xdev, eye_relation, view_index, out_pose);
}

/*!
 * Helper function for @ref xrt_device::destroy.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_destroy(struct xrt_device **xdev_ptr)
{
	struct xrt_device *xdev = *xdev_ptr;
	if (xdev == NULL) {
		return;
	}

	xdev->destroy(xdev);
	*xdev_ptr = NULL;
}


#ifdef __cplusplus
}
#endif
