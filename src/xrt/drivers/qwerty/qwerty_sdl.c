// Copyright 2021, Mateo de Mayo.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Connection between user-generated SDL events and qwerty devices.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

#include "qwerty_device.h"
#include "util/u_device.h"
#include "xrt/xrt_device.h"
#include <SDL2/SDL.h>
#include <assert.h>

// Amount of look_speed units a mouse delta of 1px in screen space will rotate the device
#define SENSITIVITY 0.1f

static struct qwerty_system *
find_qwerty_system(struct xrt_device **xdevs, size_t num_xdevs)
{
	struct xrt_device *xdev = NULL;
	for (size_t i = 0; i < num_xdevs; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}
		if (strcmp(xdevs[i]->str, QWERTY_HMD_STR) == 0 || strcmp(xdevs[i]->str, QWERTY_LEFT_STR) == 0 ||
		    strcmp(xdevs[i]->str, QWERTY_RIGHT_STR) == 0) {
			xdev = xdevs[i];
			break;
		}
	}

	assert(xdev != NULL && "There is no device in xdevs with the name of a qwerty device");
	struct qwerty_device *qdev = qwerty_device(xdev);
	struct qwerty_system *qsys = qdev->sys;
	assert(qsys != NULL && "The qwerty_system of a qwerty_device was null");
	return qsys;
}

// Determines the default qwerty device based on which devices are in use
static struct qwerty_device *
default_qwerty_device(struct xrt_device **xdevs, size_t num_xdevs, struct qwerty_system *qsys)
{
	int head, left, right;
	head = left = right = XRT_DEVICE_ROLE_UNASSIGNED;
	u_device_assign_xdev_roles(xdevs, num_xdevs, &head, &left, &right);

	struct xrt_device *xd_hmd = qsys->hmd ? &qsys->hmd->base.base : NULL;
	struct xrt_device *xd_left = &qsys->lctrl->base.base;
	struct xrt_device *xd_right = &qsys->rctrl->base.base;

	struct qwerty_device *default_qdev = NULL;
	if (xdevs[head] == xd_hmd) {
		default_qdev = qwerty_device(xd_hmd);
	} else if (xdevs[right] == xd_right) {
		default_qdev = qwerty_device(xd_right);
	} else if (xdevs[left] == xd_left) {
		default_qdev = qwerty_device(xd_left);
	} else { // Even here, xd_right is allocated and so we can modify it
		default_qdev = qwerty_device(xd_right);
	}

	return default_qdev;
}

// Determines the default qwerty controller based on which devices are in use
static struct qwerty_controller *
default_qwerty_controller(struct xrt_device **xdevs, size_t num_xdevs, struct qwerty_system *qsys)
{
	int head, left, right;
	head = left = right = XRT_DEVICE_ROLE_UNASSIGNED;
	u_device_assign_xdev_roles(xdevs, num_xdevs, &head, &left, &right);

	struct xrt_device *xd_left = &qsys->lctrl->base.base;
	struct xrt_device *xd_right = &qsys->rctrl->base.base;

	struct qwerty_controller *default_qctrl = NULL;
	if (xdevs[right] == xd_right) {
		default_qctrl = qwerty_controller(xd_right);
	} else if (xdevs[left] == xd_left) {
		default_qctrl = qwerty_controller(xd_left);
	} else { // Even here, xd_right is allocated and so we can modify it
		default_qctrl = qwerty_controller(xd_right);
	}

	return default_qctrl;
}

