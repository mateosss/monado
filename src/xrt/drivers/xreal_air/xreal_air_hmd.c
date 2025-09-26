// Copyright 2023-2025, Tobias Frisch
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Xreal Air packet parsing implementation.
 * @author Tobias Frisch <jacki@thejackimonster.de>
 * @ingroup drv_xreal_air
 */

#include "xrt/xrt_compiler.h"

#include "os/os_hid.h"
#include "os/os_time.h"
#include "os/os_threading.h"

#include "math/m_api.h"
#include "math/m_imu_3dof.h"

#include "util/u_device.h"
#include "util/u_distortion_mesh.h"
#include "util/u_trace_marker.h"
#include "util/u_var.h"

#include "xreal_air_hmd.h"
#include "xreal_air_camera.h"
#include "xreal_air_tracker.h"

#include "math/m_mathinclude.h"
#include "math/m_relation_history.h"
#include "xrt/xrt_defines.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XREAL_AIR_DEBUG(hmd, ...) U_LOG_XDEV_IFL_D(&hmd->base, hmd->log_level, __VA_ARGS__)
#define XREAL_AIR_ERROR(hmd, ...) U_LOG_XDEV_IFL_E(&hmd->base, hmd->log_level, __VA_ARGS__)

#define SENSOR_BUFFER_SIZE 512
#define CONTROL_BUFFER_SIZE 64

#define SENSOR_HEAD 0xFD
#define CONTROL_HEAD 0xFD

/*!
 * Private struct for the xreal_air device.
 *
 * @ingroup drv_xreal_air
 * @implements xrt_device
 */
struct xreal_air_hmd
{
	struct xrt_device base;

	struct xreal_air_system *sys;

	//! Owned by the @ref oth thread.
	struct os_hid_device *hid_sensor;

	//! Owned and protected by the device_mutex.
	struct os_hid_device *hid_control;
	struct os_mutex device_mutex;

	//! Used to read sensor packets.
	struct os_thread_helper oth;

	//! Only touched from the sensor thread.
	timepoint_ns last_sensor_time;

	struct xreal_air_camera *camera;

	struct xreal_air_parsed_sensor last;

	struct
	{
		uint8_t brightness;
		uint8_t display_mode;
	} wants;

	struct
	{
		uint8_t brightness;
		uint8_t display_mode;
	} state;

	struct
	{
		float temperature;

		struct xrt_vec3 gyro;
		struct xrt_vec3 accel;
		struct xrt_vec3 mag;
	} read;

	uint32_t static_id;
	bool display_on;
	uint8_t blend_state;
	uint8_t control_mode;
	uint8_t imu_stream_state;

	enum u_logging_level log_level;
	uint16_t max_sensor_buffer_size;

	uint32_t calibration_buffer_len;
	uint32_t calibration_buffer_pos;
	char *calibration_buffer;
	bool calibration_valid;

	struct
	{
		bool last_frame;
		bool calibration;
	} gui;

	struct m_imu_3dof fusion;
	struct m_relation_history *relation_hist;
};

/*
 *
 * Helper functions.
 *
 */

static inline struct xreal_air_hmd *
xreal_air_hmd(struct xrt_device *dev)
{
	return (struct xreal_air_hmd *)dev;
}

const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832,
    0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A,
    0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3,
    0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
    0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
    0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074,
    0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525,
    0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615,
    0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76,
    0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6,
    0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
    0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7,
    0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
    0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330,
    0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

static uint32_t
crc32_checksum(const uint8_t *buf, uint32_t len)
{
	uint32_t CRC32_data = 0xFFFFFFFF;
	for (uint32_t i = 0; i < len; i++) {
		const uint8_t t = (CRC32_data ^ buf[i]) & 0xFFu;
		CRC32_data = ((CRC32_data >> 8u) & 0xFFFFFFu) ^ crc32_table[t];
	}

	return ~CRC32_data;
}

static uint8_t
scale_brightness(uint8_t brightness)
{
	float relative =
	    ((float)brightness - XREAL_AIR_BRIGHTNESS_MIN) / (XREAL_AIR_BRIGHTNESS_MAX - XREAL_AIR_BRIGHTNESS_MIN);
	relative = CLAMP(relative, 0.0f, 1.0f);
	return (uint8_t)(relative * 100.0f);
}

static uint8_t
unscale_brightness(uint8_t scaled_brightness)
{
	float relative = ((float)scaled_brightness) / 100.0f;
	relative = CLAMP(relative, 0.0f, 1.0f);
	return (uint8_t)(relative * (XREAL_AIR_BRIGHTNESS_MAX - XREAL_AIR_BRIGHTNESS_MIN)) + XREAL_AIR_BRIGHTNESS_MIN;
}

/*
 *
 * Sensor functions.
 *
 */

static bool
send_payload_to_sensor(struct xreal_air_hmd *hmd, uint8_t msgid, const uint8_t *data, uint8_t len)
{
	uint8_t payload[SENSOR_BUFFER_SIZE];

	const uint16_t packet_len = 3 + len;
	const uint16_t payload_len = 5 + packet_len;

	payload[0] = 0xAA;
	payload[5] = (packet_len & 0xFFu);
	payload[6] = ((packet_len >> 8u) & 0xFFu);
	payload[7] = msgid;

	memcpy(payload + 8, data, len);

	const uint32_t checksum = crc32_checksum(payload + 5, packet_len);

	payload[1] = (checksum & 0xFFu);
	payload[2] = ((checksum >> 8u) & 0xFFu);
	payload[3] = ((checksum >> 16u) & 0xFFu);
	payload[4] = ((checksum >> 24u) & 0xFFu);

	return os_hid_write(hmd->hid_sensor, payload, payload_len);
}

