#include "xreal_air.h"
#include "xreal_air_hmd.h"
#include "xreal_air_tracker.h"

#include "util/u_misc.h"
#include "util/u_trace_marker.h"

#define XREAL_AIR_TRACE(...)	U_LOG_IFL_T(sys->log_level, __VA_ARGS__)
#define XREAL_AIR_INFO(...)	U_LOG_IFL_I(sys->log_level, __VA_ARGS__)
#define XREAL_AIR_DEBUG(...)	U_LOG_IFL_D(sys->log_level, __VA_ARGS__)
#define XREAL_AIR_WARN(...)	U_LOG_IFL_W(sys->log_level, __VA_ARGS__)
#define XREAL_AIR_ERROR(...)	U_LOG_IFL_E(sys->log_level, __VA_ARGS__)


struct xreal_air_system *
xreal_air_system_create(struct xrt_prober *xp,
                        struct xrt_frame_context *xfctx,
                        const unsigned char *hmd_serial_no,
                        struct os_hid_device *hid_imu,
                        struct os_hid_device *hid_control,
                        int max_sensor_buffer_size,
                        enum u_logging_level log_level)
{
	struct xreal_air_system *sys;

	DRV_TRACE_MARKER();

	sys = U_TYPED_CALLOC(struct xreal_air_system);

	sys->base.type = XRT_TRACKING_TYPE_NONE;
	sys->base.initial_offset.orientation.w = 1.0f;
	sys->log_level = log_level;
	sys->xfctx = xfctx;

	/* Init refcount */
	sys->ref.count = 1;

	sys->hmd = xreal_air_hmd_create_device(sys, hid_imu, hid_control, max_sensor_buffer_size);
	if (sys->hmd == NULL) {
		XREAL_AIR_ERROR("Failed to initialise Xreal Air driver");
		goto cleanup;
	}

	sys->tracker = xreal_air_tracker_create(sys);
	if (sys->tracker == NULL) {
		XREAL_AIR_ERROR("Failed to init tracking");
		goto cleanup;
	}

	/* At this point the callibration data is filled in */
	sys->camera = xreal_air_camera_create(xp, sys);

	XREAL_AIR_DEBUG("XReal Air driver ready");

	/* Finally, start tracking*/
	xreal_air_tracker_start(sys->tracker);

	return sys;

cleanup:
	xreal_air_system_reference(&sys, NULL);
	return NULL;
}

static void
xreal_air_system_free(struct xreal_air_system *sys)
{
	xrt_device_destroy((struct xrt_device**)&sys->hmd);
	xrt_device_destroy((struct xrt_device**)&sys->tracker);
	xreal_air_camera_destroy(sys->camera);
	free(sys);
}

/* Reference count handling for rift_s_system */
void
xreal_air_system_reference(struct xreal_air_system **dst, struct xreal_air_system *src)
{
	struct xreal_air_system *old_dst = *dst;

	if (old_dst == src) {
		return;
	}

	if (src) {
		xrt_reference_inc(&src->ref);
	}

	*dst = src;

	if (old_dst) {
		if (xrt_reference_dec_and_is_zero(&old_dst->ref)) {
			xreal_air_system_free(old_dst);
		}
	}
}
