// XXX: Any SDPX/author format for the files?
#pragma once

#include "xrt/xrt_device.h"
#include "util/u_logging.h"


struct qwerty_devices
{
	struct qwerty_device *hmd;
	struct qwerty_device *lctrl;
	struct qwerty_device *rctrl;
};

struct qwerty_device
{
	struct xrt_device base;
	struct xrt_pose pose;        // Pose of controllers is relative to hmd pose if follow_hmd
	struct qwerty_devices qdevs; // References to all qwerty devices. Same in all devices.
	enum u_logging_level ll;

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

	// Controller buttons, unused for hmd
	bool select_clicked;
	bool menu_clicked;

	// Only used for controllers and when a qwerty hmd exists.
	// If true, `pose` is relative to the qwerty HMD.
	// XXXFUT: Would be nice for it to also work with non-qwerty HMDs.
	// XXX: Enough reason to separate qwerty_device into qwerty_hmd and qwerty_controller
	bool follow_hmd;

	// How much extra yaw and pitch to add for the next pose. Then reset to 0.
	float yaw_delta;
	float pitch_delta;
};

struct qwerty_hmd
{
	struct qwerty_device base;
};

struct qwerty_controller
{
	struct qwerty_device base;
};

struct qwerty_hmd *
qwerty_hmd_create();

struct qwerty_controller *
qwerty_controller_create(struct qwerty_hmd *qh, bool is_left);

// clang-format off
void qwerty_press_left(struct qwerty_device *qd);
void qwerty_release_left(struct qwerty_device *qd);
void qwerty_press_right(struct qwerty_device *qd);
void qwerty_release_right(struct qwerty_device *qd);
void qwerty_press_forward(struct qwerty_device *qd);
void qwerty_release_forward(struct qwerty_device *qd);
void qwerty_press_backward(struct qwerty_device *qd);
void qwerty_release_backward(struct qwerty_device *qd);
void qwerty_press_up(struct qwerty_device *qd);
void qwerty_release_up(struct qwerty_device *qd);
void qwerty_press_down(struct qwerty_device *qd);
void qwerty_release_down(struct qwerty_device *qd);

void qwerty_press_look_left(struct qwerty_device *qd);
void qwerty_release_look_left(struct qwerty_device *qd);
void qwerty_press_look_right(struct qwerty_device *qd);
void qwerty_release_look_right(struct qwerty_device *qd);
void qwerty_press_look_up(struct qwerty_device *qd);
void qwerty_release_look_up(struct qwerty_device *qd);
void qwerty_press_look_down(struct qwerty_device *qd);
void qwerty_release_look_down(struct qwerty_device *qd);

void qwerty_select_click(struct xrt_device *xdev);
void qwerty_menu_click(struct xrt_device *xdev);

bool qwerty_get_follow_hmd(struct xrt_device *xdev);
void qwerty_follow_hmd(struct xrt_device *xdev, bool follow);
void qwerty_toggle_follow_hmd(struct xrt_device *xdev);

// Resets controller to initial pose and makes it follow the HMD
void qwerty_reset_controller_pose(struct xrt_device *xdev);

// Change movement speed in steps which are usually integers, though any float is allowed.
void qwerty_change_movement_speed(struct qwerty_device *qd, float steps);

void qwerty_release_all(struct qwerty_device *qd);

// XXX: Put proper doxygen documentation. See in which other functions I should put it.
// Add yaw and pitch movement for the next frame
void qwerty_add_look_delta(struct qwerty_device *qd, float yaw, float pitch);

// Given an xdev qwerty device returns whether a qwerty HMD is in use or not.
bool qwerty_hmd_available(struct qwerty_device *qd);
// clang-format on
