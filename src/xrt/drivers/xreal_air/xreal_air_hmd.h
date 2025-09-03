// Copyright 2023-2025, Tobias Frisch
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Xreal Air packet parsing implementation.
 * @author Tobias Frisch <jacki@thejackimonster.de>
 * @ingroup drv_xreal_air
 */

#pragma once

#include "xrt/xrt_device.h"
#include "xrt/xrt_prober.h"

#include "os/os_hid.h"

#include "xreal_air_camera.h"

#include "util/u_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XREAL_AIR_MSG_R_BRIGHTNESS 0x03
#define XREAL_AIR_MSG_W_BRIGHTNESS 0x04
#define XREAL_AIR_MSG_R_DISP_MODE 0x07
#define XREAL_AIR_MSG_W_DISP_MODE 0x08

#define XREAL_AIR_MSG_P_START_HEARTBEAT 0x6c02
#define XREAL_AIR_MSG_P_DISPLAY_TOGGLED 0x6C04
#define XREAL_AIR_MSG_P_BUTTON_PRESSED 0x6C05
#define XREAL_AIR_MSG_P_END_HEARTBEAT 0x6c12
#define XREAL_AIR_MSG_P_ASYNC_TEXT_LOG 0x6c09

#define XREAL_AIR_BUTTON_PHYS_DISPLAY_TOGGLE 0x1
#define XREAL_AIR_BUTTON_PHYS_BRIGHTNESS_UP 0x2
#define XREAL_AIR_BUTTON_PHYS_BRIGHTNESS_DOWN 0x3

#define XREAL_AIR_BUTTON_VIRT_DISPLAY_TOGGLE 0x1
#define XREAL_AIR_BUTTON_VIRT_MENU_TOGGLE 0x3
#define XREAL_AIR_BUTTON_VIRT_BRIGHTNESS_UP 0x6
#define XREAL_AIR_BUTTON_VIRT_BRIGHTNESS_DOWN 0x7
#define XREAL_AIR_BUTTON_VIRT_UP 0x8
#define XREAL_AIR_BUTTON_VIRT_DOWN 0x9
#define XREAL_AIR_BUTTON_VIRT_MODE_2D 0xA
#define XREAL_AIR_BUTTON_VIRT_MODE_3D 0xB
#define XREAL_AIR_BUTTON_VIRT_BLEND_CYCLE 0xC
#define XREAL_AIR_BUTTON_VIRT_CONTROL_TOGGLE 0xF

#define XREAL_AIR_BLEND_STATE_LOW 0x0
#define XREAL_AIR_BLEND_STATE_DEFAULT 0x1
#define XREAL_AIR_BLEND_STATE_MEDIUM 0x2
#define XREAL_AIR_BLEND_STATE_FULL 0x3

#define XREAL_AIR_CONTROL_MODE_BRIGHTNESS 0x0
#define XREAL_AIR_CONTROL_MODE_VOLUME 0x1

#define XREAL_AIR_BRIGHTNESS_MIN 0
#define XREAL_AIR_BRIGHTNESS_MAX 7

#define XREAL_AIR_DISPLAY_MODE_2D 0x1
#define XREAL_AIR_DISPLAY_MODE_3D 0x3

#define XREAL_AIR_TICKS_PER_SECOND (1000.0) // 1 KHz ticks
#define XREAL_AIR_NS_PER_TICK (1000000)     // Each tick is a millisecond

#define XREAL_AIR_MSG_GET_CAL_DATA_LENGTH 0x14
#define XREAL_AIR_MSG_CAL_DATA_GET_NEXT_SEGMENT 0x15
#define XREAL_AIR_MSG_ALLOCATE_CAL_DATA_BUFFER 0x16
#define XREAL_AIR_MSG_WRITE_CAL_DATA_SEGMENT 0x17
#define XREAL_AIR_MSG_FREE_CAL_BUFFER 0x18
#define XREAL_AIR_MSG_START_IMU_DATA 0x19
#define XREAL_AIR_MSG_GET_STATIC_ID 0x1A
#define XREAL_AIR_MSG_UNKNOWN 0x1D

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
	struct {
		struct xrt_size resolution;
		struct xrt_vec2 camera_center; /* cc */
		struct xrt_vec2 focal_length; /* fc */
		struct xrt_vec3 imu_p_cam;
		struct xrt_quat imu_q_cam; /* direction in which the camera looks? */
		float kc[12];
	} slam_camera[2];
};

/*!
 * A parsed single gyroscope, accelerometer and
 * magnetometer sample with their corresponding
 * factors for conversion from raw data.
 *
 * @ingroup drv_xreal_air
 */
struct xreal_air_parsed_sample
{
	struct xrt_vec3_i32 accel;
	struct xrt_vec3_i32 gyro;
	struct xrt_vec3_i32 mag;

	int16_t accel_multiplier;
	int16_t gyro_multiplier;
	int16_t mag_multiplier;

	int32_t accel_divisor;
	int32_t gyro_divisor;
	int32_t mag_divisor;
};

/*!
 * Over the wire sensor packet from the glasses.
 *
 * @ingroup drv_xreal_air
 */
struct xreal_air_parsed_sensor
{
	int16_t temperature;
	uint64_t timestamp;

	struct xreal_air_parsed_sample sample;
};

/*!
 * Over the wire sensor control data packet from the glasses.
 *
 * @ingroup drv_xreal_air
 */
struct xreal_air_parsed_sensor_control_data
{
	uint16_t length;
	uint8_t msgid;

	uint8_t data[512 - 8];
};

/*!
 * A control packet from the glasses in wire format.
 *
 * @ingroup drv_xreal_air
 */
struct xreal_air_parsed_control
{
	uint16_t length;
	uint64_t timestamp;
	uint16_t action;

	uint8_t data[42];
};

/*!
 * Create Xreal Air glasses.
 *
 * @ingroup drv_xreal_air
 */
struct xrt_device *
xreal_air_hmd_create_device(struct os_hid_device *sensor_device,
                            struct os_hid_device *control_device,
                            struct xreal_air_camera *camera,
                            enum u_logging_level log_level,
                            uint16_t max_sensor_buffer_size);

struct xreal_air_hmd;
struct xreal_air_parsed_calibration *
xreal_air_hmd_get_callibration(struct xreal_air_hmd *hmd);

bool
xreal_air_parse_calibration_buffer(struct xreal_air_parsed_calibration *calibration, const char *buffer, size_t size);

bool
xreal_air_parse_sensor_packet(struct xreal_air_parsed_sensor *sensor,
                              const uint8_t *buffer,
                              size_t size,
                              size_t max_size);

bool
xreal_air_parse_sensor_control_data_packet(struct xreal_air_parsed_sensor_control_data *data,
                                           const uint8_t *buffer,
                                           size_t size,
                                           size_t max_size);

bool
xreal_air_parse_control_packet(struct xreal_air_parsed_control *control, const uint8_t *buffer, int size);

#ifdef __cplusplus
}
#endif
