/*!
 * @file
 * @brief Internal header for qwerty_device and its friends.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */
#pragma once

#include "xrt/xrt_device.h"
#include "util/u_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @addtogroup drv_qwerty
 * @{
 */

// XXXASK: qwerty_system uses the same concept as hydra_system and
// survive_system but they derive from xrt_tracking_origin

//! Container of qwerty devices and driver properties.
struct qwerty_system
{
	struct qwerty_hmd *hmd;
	struct qwerty_controller *lctrl;
	struct qwerty_controller *rctrl;
	enum u_logging_level ll;
	bool process_keys; //!< If false disable keyboard and mouse input
};

//! Fake device that modifies its tracked pose through its methods.
//! @implements xrt_device
struct qwerty_device
{
	struct xrt_device base;
	struct xrt_pose pose;      //!< Internal pose state
	struct qwerty_system *sys; //!< Reference to the system this device is in.

	float movement_speed; //!< In meters per frame
	bool left_pressed;
	bool right_pressed;
	bool forward_pressed;
	bool backward_pressed;
	bool up_pressed;
	bool down_pressed;

	float look_speed; //!< In radians per frame
	bool look_left_pressed;
	bool look_right_pressed;
	bool look_up_pressed;
	bool look_down_pressed;

	float yaw_delta;   //!< How much extra yaw to add for the next pose. Then reset to 0.
	float pitch_delta; //!< Similar to `yaw`
};

//! @implements qwerty_device
struct qwerty_hmd
{
	struct qwerty_device base;
};

//! Supports input actions and can be attached to the HMD pose.
//! @implements qwerty_device
struct qwerty_controller
{
	struct qwerty_device base;

	bool select_clicked; //!< input/select/click.
	bool menu_clicked;   //!< input/menu/click

	// XXXFUT: Would be nice for it to also work with non-qwerty HMDs.
	/*!
	 * Only used when a qwerty_hmd exists in the system.
	 * Use qwerty_follow_hmd() for setting it.
	 * If true, `pose` is relative to the qwerty_hmd.
	 */
	bool follow_hmd;
};

/*!
 * @name Qwerty System
 * qwerty_system methods
 * @{
 */

struct qwerty_system *
qwerty_system_create(struct qwerty_hmd *qhmd,
                     struct qwerty_controller *qleft,
                     struct qwerty_controller *qright,
                     enum u_logging_level log_level);

/*!
 * @}
 */

/*!
 * @name Qwerty Device
 * qwerty_device methods
 * @{
 */

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
// clang-format on

//! Add yaw and pitch movement for the next frame
void
qwerty_add_look_delta(struct qwerty_device *qd, float yaw, float pitch);

//! Change movement speed in steps which are usually integers, though any float is allowed.
void
qwerty_change_movement_speed(struct qwerty_device *qd, float steps);

//! Release all movement input
void
qwerty_release_all(struct qwerty_device *qd);

/*!
 * @}
 */

/*!
 * @name Qwerty HMD
 * qwerty_hmd methods
 * @{
 */

struct qwerty_hmd *
qwerty_hmd_create();

//! Cast to qwerty_hmd. Ensures returning a valid HMD or crashing.
struct qwerty_hmd *
qwerty_hmd(struct xrt_device *xd);

/*!
 * @}
 */

/*!
 * @name Qwerty Controller
 * qwerty_controller methods
 * @{
 */

struct qwerty_controller *
qwerty_controller_create(bool is_left, struct qwerty_hmd *qhmd);

//! Cast to qwerty_controller. Ensures returning a valid controller or crashing.
struct qwerty_controller *
qwerty_controller(struct xrt_device *xd);

//! Simulate input/select/click
void
qwerty_select_click(struct qwerty_controller *qc);

//! Simulate input/menu/click
void
qwerty_menu_click(struct qwerty_controller *qc);

//! Attach/detach the pose of `qc` to its HMD. Only works when a qwerty_hmd is present.
void
qwerty_follow_hmd(struct qwerty_controller *qc, bool follow);

//! Resets controller to initial pose and makes it follow the HMD
void
qwerty_reset_controller_pose(struct qwerty_controller *qc);

/*!
 * @}
 */

/*!
 * @}
 */

#ifdef __cplusplus
}
#endif
