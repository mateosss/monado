#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define QWERTY_VID 0x1a2c
#define QWERTY_PID 0x2c27

int
qwerty_found(struct xrt_prober *xp,
             struct xrt_prober_device **devices,
             size_t num_devices,
             size_t index,
             cJSON *attached_data,
             struct xrt_device **out_xdevs);

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

#ifdef __cplusplus
}
#endif
