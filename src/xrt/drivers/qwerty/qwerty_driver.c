#include "math/m_mathinclude.h"
#include "math/m_api.h"

#include <stddef.h>
#include <stdio.h>
#include "xrt/xrt_prober.h"
#include "util/u_device.h"
#include "util/u_distortion_mesh.h"

#define QWERTY_HMD_INITIAL_MOVEMENT_SPEED 0.002f // in meters per frame
#define QWERTY_HMD_INITIAL_LOOK_SPEED 0.02f      // in radians per frame
#define QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED 0.005f
#define QWERTY_CONTROLLER_INITIAL_LOOK_SPEED 0.05f

struct qwerty_device
{
	struct xrt_device base;
	struct xrt_pose pose;

	float movement_speed;
	bool left_pressed;
	bool right_pressed;
	bool forward_pressed;
	bool backward_pressed;
	bool up_pressed;
	bool down_pressed;

	float look_speed;
	bool look_left_pressed;
	bool look_right_pressed;
	bool look_up_pressed;
	bool look_down_pressed;

	// How much extra yaw and pitch to add for the next pose. Then reset to 0.
	float yaw_delta;
	float pitch_delta;
};

static inline struct qwerty_device *
qwerty_device(struct xrt_device *xdev)
{
	return (struct qwerty_device *)xdev;
}


static void
qwerty_update_inputs(struct xrt_device *xdev)
{
	// TODO: Understand the role of this function
}

// XXX: Just noticed that the psmv version of these functions use psmv_device as
// prefix instead of just psmv check other drivers to know if it is the standard
// name style for drivers
static void
qwerty_set_output(struct xrt_device *xdev, enum xrt_output_name name, union xrt_output_value *value)
{
	// XXX: Should probably be using DEBUG macros instead of printf's

	float frequency = value->vibration.frequency;
	float amplitude = value->vibration.amplitude;
	time_duration_ns duration = value->vibration.duration;
	if (amplitude || duration || frequency) {
		printf(
		    "[QWERTY] Vibration emitted with "
		    "frequency=%f amplitude=%f duration=%ld\n",
		    frequency, amplitude, duration);
	}
}

static void
qwerty_get_tracked_pose(struct xrt_device *xdev,
                        enum xrt_input_name name,
                        uint64_t at_timestamp_ns,
                        struct xrt_space_relation *out_relation)
{
	if (name == XRT_INPUT_GENERIC_HEAD_POSE) {
		// printf(">>> XRT_INPUT_GENERIC_HEAD_POSE\n");
	} else if (name == XRT_INPUT_SIMPLE_SELECT_CLICK) {
		// printf(">>> XRT_INPUT_SIMPLE_SELECT_CLICK\n");
	} else if (name == XRT_INPUT_SIMPLE_MENU_CLICK) {
		// printf(">>> XRT_INPUT_SIMPLE_MENU_CLICK\n");
	} else if (name == XRT_INPUT_SIMPLE_GRIP_POSE) {
		// printf(">>> XRT_INPUT_SIMPLE_GRIP_POSE\n");
	} else if (name == XRT_INPUT_SIMPLE_AIM_POSE) {
		// printf(">>> XRT_INPUT_SIMPLE_AIM_POSE\n");
	} else {
		// XXX: Using unsigned, what should I use to be more specific for a enum? uint32_t?
		printf("[QWERTY ERROR] Unknown input name = %d\n", (unsigned)name);
	}

	struct qwerty_device *qh = qwerty_device(xdev);

	// Position

	struct xrt_vec3 pos_delta = {
	    qh->movement_speed * (qh->right_pressed - qh->left_pressed),
	    0, // Up/down movement will be global
	    qh->movement_speed * (qh->backward_pressed - qh->forward_pressed),
	};
	math_quat_rotate_vec3(&qh->pose.orientation, &pos_delta, &pos_delta);
	pos_delta.y += qh->movement_speed * (qh->up_pressed - qh->down_pressed);
	math_vec3_accum(&pos_delta, &qh->pose.position);

	// Rotation

	float x_look_speed = 0.f;
	float y_look_speed = 0.f;

	// clang-format off
	if (qh->look_left_pressed) y_look_speed += qh->look_speed;
	if (qh->look_right_pressed) y_look_speed -= qh->look_speed;
	if (qh->look_up_pressed) x_look_speed += qh->look_speed;
	if (qh->look_down_pressed) x_look_speed -= qh->look_speed;
	// clang-format on
	y_look_speed += qh->yaw_delta;
	x_look_speed += qh->pitch_delta;
	qh->yaw_delta = qh->pitch_delta = 0;