void
qwerty_process_event(struct xrt_device **xdevs, size_t num_xdevs, SDL_Event event)
{
	static struct qwerty_system *qsys = NULL;

	static bool alt_pressed = false;
	static bool ctrl_pressed = false;

	// Default focused device: the one focused when CTRL and ALT are not pressed
	static struct qwerty_device *default_qdev;
	// Default focused controller: the one used for qwerty_controller specific methods
	static struct qwerty_controller *default_qctrl;

	// We can cache the devices as they don't get destroyed during runtime
	static bool cached = false;
	if (!cached) {
		qsys = find_qwerty_system(xdevs, num_xdevs);
		default_qdev = default_qwerty_device(xdevs, num_xdevs, qsys);
		default_qctrl = default_qwerty_controller(xdevs, num_xdevs, qsys);
		cached = true;
	}

	if (!qsys->process_keys) {
		return;
	}

	// Initialize different views of the same pointers.

	struct qwerty_controller *qleft = qsys->lctrl;
	struct qwerty_device *qd_left = &qleft->base;

	struct qwerty_controller *qright = qsys->rctrl;
	struct qwerty_device *qd_right = &qright->base;

	bool using_qhmd = qsys->hmd != NULL;
	struct qwerty_hmd *qhmd = using_qhmd ? qsys->hmd : NULL;
	struct qwerty_device *qd_hmd = using_qhmd ? &qhmd->base : NULL;

	// clang-format off
	// CTRL/ALT keys logic
	bool alt_down = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LALT;
	bool alt_up = event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LALT;
	bool ctrl_down = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LCTRL;
	bool ctrl_up = event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LCTRL;
	if (alt_down) alt_pressed = true;
	if (alt_up) alt_pressed = false;
	if (ctrl_down) ctrl_pressed = true;
	if (ctrl_up) ctrl_pressed = false;

	bool change_focus = alt_down || alt_up || ctrl_down || ctrl_up;
	if (change_focus) {
		if (using_qhmd) qwerty_release_all(qd_hmd);
		qwerty_release_all(qd_right);
		qwerty_release_all(qd_left);
	}

	// Determine focused device
	struct qwerty_device *qdev;
	if (ctrl_pressed) qdev = qd_left;
	else if (alt_pressed) qdev = qd_right;
	else qdev = default_qdev;

	// Determine focused controller for qwerty_controller specific methods
	struct qwerty_controller *qctrl = qdev != qd_hmd ? qwerty_controller(&qdev->base) : default_qctrl;

	// Update gui tracked variables
	qsys->hmd_focused = qdev == qd_hmd;
	qsys->lctrl_focused = qdev == qd_left;
	qsys->rctrl_focused = qdev == qd_right;

	// WASDQE Movement
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a) qwerty_press_left(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_a) qwerty_release_left(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_d) qwerty_press_right(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_d) qwerty_release_right(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_w) qwerty_press_forward(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_w) qwerty_release_forward(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s) qwerty_press_backward(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_s) qwerty_release_backward(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_e) qwerty_press_up(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_e) qwerty_release_up(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q) qwerty_press_down(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_q) qwerty_release_down(qdev);

	// Arrow keys rotation
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT) qwerty_press_look_left(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LEFT) qwerty_release_look_left(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) qwerty_press_look_right(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RIGHT) qwerty_release_look_right(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP) qwerty_press_look_up(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_UP) qwerty_release_look_up(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) qwerty_press_look_down(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_DOWN) qwerty_release_look_down(qdev);

	// Movement speed
	if (event.type == SDL_MOUSEWHEEL) qwerty_change_movement_speed(qdev, event.wheel.y);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_KP_PLUS) qwerty_change_movement_speed(qdev, 1);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_KP_MINUS) qwerty_change_movement_speed(qdev, -1);

	// Sprinting
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LSHIFT) qwerty_press_sprint(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LSHIFT) qwerty_release_sprint(qdev);

	// Mouse rotation
	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
		SDL_SetRelativeMouseMode(false);
	}
	if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON_RMASK) {
		SDL_SetRelativeMouseMode(true);
		float yaw = -event.motion.xrel * SENSITIVITY;
		float pitch = -event.motion.yrel * SENSITIVITY;
		qwerty_add_look_delta(qdev, yaw, pitch);
	}

	// Select and menu clicks only for controllers.
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) qwerty_select_click(qctrl);
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE) qwerty_menu_click(qctrl);

	// clang-format on

	// Controllers follow/unfollow HMD
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f && event.key.repeat == 0) {
		if (qdev != qd_hmd) {
			qwerty_follow_hmd(qctrl, !qctrl->follow_hmd);
		} else { // If no controller is focused, set both to the same state
			bool both_not_following = !qleft->follow_hmd && !qright->follow_hmd;
			qwerty_follow_hmd(qleft, both_not_following);
			qwerty_follow_hmd(qright, both_not_following);
		}
	}

	// Reset controller poses
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && event.key.repeat == 0) {
		if (qdev != qd_hmd) {
			qwerty_reset_controller_pose(qctrl);
		} else { // If no controller is focused, reset both
			qwerty_reset_controller_pose(qleft);
			qwerty_reset_controller_pose(qright);
		}
	}
}