static void
pre_biased_coordinate_system(struct xrt_vec3 *out)
{
	const float y = out->y;
	out->x = -out->x;
	out->y = -out->z;
	out->z = -y;
}

static void
post_biased_coordinate_system(const struct xrt_vec3 *in, struct xrt_vec3 *out)
{
	out->x = in->x;
	out->y = -in->y;
	out->z = -in->z;
}

static void
read_sample_and_apply_calibration(struct xreal_air_hmd *hmd,
                                  struct xreal_air_parsed_sample *sample,
                                  struct xrt_vec3 *out_accel,
                                  struct xrt_vec3 *out_gyro,
                                  struct xrt_vec3 *out_mag)
{
	const float accel_factor = (float)sample->accel_multiplier / (float)sample->accel_divisor;
	const float gyro_factor = (float)sample->gyro_multiplier / (float)sample->gyro_divisor;
	const float mag_factor = (float)sample->mag_multiplier / (float)sample->mag_divisor;

	// Convert from raw values to real ones.

	struct xrt_vec3 accel;
	accel.x = (float)sample->accel.x;
	accel.y = (float)sample->accel.y;
	accel.z = (float)sample->accel.z;

	struct xrt_vec3 gyro;
	gyro.x = (float)sample->gyro.x;
	gyro.y = (float)sample->gyro.y;
	gyro.z = (float)sample->gyro.z;

	struct xrt_vec3 mag;
	mag.x = (float)sample->mag.x;
	mag.y = (float)sample->mag.y;
	mag.z = (float)sample->mag.z;

	math_vec3_scalar_mul(accel_factor, &accel);
	math_vec3_scalar_mul(gyro_factor, &gyro);
	math_vec3_scalar_mul(mag_factor, &mag);

	// Apply misalignment via quaternions.

	struct xrt_quat accel_q_mag;
	math_quat_rotate(&hmd->sys->calibration.accel_q_gyro, &hmd->sys->calibration.gyro_q_mag, &accel_q_mag);

	math_quat_rotate_vec3(&hmd->sys->calibration.accel_q_gyro, &gyro, &gyro);
	math_quat_rotate_vec3(&accel_q_mag, &mag, &mag);

	// Go from Gs to m/s2.
	math_vec3_scalar_mul((float)+MATH_GRAVITY_M_S2, &accel);

	// Got from angles to radians.
	math_vec3_scalar_mul((float)+M_PI / 180.0f, &gyro);

	// Apply bias correction and scaling factors.

	pre_biased_coordinate_system(&accel);
	pre_biased_coordinate_system(&gyro);
	pre_biased_coordinate_system(&mag);

	math_vec3_subtract(&hmd->sys->calibration.accel_bias, &accel);
	math_vec3_subtract(&hmd->sys->calibration.gyro_bias, &gyro);
	math_vec3_subtract(&hmd->sys->calibration.mag_bias, &mag);

	accel.x *= hmd->sys->calibration.scale_accel.x;
	accel.y *= hmd->sys->calibration.scale_accel.y;
	accel.z *= hmd->sys->calibration.scale_accel.z;

	gyro.x *= hmd->sys->calibration.scale_gyro.x;
	gyro.y *= hmd->sys->calibration.scale_gyro.y;
	gyro.z *= hmd->sys->calibration.scale_gyro.z;

	mag.x *= hmd->sys->calibration.scale_mag.x;
	mag.y *= hmd->sys->calibration.scale_mag.y;
	mag.z *= hmd->sys->calibration.scale_mag.z;

	post_biased_coordinate_system(&accel, out_accel);
	post_biased_coordinate_system(&gyro, out_gyro);
	post_biased_coordinate_system(&mag, out_mag);
}

static void
update_fusion_locked(struct xreal_air_hmd *hmd, struct xreal_air_parsed_sample *sample, uint64_t timestamp_ns)
{
	read_sample_and_apply_calibration(hmd, sample, &hmd->read.accel, &hmd->read.gyro, &hmd->read.mag);
	m_imu_3dof_update(&hmd->fusion, timestamp_ns, &hmd->read.accel, &hmd->read.gyro);

	/* XXX: With this, do we still need the fusion and relation_hist here? */
	xreal_air_tracker_imu_update(hmd->sys->tracker, hmd->last.timestamp, &hmd->read.accel, &hmd->read.gyro);
}

static void
update_fusion(struct xreal_air_hmd *hmd, struct xreal_air_parsed_sample *sample, uint64_t timestamp_ns)
{
	struct xrt_space_relation rel;
	U_ZERO(&rel); // Clear out the relation.
	rel.relation_flags = (enum xrt_space_relation_flags)(XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |
	                                                     XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);

	os_mutex_lock(&hmd->device_mutex);
	update_fusion_locked(hmd, sample, timestamp_ns);
	rel.pose.orientation = hmd->fusion.rot; // We have no tracking, don't return a position.
	os_mutex_unlock(&hmd->device_mutex);

	m_relation_history_push(hmd->relation_hist, &rel, timestamp_ns);
}

static timepoint_ns
ensure_forward_progress_timestamps(struct xreal_air_hmd *hmd, timepoint_ns timestamp_ns)
{
	timepoint_ns t = timestamp_ns;

	/*
	 * This make sure the timestamp is after the last we sent to the fusion,
	 * but it effectively drops the sample.
	 */
	if (hmd->last_sensor_time > t) {
		t = hmd->last_sensor_time + 1;
	}

	hmd->last_sensor_time = t;
	return t;
}

