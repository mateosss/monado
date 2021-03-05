// XXX: Check includes order in all files
// XXX: Should I be more explicit with my includes?

#include "qwerty_device.h"

#include "util/u_device.h"
#include "util/u_distortion_mesh.h"
#include "util/u_var.h"

#include "math/m_api.h"
#include "math/m_space.h"
#include "math/m_mathinclude.h"

#include "xrt/xrt_device.h"

#include <stdio.h>

#define QWERTY_HMD_INITIAL_MOVEMENT_SPEED 0.002f // in meters per frame
#define QWERTY_HMD_INITIAL_LOOK_SPEED 0.02f      // in radians per frame
#define QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED 0.005f
#define QWERTY_CONTROLLER_INITIAL_LOOK_SPEED 0.05f
#define MOVEMENT_SPEED_STEP 1.25f // Multiplier for how fast will mov speed increase/decrease

// clang-format off
// Values taken from u_device_setup_tracking_origins. CONTROLLER relative to HMD.
#define QWERTY_HMD_INITIAL_POS (struct xrt_vec3){0, 1.6f, 0}
#define QWERTY_CONTROLLER_INITIAL_POS(is_left) (struct xrt_vec3){is_left ? -0.2f : 0.2f, -0.3f, -0.5f}
// clang-format on

// Indices for fake controller input components
#define QWERTY_SELECT 0
#define QWERTY_MENU 1
#define QWERTY_GRIP 2
#define QWERTY_AIM 3
#define QWERTY_VIBRATION 0

#define QWERTY_TRACE(qd, ...) U_LOG_XDEV_IFL_T(&qd->base, qd->ll, __VA_ARGS__)
#define QWERTY_DEBUG(qd, ...) U_LOG_XDEV_IFL_D(&qd->base, qd->ll, __VA_ARGS__)
#define QWERTY_INFO(qd, ...) U_LOG_XDEV_IFL_I(&qd->base, qd->ll, __VA_ARGS__)
#define QWERTY_WARN(qd, ...) U_LOG_XDEV_IFL_W(&qd->base, qd->ll, __VA_ARGS__)
#define QWERTY_ERROR(qd, ...) U_LOG_XDEV_IFL_E(&qd->base, qd->ll, __VA_ARGS__)

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
		QWERTY_INFO(qdev, "[%s] Select click", xdev->str);
		qdev->select_clicked = false;
	}

	xdev->inputs[QWERTY_MENU].value.boolean = qdev->menu_clicked;
	if (qdev->menu_clicked) {
		QWERTY_INFO(qdev, "[%s] Menu click", xdev->str);
		qdev->menu_clicked = false;
	}

	// XXXFUT: Wasn't necessary to set input timestamp as below, why?
	// xdev->inputs[i].timestamp = os_monotonic_get_ns();
}

// XXXFUT: Just noticed that the psmv version of these functions use psmv_device as
// prefix instead of just psmv check other drivers to know if it is the standard
// name style for drivers
static void
qwerty_set_output(struct xrt_device *xdev, enum xrt_output_name name, union xrt_output_value *value)
{
	// XXX: Should probably be using DEBUG macros instead of printf's

	struct qwerty_device *qd = qwerty_device(xdev);
	float frequency = value->vibration.frequency;
	float amplitude = value->vibration.amplitude;
	time_duration_ns duration = value->vibration.duration;
	if (amplitude || duration || frequency) {
		QWERTY_INFO(qd,
		            "[%s] Haptic output: \n"
		            "\tfrequency=%.2f amplitude=%.2f duration=%ld",
		            xdev->str, frequency, amplitude, duration);
	}
}

static void
qwerty_get_tracked_pose(struct xrt_device *xdev,
                        enum xrt_input_name name,
                        uint64_t at_timestamp_ns,
                        struct xrt_space_relation *out_relation)
{
	struct qwerty_device *qd = qwerty_device(xdev);

	// XXXASK: How much nullcheck/nullcheck-print/assert/comment for function preconditions?

