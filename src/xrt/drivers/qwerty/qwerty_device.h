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
	// XXX: Would be nice for it to also work with non-qwerty HMDs.
	// XXX: Enough reason to separate qwerty_device into qwerty_hmd and qwerty_controller
	bool follow_hmd;

	// How much extra yaw and pitch to add for the next pose. Then reset to 0.
	float yaw_delta;
	float pitch_delta;
};

struct qwerty_device *
qwerty_hmd_create();

struct qwerty_device *
qwerty_controller_create(struct qwerty_device *qhmd, bool is_left);
