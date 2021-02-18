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

#ifdef __cplusplus
}
#endif