static void
request_sensor_control_get_cal_data_length(struct xreal_air_hmd *hmd)
{
	// Request calibration data length.

	if (!send_payload_to_sensor(hmd, XREAL_AIR_MSG_GET_CAL_DATA_LENGTH, NULL, 0)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for receiving calibration data length!");
	}
}

static void
request_sensor_control_cal_data_get_next_segment(struct xreal_air_hmd *hmd)
{
	// Request next segment of calibration data.

	if (!send_payload_to_sensor(hmd, XREAL_AIR_MSG_CAL_DATA_GET_NEXT_SEGMENT, NULL, 0)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for receiving next calibration data segment! %d / %d",
		                hmd->calibration_buffer_pos, hmd->calibration_buffer_len);
	}
}

static void
request_sensor_control_get_static_id(struct xreal_air_hmd *hmd)
{
	// Request the static id.

	if (!send_payload_to_sensor(hmd, XREAL_AIR_MSG_GET_STATIC_ID, NULL, 0)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for receiving static id!");
	}
}

static void
request_sensor_control_start_imu_data(struct xreal_air_hmd *hmd, uint8_t imu_stream_state)
{
	// Request to change the imu stream state.

	if (!send_payload_to_sensor(hmd, XREAL_AIR_MSG_START_IMU_DATA, &imu_stream_state, 1)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for changing the imu stream state! %d", imu_stream_state);
	}
}

static void
handle_sensor_control_get_cal_data_length(struct xreal_air_hmd *hmd,
                                          const struct xreal_air_parsed_sensor_control_data *data)
{
	// Read calibration data length.
	const uint32_t calibration_data_length =
	    ((data->data[0] << 0u) | (data->data[1] << 8u) | (data->data[2] << 16u) | (data->data[3] << 24u));

	hmd->calibration_buffer_len = calibration_data_length;
	hmd->calibration_valid = false;

	if (hmd->calibration_buffer_len > 0) {
		if (hmd->calibration_buffer) {
			free(hmd->calibration_buffer);
		}

		// Allocate calibration buffer.
		hmd->calibration_buffer = U_TYPED_ARRAY_CALLOC(char, hmd->calibration_buffer_len);
		hmd->calibration_buffer_pos = 0;
	}

	if (hmd->calibration_buffer) {
		request_sensor_control_cal_data_get_next_segment(hmd);
	}
}

static void
handle_sensor_control_cal_data_get_next_segment(struct xreal_air_hmd *hmd,
                                                const struct xreal_air_parsed_sensor_control_data *data)
{
	if (hmd->calibration_valid) {
		return;
	} else if (hmd->calibration_buffer_len == 0) {
		request_sensor_control_get_cal_data_length(hmd);
		return;
	}

	if (hmd->calibration_buffer_len <= hmd->calibration_buffer_pos) {
		XREAL_AIR_ERROR(hmd, "Failed to receive next calibration data segment! %d / %d",
		                hmd->calibration_buffer_pos, hmd->calibration_buffer_len);
		return;
	}

	uint16_t data_buffer_size = SENSOR_BUFFER_SIZE;

	if (data_buffer_size > hmd->max_sensor_buffer_size) {
		data_buffer_size = hmd->max_sensor_buffer_size;
	}

	if (data_buffer_size <= 8) {
		XREAL_AIR_ERROR(hmd, "Failed to receive next calibration data from buffer!");
		return;
	}

	data_buffer_size -= 8;

	const uint32_t remaining = (hmd->calibration_buffer_len - hmd->calibration_buffer_pos);
	const uint16_t next = (remaining > data_buffer_size ? data_buffer_size : remaining);

	if (hmd->calibration_buffer) {
		memcpy(hmd->calibration_buffer + hmd->calibration_buffer_pos, data->data, next);
	} else {
		request_sensor_control_get_cal_data_length(hmd);
		return;
	}

	hmd->calibration_buffer_pos += next;

	if (hmd->calibration_buffer_pos == hmd->calibration_buffer_len) {
		// Parse calibration data from raw json.
		if (!xreal_air_parse_calibration_buffer(&hmd->sys->calibration, hmd->calibration_buffer,
		                                        hmd->calibration_buffer_len)) {
			hmd->calibration_valid = false;

			XREAL_AIR_ERROR(hmd, "Failed parse calibration data!");
		} else {
			hmd->calibration_valid = true;

			// Switch to imu sensor data stream
			request_sensor_control_start_imu_data(hmd, 0x01);
		}

		// Free calibration buffer.
		free(hmd->calibration_buffer);
		hmd->calibration_buffer = NULL;
		hmd->calibration_buffer_len = 0;
		hmd->calibration_buffer_pos = 0;
	} else {
		request_sensor_control_cal_data_get_next_segment(hmd);
	}
}

static void
handle_sensor_control_start_imu_data(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_sensor_control_data *data)
{
	// Read the imu stream state.
	const uint8_t imu_stream_state = data->data[0];

	hmd->imu_stream_state = imu_stream_state;

	if (hmd->static_id == 0) {
		request_sensor_control_get_static_id(hmd);
	} else if ((!hmd->calibration_valid) && (!hmd->calibration_buffer_len)) {
		request_sensor_control_get_cal_data_length(hmd);
	}
}

