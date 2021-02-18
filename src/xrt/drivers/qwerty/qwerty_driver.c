#include "math/m_mathinclude.h"
#include <stddef.h>
#include <stdio.h>
#include "xrt/xrt_prober.h"
#include "util/u_device.h"
#include "util/u_distortion_mesh.h"


struct qwerty_hmd
{
	struct xrt_device base;
};

static inline struct qwerty_hmd *
qwerty_hmd(struct xrt_device *xdev)
{
	return (struct qwerty_hmd *)xdev;
}


static void
qwerty_update_inputs(struct xrt_device *xdev)
{
	printf(">>> [QWERTY] qwerty_update_inputs\n");
}

static void
qwerty_get_tracked_pose(struct xrt_device *xdev,
                        enum xrt_input_name name,
                        uint64_t at_timestamp_ns,
                        struct xrt_space_relation *out_relation)
{
	printf(">>> [QWERTY] qwerty_get_tracked_pose\n");
}

static void
qwerty_get_view_pose(struct xrt_device *xdev,
                     struct xrt_vec3 *eye_relation,
                     uint32_t view_index,
                     struct xrt_pose *out_pose)
{
	printf(">>> [QWERTY] qwerty_get_view_pose\n");
}

static void
qwerty_destroy(struct xrt_device *xdev)
{
 	struct qwerty_hmd *qh = qwerty_hmd(xdev);

	u_device_free(&qh->base);

	printf(">>> [QWERTY] qwerty_destroy\n");
}

int
qwerty_found(struct xrt_prober *xp,
             struct xrt_prober_device **devices,
             size_t num_devices,
             size_t index,
             cJSON *attached_data,
             struct xrt_device **out_xdevs)
{
	printf(">>> [QWERTY] qwerty_found()\n");
	// struct qwerty_hmd *qh = U_DE;
	enum u_device_alloc_flags flags = U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE;

	// U_DEVICE_ALLOCATE makes a calloc and fill pointers to zeroed unique memory
	// the properties set are commented below
	size_t num_inputs = 1, num_outputs = 0;
	struct qwerty_hmd *qh = U_DEVICE_ALLOCATE(struct qwerty_hmd, flags, num_inputs, num_outputs);
	qh->base.name = XRT_DEVICE_GENERIC_HMD;
	qh->base.device_type = XRT_DEVICE_TYPE_HMD;

	snprintf(qh->base.str, XRT_DEVICE_NAME_LEN, "Qwerty HMD");
	snprintf(qh->base.serial, XRT_DEVICE_NAME_LEN, "Qwerty HMD");

	// Fills qh->base.hmd
	struct u_device_simple_info info;
	info.display.w_pixels = 1280;
	info.display.h_pixels = 720;
	info.display.w_meters = 0.13f;
	info.display.h_meters = 0.07f;
	info.lens_horizontal_separation_meters = 0.13f / 2.0f;
	info.lens_vertical_position_meters = 0.07f / 2.0f;
	info.views[0].fov = 85.0f * (M_PI / 180.0f);
	info.views[1].fov = 85.0f * (M_PI / 180.0f);

	if (!u_device_setup_split_side_by_side(&qh->base, &info)) {
		printf(">>> [QWERTY ERROR] Failed to setup basic device info\n");
		qwerty_destroy(&qh->base);
		return -1;
	}


	// qh->base.tracking_origin // Set on alloc with
	// .type == XRT_TRACKING_TYPE_NONE
	// .offset.orientation.w = 1f and
	// .name == "No tracking" */

	// qh->base.num_binding_profiles // Set on alloc
	// qh->base.binding_profiles // Does not matter as num is zero

	// qh->base.num_inputs // Set on alloc
	// qh->base.inputs // Set on alloc with inputs[i].active == true

	// qh->base.num_outputs // Set on alloc
	// qh->base.outputs // Set on alloc

	// qh->base.orientation_tracking_supported
	// qh->base.position_tracking_supported
	// qh->base.hand_tracking_supported

	qh->base.update_inputs = qwerty_update_inputs;
	qh->base.get_tracked_pose = qwerty_get_tracked_pose;
	// qh->base.get_hand_tracking // Hopefully unused
	// qh->base.set_output // Hopefully unused
	qh->base.get_view_pose = qwerty_get_view_pose;
	u_distortion_mesh_set_none(&qh->base); // Fills qh->base.compute_distortion
	qh->base.destroy = qwerty_destroy;


	out_xdevs[0] = &qh->base;
	return 1;
}
