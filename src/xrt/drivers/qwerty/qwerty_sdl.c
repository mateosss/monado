// Connection between SDL and qwerty drivers, translates user input into driver actions.
// XXX: Proper documentation for this file

#include "xrt/xrt_device.h"
#include "qwerty_interface.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

void
qwerty_try_process_inputs(struct xrt_device **xdevs, SDL_Event event)
{

  // XXX: Get using_qwerty condition from somewhere else.
  // Maybe add a new xrt_device_name and scan xdevs? An environment var?
  // This is how I was doing it before enabling non-qwerty hmds to work:
  // 	struct xrt_device *hmd = p->base.xdevs[0];
  // 	bool using_qwerty = hmd != NULL && !strcmp(hmd->serial, "Qwerty HMD");
  // EDIT: Using QWERTY_ENABLE env var while also documenting qwerty for now just works
  // in two setups: alone, or just with another hmd, not with another controllers
  bool using_qwerty = true;
  if (!using_qwerty) return;

	// XXX: Think about a better way of obtaining qwerty_devices from xdevs than
	// hardcoding xdevs[] indices. Maybe adding a QWERTY xrt_device_name?
	struct xrt_device *qhmd = xdevs[0]; // This might not be a qwerty HMD
	struct xrt_device *qleft = xdevs[1]; // XXX: Why q prefix? This should be xleft
	struct xrt_device *qright = xdevs[2];

	bool using_qhmd = qwerty_hmd_available(qleft);

	// clang-format off
	// XXX: I'm definitely pushing some limits with so much clang-format off
	// it is mainly for the oneline ifs that I think are more readable

	// XXX: This static vars seem a bad idea, maybe should use
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
		if (using_qhmd) qwerty_release_all(qhmd);
		qwerty_release_all(qright);
		qwerty_release_all(qleft);
	}

	struct xrt_device *qdev; // Focused device
	if (ctrl_pressed) qdev = qleft;
	else if (alt_pressed) qdev = qright;
	else if (using_qhmd) qdev = qhmd;
	else /* if (!using_qhmd) */ qdev = qright;

	// Default controller for methods that only make sense for controllers.
	// Right one because some window managers capture alt+click actions before SDL
	struct xrt_device *qctrl = qdev != qhmd ? qdev : qright;

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

	// XXX: The behaviour of the F key is not consistent to that of the R key in that touching F without ctrl or alt does not affect both controllers.
	// XXX: Look for other inconsistencies in the shortcuts usage and improve it

	// Controllers follow/unfollow HMD
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f && event.key.repeat == 0) {
		if (qdev != qhmd) qwerty_toggle_follow_hmd(qdev);
		else { // If no controller is focused, set both to the same state
			bool both_not_following = !qwerty_get_follow_hmd(qleft) && !qwerty_get_follow_hmd(qright);
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