	struct xrt_quat x_rotation, y_rotation;
	struct xrt_vec3 x_axis = {1, 0, 0}, y_axis = {0, 1, 0};
	math_quat_from_angle_vector(x_look_speed, &x_axis, &x_rotation);
	math_quat_from_angle_vector(y_look_speed, &y_axis, &y_rotation);
	math_quat_rotate(&qh->pose.orientation, &x_rotation, &qh->pose.orientation); // local pitch
	math_quat_rotate(&y_rotation, &qh->pose.orientation, &qh->pose.orientation); // global yaw
	math_quat_normalize(&qh->pose.orientation);

	out_relation->pose = qh->pose;
	out_relation->relation_flags = XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |
	                               XRT_SPACE_RELATION_POSITION_VALID_BIT |
	                               XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT;
}

static void
qwerty_get_view_pose(struct xrt_device *xdev,
                     struct xrt_vec3 *eye_relation,
                     uint32_t view_index,
                     struct xrt_pose *out_pose)
{
	struct xrt_pose pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
	bool adjust = view_index == 0;

	pose.position.x = eye_relation->x / 2.0f;
	pose.position.y = eye_relation->y / 2.0f;
	pose.position.z = eye_relation->z / 2.0f;

	// Adjust for left/right while also making sure there aren't any -0.f.
	if (pose.position.x > 0.0f && adjust) {
		pose.position.x = -pose.position.x;
	}
	if (pose.position.y > 0.0f && adjust) {
		pose.position.y = -pose.position.y;
	}
	if (pose.position.z > 0.0f && adjust) {
		pose.position.z = -pose.position.z;
	}

	*out_pose = pose;
}

static void
qwerty_destroy(struct xrt_device *xdev)
{
	struct qwerty_device *qh = qwerty_device(xdev);

	u_device_free(&qh->base);
}

static struct qwerty_device *
qwerty_hmd_create()
{
	// U_DEVICE_ALLOCATE makes a calloc and fill pointers to zeroed unique memory
	// the properties set are commented below
	enum u_device_alloc_flags flags = U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE;
	size_t num_inputs = 1, num_outputs = 0;
	struct qwerty_device *qh = U_DEVICE_ALLOCATE(struct qwerty_device, flags, num_inputs, num_outputs);

	// Fill qwerty specific properties
	qh->pose.orientation.w = 1.f;
	qh->movement_speed = QWERTY_HMD_INITIAL_MOVEMENT_SPEED;
	qh->look_speed = QWERTY_HMD_INITIAL_LOOK_SPEED;

	// Fill xrt_device properties
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
		printf("[QWERTY ERROR] Failed to setup basic device info\n");
		qwerty_destroy(&qh->base);
		return NULL;
	}


	// qh->base.tracking_origin // Set on alloc with
	// .type == XRT_TRACKING_TYPE_NONE
	// .offset.orientation.w = 1f and
	// .name == "No tracking" */

	// qh->base.num_binding_profiles // Set on alloc
	// qh->base.binding_profiles // Does not matter as num is zero

	// qh->base.num_inputs // Set on alloc
	// qh->base.inputs // Set on alloc with inputs[i].active == true
	qh->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

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
	return qh;
}

static struct qwerty_device *
qwerty_controller_create()
{
	struct qwerty_device *qc = U_DEVICE_ALLOCATE(struct qwerty_device, U_DEVICE_ALLOC_TRACKING_NONE, 4, 1);

	// Fill qwerty specific properties
	qc->pose.orientation.w = 1.f;
	qc->movement_speed = QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED;
	qc->look_speed = QWERTY_CONTROLLER_INITIAL_LOOK_SPEED;

	// Fill xrt_device properties
	qc->base.name = XRT_DEVICE_SIMPLE_CONTROLLER;
	qc->base.device_type = XRT_DEVICE_TYPE_ANY_HAND_CONTROLLER;

	snprintf(qc->base.str, XRT_DEVICE_NAME_LEN, "Qwerty Controller");
	snprintf(qc->base.serial, XRT_DEVICE_NAME_LEN, "Qwerty Controller");

	// XXX: Is XRT_TRACKING_TYPE_NONE correct? Isn't that "tracking" what this
	// driver simulates? in any case, see qh->base.*_tracking_supported bools
	// unset in this controller but also on  the qwerty hmd
	qc->base.tracking_origin->type = XRT_TRACKING_TYPE_NONE;
	snprintf(qc->base.tracking_origin->name, XRT_TRACKING_NAME_LEN, "%s", "Qwerty Tracker");

	qc->base.inputs[0].name = XRT_INPUT_SIMPLE_SELECT_CLICK;
	qc->base.inputs[1].name = XRT_INPUT_SIMPLE_MENU_CLICK;
	qc->base.inputs[2].name = XRT_INPUT_SIMPLE_GRIP_POSE;
	qc->base.inputs[3].name = XRT_INPUT_SIMPLE_AIM_POSE; // XXX Understand aim inputs
	qc->base.outputs[0].name = XRT_OUTPUT_NAME_SIMPLE_VIBRATION;

	qc->base.update_inputs = qwerty_update_inputs;
	qc->base.get_tracked_pose = qwerty_get_tracked_pose;
	qc->base.set_output = qwerty_set_output;
	qc->base.destroy = qwerty_destroy;
	return qc;
}

