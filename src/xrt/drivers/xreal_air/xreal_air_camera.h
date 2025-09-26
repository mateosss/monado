#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xreal_air_hmd.h"

// eat this shit
struct xreal_air_tracker;
struct xreal_air_camera;

struct xreal_air_camera *
xreal_air_camera_create(struct xrt_prober *xp,
                        struct xrt_frame_context *xfctx);
void xreal_air_camera_destroy(struct xreal_air_camera *camera);