	// XXX: Remove these empty ifs
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
		// ANS: Enumerators in enums can have different sizes unfortunately. So just pray for these to fit
		QWERTY_ERROR(qd, "Unexpected input name = 0x%04X", name >> 8);
	}

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

	// Base Space Adjustment

	if (name == XRT_INPUT_SIMPLE_GRIP_POSE && qd->follow_hmd) {
		struct xrt_space_graph space_graph = {0};
		struct qwerty_device *qd_hmd = &qd->qdevs.hmd->base;
		m_space_graph_add_pose(&space_graph, &qd->pose);     // controller pose
		m_space_graph_add_pose(&space_graph, &qd_hmd->pose); // base space is hmd space
		m_space_graph_resolve(&space_graph, out_relation);
	} else {
		out_relation->pose = qd->pose;
	}
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
	// XXXFUT: This behaviour is different from the majority of driver's
	// get_view_pose. See if that behaviour could be better than this
	// i.e. the "avoid -0.f" and "only flip if negative" if statements.
	struct xrt_pose pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
	bool is_left = view_index == 0;
	float adjust = is_left ? -0.5f : 0.5f;
	struct xrt_vec3 eye_offset = *eye_relation;
	math_vec3_scalar_mul(adjust, &eye_offset);
	math_vec3_accum(&eye_offset, &pose.position);
	*out_pose = pose;
}

static void
qwerty_destroy(struct xrt_device *xdev)
{
	struct qwerty_device *qdev = qwerty_device(xdev);
	u_var_remove_root(qdev);
	u_device_free(&qdev->base);
}

static void
qwerty_setup_var_tracking(struct qwerty_device *qd)
{
	u_var_add_root(qd, qd->base.str, true);
	u_var_add_pose(qd, &qd->pose, "pose");
	u_var_add_f32(qd, &qd->movement_speed, "movement speed");
	u_var_add_f32(qd, &qd->look_speed, "look speed");
	u_var_add_log_level(qd, &qd->ll, "log level");
	u_var_add_gui_header(qd, NULL, "Help");
	u_var_add_ro_text(qd, "FD: focused device. FC: focused controller.", "Notation");
	u_var_add_ro_text(qd, "HMD is FD by default. Right is FC by default", "Defaults");
	u_var_add_ro_text(qd, "Hold left/right FD", "LCTRL/LALT");
	u_var_add_ro_text(qd, "Move FD", "WASDQE");
	u_var_add_ro_text(qd, "Rotate FD", "Arrow keys");
	u_var_add_ro_text(qd, "Rotate FD", "Hold right click");
	u_var_add_ro_text(qd, "Hold for movement speed", "LSHIFT");
	u_var_add_ro_text(qd, "Modify FD movement speed", "Mouse wheel");
	u_var_add_ro_text(qd, "Reset both or FC pose", "R");
	u_var_add_ro_text(qd, "Toggle both or FC parenting to HMD", "F");
}

struct qwerty_hmd *
qwerty_hmd_create()
{
	enum u_device_alloc_flags flags = U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE;
	size_t num_inputs = 1, num_outputs = 0;
	struct qwerty_hmd *qh = U_DEVICE_ALLOCATE(struct qwerty_hmd, flags, num_inputs, num_outputs);

	struct qwerty_device *qd = &qh->base;
	qd->pose.orientation.w = 1.f;
	qd->pose.position = QWERTY_HMD_INITIAL_POS;
	qd->movement_speed = QWERTY_HMD_INITIAL_MOVEMENT_SPEED;
	qd->look_speed = QWERTY_HMD_INITIAL_LOOK_SPEED;
	qd->follow_hmd = false;

	struct xrt_device *xd = &qd->base;
	xd->name = XRT_DEVICE_GENERIC_HMD;
	xd->device_type = XRT_DEVICE_TYPE_HMD;

	snprintf(xd->str, XRT_DEVICE_NAME_LEN, "Qwerty HMD");
	snprintf(xd->serial, XRT_DEVICE_NAME_LEN, "Qwerty HMD");

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

	if (!u_device_setup_split_side_by_side(xd, &info)) {
		QWERTY_ERROR(qd, "Failed to setup HMD properties");
		qwerty_destroy(xd);
		return NULL;
	}

	xd->tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	snprintf(xd->tracking_origin->name, XRT_TRACKING_NAME_LEN, "Qwerty HMD Tracker");

	xd->inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

	xd->update_inputs = qwerty_update_inputs;
	xd->get_tracked_pose = qwerty_get_tracked_pose;
	xd->get_view_pose = qwerty_get_view_pose;
	xd->destroy = qwerty_destroy;
	u_distortion_mesh_set_none(xd); // Fills xd->compute_distortion()

	qwerty_setup_var_tracking(qd);

	return qh;
}