int
qwerty_found(struct xrt_prober *xp,
             struct xrt_prober_device **devices,
             size_t num_devices,
             size_t index,
             cJSON *attached_data,
             struct xrt_device **out_xdevs)
{
	struct qwerty_device *qhmd = qwerty_hmd_create();
	struct qwerty_device *qctrl_left = qwerty_controller_create();
	struct qwerty_device *qctrl_right = qwerty_controller_create();

	out_xdevs[0] = &qhmd->base;
	out_xdevs[1] = &qctrl_left->base;
	out_xdevs[2] = &qctrl_right->base;
	return 3;
}

// Emulated actions

// XXX Using `qh` for "qwerty headset" even though now this work for controllers as well
// clang-format off
void qwerty_press_left(struct xrt_device *qh) { qwerty_device(qh)->left_pressed = true; }
void qwerty_release_left(struct xrt_device *qh) { qwerty_device(qh)->left_pressed = false; }
void qwerty_press_right(struct xrt_device *qh) { qwerty_device(qh)->right_pressed = true; }
void qwerty_release_right(struct xrt_device *qh) { qwerty_device(qh)->right_pressed = false; }
void qwerty_press_forward(struct xrt_device *qh) { qwerty_device(qh)->forward_pressed = true; }
void qwerty_release_forward(struct xrt_device *qh) { qwerty_device(qh)->forward_pressed = false; }
void qwerty_press_backward(struct xrt_device *qh) { qwerty_device(qh)->backward_pressed = true; }
void qwerty_release_backward(struct xrt_device *qh) { qwerty_device(qh)->backward_pressed = false; }
void qwerty_press_up(struct xrt_device *qh) { qwerty_device(qh)->up_pressed = true; }
void qwerty_release_up(struct xrt_device *qh) { qwerty_device(qh)->up_pressed = false; }
void qwerty_press_down(struct xrt_device *qh) { qwerty_device(qh)->down_pressed = true; }
void qwerty_release_down(struct xrt_device *qh) { qwerty_device(qh)->down_pressed = false; }

void qwerty_press_look_left(struct xrt_device *qh) { qwerty_device(qh)->look_left_pressed = true; }
void qwerty_release_look_left(struct xrt_device *qh) { qwerty_device(qh)->look_left_pressed = false; }
void qwerty_press_look_right(struct xrt_device *qh) { qwerty_device(qh)->look_right_pressed = true; }
void qwerty_release_look_right(struct xrt_device *qh) { qwerty_device(qh)->look_right_pressed = false; }
void qwerty_press_look_up(struct xrt_device *qh) { qwerty_device(qh)->look_up_pressed = true; }
void qwerty_release_look_up(struct xrt_device *qh) { qwerty_device(qh)->look_up_pressed = false; }
void qwerty_press_look_down(struct xrt_device *qh) { qwerty_device(qh)->look_down_pressed = true; }
void qwerty_release_look_down(struct xrt_device *qh) { qwerty_device(qh)->look_down_pressed = false; }
// clang-format on

void
qwerty_release_all(struct xrt_device *xdev)
{
	struct qwerty_device *qdev = qwerty_device(xdev);
	qdev->left_pressed = false;
	qdev->right_pressed = false;
	qdev->forward_pressed = false;
	qdev->backward_pressed = false;
	qdev->up_pressed = false;
	qdev->down_pressed = false;
	qdev->look_left_pressed = false;
	qdev->look_right_pressed = false;
	qdev->look_up_pressed = false;
	qdev->look_down_pressed = false;
}

void
qwerty_add_look_delta(struct xrt_device *xdev, float yaw, float pitch)
{
	struct qwerty_device *qdev = qwerty_device(xdev);
	qdev->yaw_delta += yaw * qdev->look_speed;
	qdev->pitch_delta += pitch * qdev->look_speed;
}
