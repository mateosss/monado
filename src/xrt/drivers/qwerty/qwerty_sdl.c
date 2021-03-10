/*!
 * @file
 * @brief Connection between user-generated SDL events and qwerty devices.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

#include "qwerty_device.h"
#include "xrt/xrt_device.h"
#include <SDL2/SDL.h>

// Amount of movement_speed steps to increase when sprinting
#define SPRINT_STEPS 5

// Amount of look_speed units a mouse delta of 1px in screen space will rotate the device
#define SENSITIVITY 0.1f

static void
find_qwerty_devices(struct xrt_device **xdevs,
                    size_t num_xdevs,
                    struct xrt_device **xd_hmd,
                    struct xrt_device **xd_left,
                    struct xrt_device **xd_right)
{
	for (size_t i = 0; i < num_xdevs; i++) {
		if (xdevs[i] == NULL)
			continue;
		else if (strcmp(xdevs[i]->str, QWERTY_HMD_STR) == 0)
			*xd_hmd = xdevs[i];
		else if (strcmp(xdevs[i]->str, QWERTY_LEFT_STR) == 0)
			*xd_left = xdevs[i];
		else if (strcmp(xdevs[i]->str, QWERTY_RIGHT_STR) == 0)
			*xd_right = xdevs[i];
	}
}

void
qwerty_process_event(struct xrt_device **xdevs, size_t num_xdevs, SDL_Event event)
{
	static struct xrt_device *xd_hmd = NULL;
	static struct xrt_device *xd_left = NULL;
	static struct xrt_device *xd_right = NULL;

	// We can cache the devices as they don't get destroyed during runtime
	static bool devices_cached = false;
	if (!devices_cached) {
		find_qwerty_devices(xdevs, num_xdevs, &xd_hmd, &xd_left, &xd_right);
		devices_cached = true;
	}

	struct qwerty_controller *qleft = qwerty_controller(xd_left);
	struct qwerty_device *qd_left = &qleft->base;

	struct qwerty_controller *qright = qwerty_controller(xd_right);
	struct qwerty_device *qd_right = &qright->base;

	bool using_qhmd = qd_left->sys->hmd != NULL;
	struct qwerty_hmd *qhmd = using_qhmd ? qwerty_hmd(xd_hmd) : NULL;
	struct qwerty_device *qd_hmd = using_qhmd ? &qhmd->base : NULL;

	struct qwerty_system *qsys = qd_left->sys;
	if (!qsys->process_keys)
		return;

	// clang-format off
	static bool alt_pressed = false;
	static bool ctrl_pressed = false;

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

	// Default focused device
	struct qwerty_device *default_qdev = using_qhmd ? qd_hmd : qd_right;

	// Determine focused device
	struct qwerty_device *qdev;
	if (ctrl_pressed) qdev = qd_left;
	else if (alt_pressed) qdev = qd_right;
	else qdev = default_qdev;

	// Default controller for qwerty_controller specific methods. `qright` by
	// default because some window managers capture alt+click actions before SDL
	struct qwerty_controller *qctrl = qdev == qd_hmd ? qright : qwerty_controller(&qdev->base);

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
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, SPRINT_STEPS);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, -SPRINT_STEPS);

	// Mouse rotation
	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
		SDL_SetRelativeMouseMode(false);
	if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON_RMASK) {
		SDL_SetRelativeMouseMode(true);
		float yaw = -event.motion.xrel * SENSITIVITY;
		float pitch = -event.motion.yrel * SENSITIVITY;
		qwerty_add_look_delta(qdev, yaw, pitch);
	}

	// Select and menu clicks only for controllers.
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) qwerty_select_click(qctrl);
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE) qwerty_menu_click(qctrl);

	// Controllers follow/unfollow HMD
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f && event.key.repeat == 0) {
		if (qdev != qd_hmd) qwerty_follow_hmd(qctrl, !qctrl->follow_hmd);
		else { // If no controller is focused, set both to the same state
			bool both_not_following = !qleft->follow_hmd && !qright->follow_hmd;
			qwerty_follow_hmd(qleft, both_not_following);
			qwerty_follow_hmd(qright, both_not_following);
		}
	}

	// Reset controller poses
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && event.key.repeat == 0) {
		if (qdev != qd_hmd) qwerty_reset_controller_pose(qctrl);
		else { // If no controller is focused, reset both
			qwerty_reset_controller_pose(qleft);
			qwerty_reset_controller_pose(qright);
		}
	}

	// clang-format on
}