struct qwerty_controller *
qwerty_controller_create(struct qwerty_hmd *qhmd, bool is_left)
{
	struct qwerty_controller *qc = U_DEVICE_ALLOCATE(struct qwerty_controller, U_DEVICE_ALLOC_TRACKING_NONE, 4, 1);

	struct qwerty_device *qd = &qc->base;
	qd->pose.orientation.w = 1.f;
	qd->pose.position = QWERTY_CONTROLLER_INITIAL_POS(is_left);
	qd->movement_speed = QWERTY_CONTROLLER_INITIAL_MOVEMENT_SPEED;
	qd->look_speed = QWERTY_CONTROLLER_INITIAL_LOOK_SPEED;
	qd->follow_hmd = qhmd != NULL;

	struct xrt_device *xd = &qd->base;

	xd->name = XRT_DEVICE_SIMPLE_CONTROLLER;
	xd->device_type = is_left ? XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER : XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER;

	char *side_name = is_left ? "Left" : "Right";
	snprintf(xd->str, XRT_DEVICE_NAME_LEN, "Qwerty %s Controller", side_name);
	snprintf(xd->serial, XRT_DEVICE_NAME_LEN, "Qwerty %s Controller", side_name);

	// XXXFUT: xd->*_tracking_supported bools are false. Is this semantically correct?
	xd->tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	snprintf(xd->tracking_origin->name, XRT_TRACKING_NAME_LEN, "Qwerty %s Controller Tracker", side_name);

	xd->inputs[QWERTY_SELECT].name = XRT_INPUT_SIMPLE_SELECT_CLICK;
	xd->inputs[QWERTY_MENU].name = XRT_INPUT_SIMPLE_MENU_CLICK;
	xd->inputs[QWERTY_GRIP].name = XRT_INPUT_SIMPLE_GRIP_POSE;
	xd->inputs[QWERTY_AIM].name = XRT_INPUT_SIMPLE_AIM_POSE; // XXXFUT: Understand aim inputs
	xd->outputs[QWERTY_VIBRATION].name = XRT_OUTPUT_NAME_SIMPLE_VIBRATION;

	xd->update_inputs = qwerty_update_inputs;
	xd->get_tracked_pose = qwerty_get_tracked_pose;
	xd->set_output = qwerty_set_output;
	xd->destroy = qwerty_destroy;

	qwerty_setup_var_tracking(qd);

	return qc;
}

// Emulated actions

// clang-format off
void qwerty_press_left(struct qwerty_device *qd) { qd->left_pressed = true; }
void qwerty_release_left(struct qwerty_device *qd) { qd->left_pressed = false; }
void qwerty_press_right(struct qwerty_device *qd) { qd->right_pressed = true; }
void qwerty_release_right(struct qwerty_device *qd) { qd->right_pressed = false; }
void qwerty_press_forward(struct qwerty_device *qd) { qd->forward_pressed = true; }
void qwerty_release_forward(struct qwerty_device *qd) { qd->forward_pressed = false; }
void qwerty_press_backward(struct qwerty_device *qd) { qd->backward_pressed = true; }
void qwerty_release_backward(struct qwerty_device *qd) { qd->backward_pressed = false; }
void qwerty_press_up(struct qwerty_device *qd) { qd->up_pressed = true; }
void qwerty_release_up(struct qwerty_device *qd) { qd->up_pressed = false; }
void qwerty_press_down(struct qwerty_device *qd) { qd->down_pressed = true; }
void qwerty_release_down(struct qwerty_device *qd) { qd->down_pressed = false; }