static void
handle_sensor_control_get_static_id(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_sensor_control_data *data)
{
	// Read the static id.
	const uint32_t static_id =
	    ((data->data[0] << 0u) | (data->data[1] << 8u) | (data->data[2] << 16u) | (data->data[3] << 24u));

	hmd->static_id = static_id;

	if ((!hmd->calibration_valid) && (!hmd->calibration_buffer_len)) {
		request_sensor_control_get_cal_data_length(hmd);
	}
}

static void
handle_sensor_control_data_msg(struct xreal_air_hmd *hmd, unsigned char *buffer, size_t size)
{
	struct xreal_air_parsed_sensor_control_data data;

	if (hmd->calibration_valid &&
	    !xreal_air_parse_sensor_control_data_packet(&data, buffer, size, hmd->max_sensor_buffer_size)) {
		XREAL_AIR_ERROR(hmd, "Could not decode sensor control data packet");
		return;
	}

	hmd->imu_stream_state = 0xAA;

	switch (data.msgid) {
	case XREAL_AIR_MSG_GET_CAL_DATA_LENGTH: handle_sensor_control_get_cal_data_length(hmd, &data); break;
	case XREAL_AIR_MSG_CAL_DATA_GET_NEXT_SEGMENT:
		handle_sensor_control_cal_data_get_next_segment(hmd, &data);
		break;
	case XREAL_AIR_MSG_ALLOCATE_CAL_DATA_BUFFER: break;
	case XREAL_AIR_MSG_WRITE_CAL_DATA_SEGMENT: break;
	case XREAL_AIR_MSG_FREE_CAL_BUFFER: break;
	case XREAL_AIR_MSG_START_IMU_DATA: handle_sensor_control_start_imu_data(hmd, &data); break;
	case XREAL_AIR_MSG_GET_STATIC_ID: handle_sensor_control_get_static_id(hmd, &data); break;
	default: XREAL_AIR_ERROR(hmd, "Got unknown sensor control data msgid, 0x%02x", data.msgid); break;
	}
}

static void
handle_sensor_msg(struct xreal_air_hmd *hmd, unsigned char *buffer, size_t size)
{
	if (buffer[0] == 0xAA) {
		handle_sensor_control_data_msg(hmd, buffer, size);
		return;
	}

	timepoint_ns now_ns = (timepoint_ns)os_monotonic_get_ns();
	uint64_t last_timestamp = hmd->last.timestamp;

	if (!xreal_air_parse_sensor_packet(&hmd->last, buffer, size, hmd->max_sensor_buffer_size)) {
		XREAL_AIR_ERROR(hmd, "Could not decode sensor packet");
	} else {
		hmd->imu_stream_state = 0x1;
	}

	if (!hmd->calibration_valid) {
		request_sensor_control_start_imu_data(hmd, 0xAA);
		return;
	}

	struct xreal_air_parsed_sensor *s = &hmd->last;

	// According to the ICM-42688-P datasheet: (offset: 25 °C, sensitivity: 132.48 LSB/°C)
	hmd->read.temperature = ((float)s->temperature) / 132.48f + 25.0f;

	time_duration_ns inter_sample_duration_ns = s->timestamp - last_timestamp;

	/* FIXME: Should we really *always* call this? That seems insane. */
	xreal_air_tracker_clock_update(hmd->sys->tracker, last_timestamp, now_ns);

	// If this is larger then one second something bad is going on.
	if (hmd->fusion.state != M_IMU_3DOF_STATE_START &&
	    inter_sample_duration_ns >= (time_duration_ns)U_TIME_1S_IN_NS) {
		XREAL_AIR_ERROR(hmd, "Drop packet (sensor too slow): %" PRId64, inter_sample_duration_ns);
		return;
	}

	// Move it back in time.
	timepoint_ns timestamp_ns = now_ns - inter_sample_duration_ns;

	// Make sure timestamps are always after a previous timestamp.
	timestamp_ns = ensure_forward_progress_timestamps(hmd, timestamp_ns);

	// Update the fusion with the sample.
	update_fusion(hmd, &s->sample, timestamp_ns);
}

static void
sensor_clear_queue(struct xreal_air_hmd *hmd)
{
	uint8_t buffer[SENSOR_BUFFER_SIZE];
	size_t buffer_size = SENSOR_BUFFER_SIZE;

	if (buffer_size > hmd->max_sensor_buffer_size) {
		buffer_size = hmd->max_sensor_buffer_size;
	}

	while (os_hid_read(hmd->hid_sensor, buffer, buffer_size, 0) > 0) {
		// Just drop the packets.
	}
}

static int
sensor_read_one_packet(struct xreal_air_hmd *hmd)
{
	uint8_t buffer[SENSOR_BUFFER_SIZE];
	size_t buffer_size = SENSOR_BUFFER_SIZE;

	if (buffer_size > hmd->max_sensor_buffer_size) {
		buffer_size = hmd->max_sensor_buffer_size;
	}

	int size = os_hid_read(hmd->hid_sensor, buffer, buffer_size, 0);
	if (size <= 0) {
		return size;
	}

	handle_sensor_msg(hmd, buffer, (size_t)size);
	return 0;
}

static int
read_one_control_packet(struct xreal_air_hmd *hmd);

