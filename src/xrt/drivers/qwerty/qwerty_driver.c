// TODO: Check includes order in all files
// XXX: Should I be more explicit with my includes?

#include "qwerty_device.h"

#include "util/u_device.h"
#include "util/u_distortion_mesh.h"

#include "math/m_api.h"
#include "math/m_space.h"
#include "math/m_mathinclude.h"

#include "xrt/xrt_device.h"

#include <stdio.h>

#define QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED 0.005f
#define QWERTY_CONTROLLER_INITIAL_LOOK_SPEED 0.05f
#define MOVEMENT_SPEED_STEP 1.25f // Multiplier for how fast will mov speed increase/decrease

// clang-format off
#define QWERTY_CONTROLLER_INITIAL_POS(is_left) (struct xrt_vec3){is_left ? -0.2f : 0.2f, -0.3f, -0.5f}
// clang-format on

// Indices for fake controller input components
#define QWERTY_SELECT 0
#define QWERTY_MENU 1
#define QWERTY_GRIP 2
#define QWERTY_AIM 3
#define QWERTY_VIBRATION 0

static inline struct qwerty_device *
qwerty_device(struct xrt_device *xdev)
{
	return (struct qwerty_device *)xdev;
}


static void
qwerty_update_inputs(struct xrt_device *xdev)
{
	struct qwerty_device *qdev = qwerty_device(xdev);

	xdev->inputs[QWERTY_SELECT].value.boolean = qdev->select_clicked;
	if (qdev->select_clicked) {
		printf("[QWERTY] [%s] Select click\n", xdev->str);
		qdev->select_clicked = false;
	}

	xdev->inputs[QWERTY_MENU].value.boolean = qdev->menu_clicked;
	if (qdev->menu_clicked) {
		printf("[QWERTY] [%s] Menu click\n", xdev->str);
		qdev->menu_clicked = false;
	}

	// XXX: Wasn't necessary to set input timestamp as below, why?
	// xdev->inputs[i].timestamp = os_monotonic_get_ns();
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
		    "[QWERTY] [%s] Haptic output: "
		    "freq=%.2ff ampl=%.2ff dur=%ld\n",
		    xdev->str, frequency, amplitude, duration);
	}
}

static void
qwerty_get_tracked_pose(struct xrt_device *xdev,
                        enum xrt_input_name name,
                        uint64_t at_timestamp_ns,
                        struct xrt_space_relation *out_relation)
{
	// TODO: Remove these empty ifs
	// XXX: How much nullcheck/nullcheck-print/assert/comment for function preconditions?
	// ASK

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
		// XXXANS: Using unsigned, what should I use to be more specific for a enum? uint32_t?
		// ANS: Enumerators in enums can have different sizes unfortunately. So just pray for these enumerators to fit into
		printf("[QWERTY ERROR] Unexpected input name = 0x%04X\n", name >> 8);
	}

	struct qwerty_device *qd = qwerty_device(xdev);

	// Position

	struct xrt_vec3 pos_delta = {
	    qd->movement_speed * (qd->right_pressed - qd->left_pressed),
	    0, // Up/down movement will be global
	    qd->movement_speed * (qd->backward_pressed - qd->forward_pressed),
	};
	math_quat_rotate_vec3(&qd->pose.orientation, &pos_delta, &pos_delta);
	pos_delta.y += qd->movement_speed * (qd->up_pressed - qd->down_pressed);
	math_vec3_accum(&pos_delta, &qd->pose.position);

	// Rotation

	// View rotation caused by keys
	float y_look_speed = qd->look_speed * (qd->look_left_pressed - qd->look_right_pressed);
	float x_look_speed = qd->look_speed * (qd->look_up_pressed - qd->look_down_pressed);

	// View rotation caused by mouse
	y_look_speed += qd->yaw_delta;
	x_look_speed += qd->pitch_delta;
	qd->yaw_delta = qd->pitch_delta = 0;

	struct xrt_quat x_rotation, y_rotation;
	struct xrt_vec3 x_axis = {1, 0, 0}, y_axis = {0, 1, 0};
	math_quat_from_angle_vector(x_look_speed, &x_axis, &x_rotation);
	math_quat_from_angle_vector(y_look_speed, &y_axis, &y_rotation);
	math_quat_rotate(&qd->pose.orientation, &x_rotation, &qd->pose.orientation); // local-space pitch
	math_quat_rotate(&y_rotation, &qd->pose.orientation, &qd->pose.orientation); // base-space yaw
	math_quat_normalize(&qd->pose.orientation);


	out_relation->pose = qd->pose;
	out_relation->relation_flags = XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |
	                               XRT_SPACE_RELATION_POSITION_VALID_BIT |
	                               XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT;
}

static void
qwerty_destroy(struct xrt_device *xdev)
{
	struct qwerty_device *qdev = qwerty_device(xdev);
	u_device_free(&qdev->base);
}