void qwerty_press_look_left(struct qwerty_device *qd) { qd->look_left_pressed = true; }
void qwerty_release_look_left(struct qwerty_device *qd) { qd->look_left_pressed = false; }
void qwerty_press_look_right(struct qwerty_device *qd) { qd->look_right_pressed = true; }
void qwerty_release_look_right(struct qwerty_device *qd) { qd->look_right_pressed = false; }
void qwerty_press_look_up(struct qwerty_device *qd) { qd->look_up_pressed = true; }
void qwerty_release_look_up(struct qwerty_device *qd) { qd->look_up_pressed = false; }
void qwerty_press_look_down(struct qwerty_device *qd) { qd->look_down_pressed = true; }
void qwerty_release_look_down(struct qwerty_device *qd) { qd->look_down_pressed = false; }

void qwerty_select_click(struct qwerty_controller *qc) { qc->base.select_clicked = true; }
void qwerty_menu_click(struct qwerty_controller *qc) { qc->base.menu_clicked = true; }
// clang-format on

bool
qwerty_get_follow_hmd(struct qwerty_controller *qc)
{
	return qc->base.follow_hmd;
}

void
qwerty_follow_hmd(struct qwerty_controller *qc, bool follow)
{
	struct qwerty_device *qd = &qc->base;
	bool no_qhmd = !qd->qdevs.hmd;
	bool not_ctrl = &qc->base == qd->qdevs.hmd;
	bool unchanged = qd->follow_hmd == follow;
	if (no_qhmd || not_ctrl || unchanged)
		return;

	struct qwerty_device *qd_hmd = &qd->qdevs.hmd->base;
	struct xrt_space_graph graph = {0};
	struct xrt_space_relation rel = {0};

	m_space_graph_add_pose(&graph, &qd->pose);
	if (follow) // From global to hmd
		m_space_graph_add_inverted_pose_if_not_identity(&graph, &qd_hmd->pose);
	else // From hmd to global
		m_space_graph_add_pose(&graph, &qd_hmd->pose);
	m_space_graph_resolve(&graph, &rel);

	qd->pose = rel.pose;
	qd->follow_hmd = follow;
}

void
qwerty_toggle_follow_hmd(struct qwerty_controller *qc)
{
	qwerty_follow_hmd(qc, !qc->base.follow_hmd);
}

void
qwerty_reset_controller_pose(struct qwerty_controller *qc)
{
	struct qwerty_device *qd = &qc->base;

	bool no_qhmd = !qd->qdevs.hmd;
	bool not_ctrl = qd == qd->qdevs.hmd;
	if (no_qhmd || not_ctrl)
		return;

	struct xrt_quat quat_identity = {0, 0, 0, 1};
	bool is_left = qd == qd->qdevs.lctrl;

	qwerty_follow_hmd(qc, true);
	struct xrt_pose pose = {quat_identity, QWERTY_CONTROLLER_INITIAL_POS(is_left)};
	qd->pose = pose;
}

void
qwerty_change_movement_speed(struct qwerty_device *qd, float steps)
{
	qd->movement_speed *= powf(MOVEMENT_SPEED_STEP, steps);
}


void
qwerty_release_all(struct qwerty_device *qd)
{
	qd->left_pressed = false;
	qd->right_pressed = false;
	qd->forward_pressed = false;
	qd->backward_pressed = false;
	qd->up_pressed = false;
	qd->down_pressed = false;
	qd->look_left_pressed = false;
	qd->look_right_pressed = false;
	qd->look_up_pressed = false;
	qd->look_down_pressed = false;
	qd->yaw_delta = 0;
	qd->pitch_delta = 0;
}

void
qwerty_add_look_delta(struct qwerty_device *qd, float yaw, float pitch)
{
	qd->yaw_delta += yaw * qd->look_speed;
	qd->pitch_delta += pitch * qd->look_speed;
}

bool
qwerty_hmd_available(struct qwerty_device *qd)
{
	return qd->qdevs.hmd != NULL;
}