static void *
read_thread(void *ptr)
{
	U_TRACE_SET_THREAD_NAME("Xreal Air");

	struct xreal_air_hmd *hmd = (struct xreal_air_hmd *)ptr;
	int ret = 0;

	os_thread_helper_lock(&hmd->oth);

	request_sensor_control_start_imu_data(hmd, 0xAA);

	while (os_thread_helper_is_running_locked(&hmd->oth) && ret >= 0) {
		os_thread_helper_unlock(&hmd->oth);

		ret = read_one_control_packet(hmd);

		if (ret >= 0) {
			ret = sensor_read_one_packet(hmd);
		}

		os_thread_helper_lock(&hmd->oth);
	}

	if (hmd->calibration_buffer) {
		// Free calibration buffer
		free(hmd->calibration_buffer);
		hmd->calibration_buffer = NULL;
		hmd->calibration_buffer_len = 0;
		hmd->calibration_buffer_pos = 0;
	}

	os_thread_helper_unlock(&hmd->oth);

	return NULL;
}

/*
 *
 * Control functions.
 *
 */
static bool
send_payload_to_control(struct xreal_air_hmd *hmd, uint16_t msgid, const uint8_t *data, uint8_t len)
{
	uint8_t payload[CONTROL_BUFFER_SIZE];

	const uint16_t packet_len = 17 + len;
	const uint16_t payload_len = 5 + packet_len;

	payload[0] = CONTROL_HEAD;
	payload[5] = (packet_len & 0xFFu);
	payload[6] = ((packet_len >> 8u) & 0xFFu);

	// Timestamp
	memset(payload + 7, 0, 8);

	payload[15] = (msgid & 0xFFu);
	payload[16] = ((msgid >> 8u) & 0xFFu);

	// Reserved
	memset(payload + 17, 0, 5);

	memcpy(payload + 22, data, len);

	const uint32_t checksum = crc32_checksum(payload + 5, packet_len);

	payload[1] = (checksum & 0xFFu);
	payload[2] = ((checksum >> 8u) & 0xFFu);
	payload[3] = ((checksum >> 16u) & 0xFFu);
	payload[4] = ((checksum >> 24u) & 0xFFu);

	return os_hid_write(hmd->hid_control, payload, payload_len);
}

static void
handle_control_brightness(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// Check status
	if (control->data[0] != 0) {
		return;
	}

	// Brightness
	const uint8_t brightness = scale_brightness(control->data[1]);

	hmd->state.brightness = brightness;
	hmd->wants.brightness = brightness;
}

static void
handle_control_display_mode(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// Check status
	if (control->data[0] != 0) {
		return;
	}

	// Display mode
	const uint8_t display_mode = control->data[1];

	hmd->state.display_mode = display_mode;
	hmd->wants.display_mode = display_mode;
}

static void
handle_control_heartbeat_start(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// TODO
}

static void
handle_control_display_toggled(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// State of display
	const uint8_t display_state = control->data[0];

	hmd->display_on = (display_state != 0);
}

static void
handle_control_button(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// Physical button
	const uint8_t phys_button = control->data[0];

	// Virtual button
	const uint8_t virt_button = control->data[4];

	// Brightness if the button changes it (or display state)
	const uint8_t value = control->data[8];

	switch (virt_button) {
	case XREAL_AIR_BUTTON_VIRT_DISPLAY_TOGGLE: {
		hmd->display_on = value;
		break;
	}
	case XREAL_AIR_BUTTON_VIRT_MENU_TOGGLE: break;
	case XREAL_AIR_BUTTON_VIRT_BRIGHTNESS_UP:
	case XREAL_AIR_BUTTON_VIRT_BRIGHTNESS_DOWN: {
		const uint8_t brightness = scale_brightness(value);

		hmd->state.brightness = brightness;
		hmd->wants.brightness = brightness;
		break;
	}
	case XREAL_AIR_BUTTON_VIRT_UP: {
		switch (hmd->control_mode) {
		case XREAL_AIR_CONTROL_MODE_BRIGHTNESS: break;
		case XREAL_AIR_CONTROL_MODE_VOLUME: break;
		default: {
			XREAL_AIR_ERROR(hmd, "Got unknown mode increase, 0x%02x (0x%02x)", hmd->control_mode, value);
			break;
		}
		}

		break;
	}
	case XREAL_AIR_BUTTON_VIRT_DOWN: {
		switch (hmd->control_mode) {
		case XREAL_AIR_CONTROL_MODE_BRIGHTNESS: break;
		case XREAL_AIR_CONTROL_MODE_VOLUME: break;
		default: {
			XREAL_AIR_ERROR(hmd, "Got unknown mode decrease, 0x%02x (0x%02x)", hmd->control_mode, value);
			break;
		}
		}

		break;
	}
	case XREAL_AIR_BUTTON_VIRT_MODE_2D: {
		const uint8_t display_mode = hmd->state.display_mode;

		if (display_mode != XREAL_AIR_DISPLAY_MODE_2D) {
			hmd->wants.display_mode = XREAL_AIR_DISPLAY_MODE_2D;
		}

		break;
	}
	case XREAL_AIR_BUTTON_VIRT_MODE_3D: {
		const uint8_t display_mode = hmd->state.display_mode;

		if (display_mode != XREAL_AIR_DISPLAY_MODE_3D) {
			hmd->wants.display_mode = XREAL_AIR_DISPLAY_MODE_3D;
		}

		break;
	}
	case XREAL_AIR_BUTTON_VIRT_BLEND_CYCLE: {
		hmd->blend_state = value;
		break;
	}
	case XREAL_AIR_BUTTON_VIRT_CONTROL_TOGGLE: {
		hmd->control_mode = value;
		break;
	}
	default: {
		XREAL_AIR_ERROR(hmd, "Got unknown button pressed, 0x%02x (0x%02x)", virt_button, phys_button);
		break;
	}
	}
}

