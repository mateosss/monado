#include "xrt/xrt_device.h"

#pragma once

struct qwerty_devices
{
	struct qwerty_device *lctrl;
	struct qwerty_device *rctrl;
};

struct qwerty_device
{
	struct xrt_device base;
	struct xrt_pose pose;
	struct qwerty_devices qdevs; // References to all qwerty devices. Same in all devices.

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

	bool select_clicked;
	bool menu_clicked;

	// How much extra yaw and pitch to add for the next pose. Then reset to 0.
	float yaw_delta;
	float pitch_delta;
};

struct qwerty_device *
qwerty_controller_create(bool is_left);
