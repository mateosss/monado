#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct xrt_auto_prober *
qwerty_create_auto_prober(void);

void qwerty_press_left(struct xrt_device *qh);
void qwerty_release_left(struct xrt_device *qh);
void qwerty_press_right(struct xrt_device *qh);
void qwerty_release_right(struct xrt_device *qh);
void qwerty_press_forward(struct xrt_device *qh);
void qwerty_release_forward(struct xrt_device *qh);
void qwerty_press_backward(struct xrt_device *qh);
void qwerty_release_backward(struct xrt_device *qh);
void qwerty_press_up(struct xrt_device *qh);
void qwerty_release_up(struct xrt_device *qh);
void qwerty_press_down(struct xrt_device *qh);
void qwerty_release_down(struct xrt_device *qh);

void qwerty_press_look_left(struct xrt_device *qh);
void qwerty_release_look_left(struct xrt_device *qh);
void qwerty_press_look_right(struct xrt_device *qh);
void qwerty_release_look_right(struct xrt_device *qh);
void qwerty_press_look_up(struct xrt_device *qh);
void qwerty_release_look_up(struct xrt_device *qh);
void qwerty_press_look_down(struct xrt_device *qh);
void qwerty_release_look_down(struct xrt_device *qh);

void qwerty_select_click(struct xrt_device *xdev);
void qwerty_menu_click(struct xrt_device *xdev);

void qwerty_follow_hmd(struct xrt_device *xdev, bool follow);
void qwerty_toggle_follow_hmd(struct xrt_device *xdev);

// Resets controller to initial pose and makes it follow the HMD
void qwerty_reset_controller_pose(struct xrt_device *xdev);
// Change movement speed in steps which are usually integers, though any float is allowed.
void qwerty_change_movement_speed(struct xrt_device *xdev, float steps);

void qwerty_release_all(struct xrt_device *xdev);

// XXX: Put proper doxygen documentation. See in which other functions I should put it.
// Add yaw and pitch movement for the next frame
void qwerty_add_look_delta(struct xrt_device *xdev, float yaw, float pitch);

#ifdef __cplusplus
}
#endif