static void
handle_control_async_text(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// Event only appears if the display is active!
	hmd->display_on = true;

	XREAL_AIR_DEBUG(hmd, "Control message: %s", (const char *)control->data);
}

static void
handle_control_heartbeat_end(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	// TODO
}

static void
handle_control_action_locked(struct xreal_air_hmd *hmd, const struct xreal_air_parsed_control *control)
{
	switch (control->action) {
	case XREAL_AIR_MSG_R_BRIGHTNESS: handle_control_brightness(hmd, control); break;
	case XREAL_AIR_MSG_W_BRIGHTNESS: break;
	case XREAL_AIR_MSG_R_DISP_MODE: handle_control_display_mode(hmd, control); break;
	case XREAL_AIR_MSG_W_DISP_MODE: break;
	case XREAL_AIR_MSG_P_START_HEARTBEAT: handle_control_heartbeat_start(hmd, control); break;
	case XREAL_AIR_MSG_P_DISPLAY_TOGGLED: handle_control_display_toggled(hmd, control); break;
	case XREAL_AIR_MSG_P_BUTTON_PRESSED: handle_control_button(hmd, control); break;
	case XREAL_AIR_MSG_P_ASYNC_TEXT_LOG: handle_control_async_text(hmd, control); break;
	case XREAL_AIR_MSG_P_END_HEARTBEAT: handle_control_heartbeat_end(hmd, control); break;
	default: XREAL_AIR_ERROR(hmd, "Got unknown control action, 0x%02x", control->action); break;
	}
}

static void
handle_control_msg(struct xreal_air_hmd *hmd, unsigned char *buffer, int size)
{
	struct xreal_air_parsed_control control;

	if (!xreal_air_parse_control_packet(&control, buffer, size)) {
		XREAL_AIR_ERROR(hmd, "Could not decode control packet");
	}

	os_mutex_lock(&hmd->device_mutex);
	handle_control_action_locked(hmd, &control);
	os_mutex_unlock(&hmd->device_mutex);
}

static void
control_clear_queue(struct xreal_air_hmd *hmd)
{
	uint8_t buffer[CONTROL_BUFFER_SIZE];

	while (os_hid_read(hmd->hid_control, buffer, CONTROL_BUFFER_SIZE, 0) > 0) {
		// Just drop the packets.
	}
}

static int
read_one_control_packet(struct xreal_air_hmd *hmd)
{
	static uint8_t buffer[CONTROL_BUFFER_SIZE];
	int size = 0;

	size = os_hid_read(hmd->hid_control, buffer, CONTROL_BUFFER_SIZE, 0);
	if (size == 0) {
		return 0;
	}
	if (size < 0) {
		return -1;
	}

	handle_control_msg(hmd, buffer, size);
	return size;
}

static bool
wait_for_brightness(struct xreal_air_hmd *hmd)
{
	for (int i = 0; i < 5000; i++) {
		read_one_control_packet(hmd);

		if (hmd->state.brightness <= 100) {
			return true;
		}

		os_nanosleep(U_TIME_1MS_IN_NS);
	}

	return false;
}

static bool
wait_for_display_mode(struct xreal_air_hmd *hmd)
{
	for (int i = 0; i < 5000; i++) {
		read_one_control_packet(hmd);

		if ((hmd->state.display_mode == XREAL_AIR_DISPLAY_MODE_2D) ||
		    (hmd->state.display_mode == XREAL_AIR_DISPLAY_MODE_3D)) {
			return true;
		}

		os_nanosleep(U_TIME_1MS_IN_NS);
	}

	return false;
}

static bool
control_brightness(struct xreal_air_hmd *hmd)
{
	if (!send_payload_to_control(hmd, XREAL_AIR_MSG_R_BRIGHTNESS, NULL, 0)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for initial brightness value!");
		return false;
	}

	if (!wait_for_brightness(hmd)) {
		XREAL_AIR_ERROR(hmd, "Failed to wait for valid brightness value!");
		return false;
	}

	return true;
}

static bool
control_display_mode(struct xreal_air_hmd *hmd)
{
	if (!send_payload_to_control(hmd, XREAL_AIR_MSG_R_DISP_MODE, NULL, 0)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload for initial display mode!");
		return false;
	}

	if (!wait_for_display_mode(hmd)) {
		XREAL_AIR_ERROR(hmd, "Failed to wait for valid display mode!");
		return false;
	}

	return true;
}

static bool
switch_display_mode(struct xreal_air_hmd *hmd, uint8_t display_mode)
{
	if ((display_mode != XREAL_AIR_DISPLAY_MODE_2D) && (display_mode > XREAL_AIR_DISPLAY_MODE_3D)) {
		return false;
	}

	struct u_device_simple_info info;
	info.display.w_pixels = 1920;
	info.display.h_pixels = 1080;
	info.display.w_meters = 0.13f;
	info.display.h_meters = 0.07f;
	info.lens_horizontal_separation_meters = 0.13f / 2.0f;
	info.lens_vertical_position_meters = 0.07f / 2.0f;

	info.display.w_meters *= 2.0f;
	info.lens_horizontal_separation_meters *= 2.0f;

	if (display_mode == XREAL_AIR_DISPLAY_MODE_3D) {
		info.display.w_pixels *= 2;
	}

	info.fov[0] = (float)(46.0 * (M_PI / 180.0));
	info.fov[1] = (float)(46.0 * (M_PI / 180.0));

	if (!u_device_setup_split_side_by_side(&hmd->base, &info)) {
		XREAL_AIR_ERROR(hmd, "Failed to setup basic device info");
		return false;
	}

	if (display_mode == XREAL_AIR_DISPLAY_MODE_2D) {
		hmd->base.hmd->views[0].display.w_pixels = info.display.w_pixels;
		hmd->base.hmd->views[0].viewport.w_pixels = info.display.w_pixels;

		hmd->base.hmd->views[1].display.w_pixels = 1;
		hmd->base.hmd->views[1].display.h_pixels = 1;
		hmd->base.hmd->views[1].viewport.x_pixels = info.display.w_pixels;
		hmd->base.hmd->views[1].viewport.w_pixels = 1;
		hmd->base.hmd->views[1].viewport.h_pixels = 1;
	}

	return true;
}

