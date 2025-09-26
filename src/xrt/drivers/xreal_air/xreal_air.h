/*!
 * @file
 * @brief  Xreal Air
 * @ingroup drv_xreal_air
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "util/u_logging.h"

#include "xrt/xrt_device.h"
#include "xrt/xrt_frame.h"
#include "xrt/xrt_prober.h"
#include "xrt/xrt_tracking.h"

struct xreal_air_camera_calibration {
	struct xrt_size resolution;
	struct xrt_vec2 camera_center; /* cc */
	struct xrt_vec2 focal_length; /* fc */
	struct xrt_vec3 imu_p_cam;
	struct xrt_quat imu_q_cam; /* direction in which the camera looks? */
	float kc[12];
};

struct xreal_air_parsed_calibration
{
	struct xrt_vec3 accel_bias;
	struct xrt_quat accel_q_gyro;
	struct xrt_vec3 gyro_bias;
	struct xrt_quat gyro_q_mag;
	struct xrt_vec3 mag_bias;

	struct xrt_vec3 scale_accel;
	struct xrt_vec3 scale_gyro;
	struct xrt_vec3 scale_mag;

	float imu_noises[4];

	/* Camera */
	struct xreal_air_camera_calibration slam_camera[2];
};

struct xreal_air_system
{
	struct xrt_tracking_origin base;
	struct xrt_reference ref;

	enum u_logging_level log_level;

	struct xreal_air_parsed_calibration calibration;
	struct xrt_frame_context xfctx;

	struct xreal_air_hmd *hmd;
	struct xreal_air_tracker *tracker;
	struct xreal_air_camera *camera;
};

struct xreal_air_system *
xreal_air_system_create(struct xrt_prober *xp,
                        struct xrt_frame_context *xfctx,
                        const unsigned char *hmd_serial_no,
                        struct os_hid_device *hid_imu,
                        struct os_hid_device *hid_control,
                        int max_sensor_buffer_size,
                        enum u_logging_level log_level);

void xreal_air_system_reference(struct xreal_air_system **dst, struct xreal_air_system *src);


#ifdef __cplusplus
}
#endif
