#pragma once

#include "tracking/t_tracking.h"

#include "xreal_air.h"

#ifdef __cplusplus
extern "C" {
#endif

struct t_camera_calibration
xreal_air_get_cam_calib(struct xreal_air_camera_calibration *callib);

#ifdef __cplusplus
}
#endif