struct qwerty_device *
qwerty_controller_create(bool is_left)
{
	struct qwerty_device *qc = U_DEVICE_ALLOCATE(struct qwerty_device, U_DEVICE_ALLOC_TRACKING_NONE, 4, 1);

	// Fill qwerty specific properties
	qc->pose.orientation.w = 1.f;
	qc->pose.position = QWERTY_CONTROLLER_INITIAL_POS(is_left);
	qc->movement_speed = QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED;
	qc->look_speed = QWERTY_CONTROLLER_INITIAL_LOOK_SPEED;

	// Fill xrt_device properties
	qc->base.name = XRT_DEVICE_SIMPLE_CONTROLLER;
	qc->base.device_type = is_left ? XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER : XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER;

	char *side_name = is_left ? "Left" : "Right";
	snprintf(qc->base.str, XRT_DEVICE_NAME_LEN, "Qwerty %s Controller", side_name);
	snprintf(qc->base.serial, XRT_DEVICE_NAME_LEN, "Qwerty %s Controller", side_name);

	// XXX: qc->base.*_tracking_supported bools are false. Is this semantically correct?
	qc->base.tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	snprintf(qc->base.tracking_origin->name, XRT_TRACKING_NAME_LEN, "%s", "Qwerty Tracker");

	qc->base.inputs[QWERTY_SELECT].name = XRT_INPUT_SIMPLE_SELECT_CLICK;
	qc->base.inputs[QWERTY_MENU].name = XRT_INPUT_SIMPLE_MENU_CLICK;
	qc->base.inputs[QWERTY_GRIP].name = XRT_INPUT_SIMPLE_GRIP_POSE;
	qc->base.inputs[QWERTY_AIM].name = XRT_INPUT_SIMPLE_AIM_POSE; // XXX: Understand aim inputs
	qc->base.outputs[QWERTY_VIBRATION].name = XRT_OUTPUT_NAME_SIMPLE_VIBRATION;

	qc->base.update_inputs = qwerty_update_inputs;
	qc->base.get_tracked_pose = qwerty_get_tracked_pose;
	qc->base.set_output = qwerty_set_output;
	qc->base.destroy = qwerty_destroy;
	return qc;
}

// Emulated actions

// clang-format off
void qwerty_press_left(struct xrt_device *qd) { qwerty_device(qd)->left_pressed = true; }
void qwerty_release_left(struct xrt_device *qd) { qwerty_device(qd)->left_pressed = false; }
void qwerty_press_right(struct xrt_device *qd) { qwerty_device(qd)->right_pressed = true; }
void qwerty_release_right(struct xrt_device *qd) { qwerty_device(qd)->right_pressed = false; }
void qwerty_press_forward(struct xrt_device *qd) { qwerty_device(qd)->forward_pressed = true; }
void qwerty_release_forward(struct xrt_device *qd) { qwerty_device(qd)->forward_pressed = false; }
void qwerty_press_backward(struct xrt_device *qd) { qwerty_device(qd)->backward_pressed = true; }
void qwerty_release_backward(struct xrt_device *qd) { qwerty_device(qd)->backward_pressed = false; }
void qwerty_press_up(struct xrt_device *qd) { qwerty_device(qd)->up_pressed = true; }
void qwerty_release_up(struct xrt_device *qd) { qwerty_device(qd)->up_pressed = false; }
void qwerty_press_down(struct xrt_device *qd) { qwerty_device(qd)->down_pressed = true; }
void qwerty_release_down(struct xrt_device *qd) { qwerty_device(qd)->down_pressed = false; }

void qwerty_press_look_left(struct xrt_device *qd) { qwerty_device(qd)->look_left_pressed = true; }
void qwerty_release_look_left(struct xrt_device *qd) { qwerty_device(qd)->look_left_pressed = false; }
void qwerty_press_look_right(struct xrt_device *qd) { qwerty_device(qd)->look_right_pressed = true; }
void qwerty_release_look_right(struct xrt_device *qd) { qwerty_device(qd)->look_right_pressed = false; }
void qwerty_press_look_up(struct xrt_device *qd) { qwerty_device(qd)->look_up_pressed = true; }
void qwerty_release_look_up(struct xrt_device *qd) { qwerty_device(qd)->look_up_pressed = false; }
void qwerty_press_look_down(struct xrt_device *qd) { qwerty_device(qd)->look_down_pressed = true; }
void qwerty_release_look_down(struct xrt_device *qd) { qwerty_device(qd)->look_down_pressed = false; }

void qwerty_select_click(struct xrt_device *xdev) { qwerty_device(xdev)->select_clicked = true; }
void qwerty_menu_click(struct xrt_device *xdev) { qwerty_device(xdev)->menu_clicked = true; }
// clang-format on

void
qwerty_reset_controller_pose(struct xrt_device *xdev)
{
	struct qwerty_device *qctrl = qwerty_device(xdev);

	struct xrt_quat quat_identity = {0, 0, 0, 1};
	bool is_left = qctrl == qctrl->qdevs.lctrl;

	struct xrt_pose pose = {quat_identity, QWERTY_CONTROLLER_INITIAL_POS(is_left)};
	qctrl->pose = pose;
}

void
qwerty_change_movement_speed(struct xrt_device *xdev, float steps)
{
	qwerty_device(xdev)->movement_speed *= powf(MOVEMENT_SPEED_STEP, steps);
}


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