/*
 *
 * Misc functions.
 *
 */

static void
teardown(struct xreal_air_hmd *hmd)
{
	// Stop the variable tracking.
	u_var_remove_root(hmd);

	// Shutdown the sensor thread early.
	os_thread_helper_stop_and_wait(&hmd->oth);

	if (hmd->hid_control != NULL) {
		os_hid_destroy(hmd->hid_control);
		hmd->hid_control = NULL;
	}

	if (hmd->hid_sensor != NULL) {
		os_hid_destroy(hmd->hid_sensor);
		hmd->hid_sensor = NULL;
	}

	m_relation_history_destroy(&hmd->relation_hist);

	// Destroy the fusion.
	m_imu_3dof_close(&hmd->fusion);

	os_thread_helper_destroy(&hmd->oth);
	os_mutex_destroy(&hmd->device_mutex);
}

static void
adjust_brightness(struct xreal_air_hmd *hmd)
{
	if (hmd->wants.brightness > 100) {
		return;
	}

	if (hmd->wants.brightness == hmd->state.brightness) {
		return;
	}

	const uint8_t raw_brightness = unscale_brightness(hmd->wants.brightness);
	const uint8_t brightness = scale_brightness(raw_brightness);

	if (brightness == hmd->state.brightness) {
		return;
	}

	if (!send_payload_to_control(hmd, XREAL_AIR_MSG_W_BRIGHTNESS, &raw_brightness, 1)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload setting custom brightness value!");
		return;
	}

	hmd->state.brightness = brightness;
}

static void
adjust_display_mode(struct xreal_air_hmd *hmd)
{
	if ((hmd->wants.display_mode != XREAL_AIR_DISPLAY_MODE_2D) &&
	    (hmd->wants.display_mode != XREAL_AIR_DISPLAY_MODE_3D)) {
		return;
	}

	if (hmd->wants.display_mode == hmd->state.display_mode) {
		return;
	}

	const uint8_t display_mode = hmd->wants.display_mode;

	if (!send_payload_to_control(hmd, XREAL_AIR_MSG_W_DISP_MODE, &display_mode, 1)) {
		XREAL_AIR_ERROR(hmd, "Failed to send payload setting custom display mode!");
		return;
	}

	hmd->state.display_mode = display_mode;
}

/*
 *
 * xrt_device functions.
 *
 */

static xrt_result_t
xreal_air_hmd_update_inputs(struct xrt_device *xdev)
{
	struct xreal_air_hmd *hmd = xreal_air_hmd(xdev);

	os_mutex_lock(&hmd->device_mutex);

	// Adjust brightness
	adjust_brightness(hmd);

	// Adjust display mode
	adjust_display_mode(hmd);

	os_mutex_unlock(&hmd->device_mutex);

	return XRT_SUCCESS;
}

static xrt_result_t
xreal_air_hmd_get_tracked_pose(struct xrt_device *xdev,
                               enum xrt_input_name name,
                               int64_t at_timestamp_ns,
                               struct xrt_space_relation *out_relation)
{
	struct xreal_air_hmd *hmd = xreal_air_hmd(xdev);

	/* FIXME: Query the tracker now that we have it? */

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		U_LOG_XDEV_UNSUPPORTED_INPUT(&hmd->base, hmd->log_level, name);
		return XRT_ERROR_INPUT_UNSUPPORTED;
	}

	const enum xrt_space_relation_flags flags = (enum xrt_space_relation_flags)(
	    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);

	struct xrt_space_relation relation = XRT_SPACE_RELATION_ZERO;
	relation.relation_flags = flags;

	m_relation_history_get(hmd->relation_hist, at_timestamp_ns, &relation);
	relation.relation_flags = flags; // Needed after history_get

	*out_relation = relation;
	struct xrt_quat *orientation = &out_relation->pose.orientation;

	// Make sure that the orientation is valid.
	if (math_quat_dot(orientation, orientation) > 0.0f) {
		math_quat_normalize(orientation);
	} else {
		orientation->w = 1.0f;
	}

	return XRT_SUCCESS;
}

static void
xreal_air_hmd_destroy(struct xrt_device *xdev)
{
	struct xreal_air_hmd *hmd = xreal_air_hmd(xdev);
	teardown(hmd);

	u_device_free(&hmd->base);
}

static bool
xreal_air_hmd_compute_distortion(
    struct xrt_device *xdev, uint32_t view, float u, float v, struct xrt_uv_triplet *result)
{
	return u_compute_distortion_none(u, v, result);
}

/*
 *
 * Exported functions.
 *
 */

