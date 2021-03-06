/*!
 * @file
 * @brief Connection between user-generated SDL events and qwerty devices.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

#include "xrt/xrt_device.h"
#include "qwerty_device.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

void
qwerty_process_event(struct xrt_device **xdevs, SDL_Event event)
{
	// XXXFUT: Think about a better way of obtaining qwerty_devices from xdevs than
	// hardcoding xdevs[] indices. Maybe adding a QWERTY xrt_device_name?

	// XXXASK: Precondition xdevs[1] is a qwerty left controller
	struct qwerty_controller *qleft = qwerty_controller(xdevs[1]);
	struct qwerty_device *qd_left = &qleft->base;

	// XXXASK: Precondition xdevs[2] is a qwerty right controller
	struct qwerty_controller *qright = qwerty_controller(xdevs[2]);
	struct qwerty_device *qd_right = &qright->base;

	// At this point xdevs[0] might not be a qwerty HMD, because the autoprober
	// does not create a qwerty hmd if it was told an hmd was found before qwerty
	// probing. However we are "sure", because it will break otherwise, that
	// xd_left is a left qwerty_controller and so we can get qwerty info from it.
	// XXXASK: Some mechanism should be in place to assert that about xd_left
	bool using_qhmd = qd_left->sys->hmd != NULL;
	struct qwerty_hmd *qhmd = using_qhmd ? qwerty_hmd(xdevs[0]) : NULL;
	struct qwerty_device *qd_hmd = using_qhmd ? &qhmd->base : NULL;

	if (!qd_left->sys->process_keys)
		return;

	// clang-format off
	// XXX: I'm definitely pushing some limits with so much clang-format off
	// it is mainly for the oneline ifs that I think are more readable

	// XXXFUT: This static vars seem a bad idea, maybe should use
	// SDL_GetKeyboardState but I quickly read that is only for SDL2 and I
	// think monado should support SDL1 as well. Also this `*_pressed = true`
	// logic is repeated in qwerty_device, smells bad.
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

	struct qwerty_device *qdev; // Focused device
	if (ctrl_pressed) qdev = qd_left;
	else if (alt_pressed) qdev = qd_right;
	else if (using_qhmd) qdev = qd_hmd;
	else /* if (!using_qhmd) */ qdev = qd_right;

	// Default controller for methods that only make sense for controllers.
	// qright default: some window managers capture alt+click actions before SDL
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
		if (ctrl_pressed) qwerty_reset_controller_pose(qleft);
		else if (alt_pressed) qwerty_reset_controller_pose(qright);
		else {
			qwerty_reset_controller_pose(qleft);
			qwerty_reset_controller_pose(qright);
		}
	}

	// Movement speed
	if (event.type == SDL_MOUSEWHEEL) qwerty_change_movement_speed(qdev, event.wheel.y);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_KP_PLUS) qwerty_change_movement_speed(qdev, 1);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_KP_MINUS) qwerty_change_movement_speed(qdev, -1);

	// Sprinting
	float sprint_steps = 5;
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, sprint_steps);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, -sprint_steps);
	// clang-format on

	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
		SDL_SetRelativeMouseMode(false);
	if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON_RMASK) {
		SDL_SetRelativeMouseMode(true);
		float sensitivity = 0.1f; // 1px moves `sensitivity` look_speed units
		float yaw = -event.motion.xrel * sensitivity;
		float pitch = -event.motion.yrel * sensitivity;
		qwerty_add_look_delta(qdev, yaw, pitch);
	}
}