struct xreal_air_hmd *
xreal_air_hmd_create_device(struct xreal_air_system *sys,
                            struct os_hid_device *sensor_device,
                            struct os_hid_device *control_device,
                            uint16_t max_sensor_buffer_size)
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct xreal_air_hmd *hmd = U_DEVICE_ALLOCATE(struct xreal_air_hmd, flags, 1, 0);
	int count;
	int ret;

	hmd->sys = sys;
	hmd->max_sensor_buffer_size = max_sensor_buffer_size;
	hmd->base.update_inputs = xreal_air_hmd_update_inputs;
	hmd->base.get_tracked_pose = xreal_air_hmd_get_tracked_pose;
	hmd->base.get_view_poses = u_device_get_view_poses;
	hmd->base.compute_distortion = xreal_air_hmd_compute_distortion;
	hmd->base.destroy = xreal_air_hmd_destroy;
	hmd->base.name = XRT_DEVICE_GENERIC_HMD;
	hmd->base.device_type = XRT_DEVICE_TYPE_HMD;
	hmd->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;
	hmd->base.supported.orientation_tracking = true;
	hmd->base.supported.position_tracking = false;

	// Set up display details refresh rate
	hmd->base.hmd->screens[0].nominal_frame_interval_ns = time_s_to_ns(1.0f / 60.0f);

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(&hmd->base);

	m_imu_3dof_init(&hmd->fusion, M_IMU_3DOF_USE_GRAVITY_DUR_20MS);
	m_relation_history_create(&hmd->relation_hist);

	hmd->static_id = 0;
	hmd->display_on = false;
	hmd->blend_state = XREAL_AIR_BLEND_STATE_DEFAULT;
	hmd->control_mode = XREAL_AIR_CONTROL_MODE_BRIGHTNESS;
	hmd->imu_stream_state = 0;

	hmd->calibration_buffer = NULL;
	hmd->calibration_buffer_len = 0;
	hmd->calibration_buffer_pos = 0;
	hmd->calibration_valid = false;

	hmd->state.brightness = 0xFF;
	hmd->state.display_mode = 0x00;

	hmd->wants.brightness = 0xFF;
	hmd->wants.display_mode = 0x00;

	hmd->last.timestamp = 0xFFFFFFFF;

	// Print name
	snprintf(hmd->base.str, XRT_DEVICE_NAME_LEN, "Xreal Air Glasses");
	snprintf(hmd->base.serial, XRT_DEVICE_NAME_LEN, "Xreal Air Glasses");

	// Do mutex and thread init before any call to teardown happens.
	os_mutex_init(&hmd->device_mutex);
	os_thread_helper_init(&hmd->oth);

	if (!sensor_device) {
		goto cleanup;
	}

	hmd->hid_sensor = sensor_device;

	// Empty the queue
	sensor_clear_queue(hmd);

	if (!control_device) {
		goto cleanup;
	}

	hmd->hid_control = control_device;

	// Empty the queue
	control_clear_queue(hmd);

	ret = os_thread_helper_start(&hmd->oth, read_thread, (void *)hmd);
	if (ret < 0) {
		goto cleanup;
	}

	if (!control_brightness(hmd) || !control_display_mode(hmd)) {
		goto cleanup;
	}

	/* Wait for callibration data to be read */
	count = 0;
	while (!hmd->calibration_valid) {
		if (count++ > 1000000)
			goto cleanup;
		os_nanosleep(1000);
	}

	/*
	 * Device setup.
	 */

	if (!switch_display_mode(hmd, hmd->state.display_mode)) {
		goto cleanup;
	}

	/*
	 * Setup variable.
	 */

	u_var_add_root(hmd, "Xreal Air Glasses", true);
	u_var_add_u8(hmd, &hmd->wants.brightness, "Brightness");
	u_var_add_u8(hmd, &hmd->wants.display_mode, "Display mode");
	u_var_add_gui_header(hmd, &hmd->gui.last_frame, "Last data");
	u_var_add_ro_vec3_i32(hmd, &hmd->last.sample.accel, "last.sample.accel");
	u_var_add_ro_vec3_i32(hmd, &hmd->last.sample.gyro, "last.sample.gyro");
	u_var_add_ro_vec3_i32(hmd, &hmd->last.sample.mag, "last.sample.mag");
	u_var_add_ro_f32(hmd, &hmd->read.temperature, "read.temperature");
	u_var_add_ro_vec3_f32(hmd, &hmd->read.accel, "read.accel");
	u_var_add_ro_vec3_f32(hmd, &hmd->read.gyro, "read.gyro");
	u_var_add_ro_vec3_f32(hmd, &hmd->read.mag, "read.mag");
	u_var_add_log_level(hmd, &hmd->log_level, "Log level");
	m_imu_3dof_add_vars(&hmd->fusion, hmd, "Fusion");
	u_var_add_gui_header(hmd, &hmd->gui.calibration, "Calibration");
	u_var_add_ro_u32(hmd, &hmd->calibration_buffer_len, "calibration_buffer_len");
	u_var_add_ro_u32(hmd, &hmd->calibration_buffer_pos, "calibration_buffer_pos");

	/*
	 * Finishing touches.
	 */

	if (hmd->log_level <= U_LOGGING_DEBUG) {
		u_device_dump_config(&hmd->base, __func__, "Xreal Air");
	}

	XREAL_AIR_DEBUG(hmd, "YES!");

	return hmd;

cleanup:
	XREAL_AIR_DEBUG(hmd, "NO! :(");

	if (hmd->calibration_buffer) {
		free(hmd->calibration_buffer);
	}

	teardown(hmd);
	free(hmd);

	return NULL;
}
