/*
 * Copyright 2013, Fredrik Hultin.
 * Copyright 2013, Jakob Bornecrantz.
 * Copyright 2016 Philipp Zabel
 * Copyright 2019-2022 Jan Schmidt
 * Copyright 2023, Collabora, Ltd.
 * SPDX-License-Identifier: BSL-1.0
 *
 */
/*!
 * @file
 * @brief  Driver code for Oculus Rift S headsets
 *
 * Implementation for the HMD 3dof and 6dof tracking
 *
 * @author Jan Schmidt <jan@centricular.com>
 * @ingroup drv_xreal_air
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "math/m_api.h"
#include "math/m_clock_tracking.h"
#include "math/m_space.h"
#include "math/m_vec3.h"

#include "os/os_time.h"

#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_sink.h"
#include "util/u_trace_marker.h"
#include "util/u_var.h"

#include "xrt/xrt_config_build.h"
#include "xrt/xrt_config_drivers.h"
#include "xrt/xrt_device.h"

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
#include "../drivers/ht/ht_interface.h"
#include "../multi_wrapper/multi.h"
#endif

#include "xreal_air.h"
#include "xreal_air_tracker.h"

#define XREAL_AIR_TRACE(...)	U_LOG_IFL_T(t->sys->log_level, __VA_ARGS__)
#define XREAL_AIR_INFO(...)	U_LOG_IFL_I(t->sys->log_level, __VA_ARGS__)
#define XREAL_AIR_DEBUG(...)	U_LOG_IFL_D(t->sys->log_level, __VA_ARGS__)
#define XREAL_AIR_WARN(...)	U_LOG_IFL_W(t->sys->log_level, __VA_ARGS__)
#define XREAL_AIR_ERROR(...)	U_LOG_IFL_E(t->sys->log_level, __VA_ARGS__)


#ifdef XRT_FEATURE_SLAM
static const bool slam_supported = true;
#else
static const bool slam_supported = false;
#endif

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
static const bool hand_supported = true;
#else
static const bool hand_supported = false;
#endif

DEBUG_GET_ONCE_BOOL_OPTION(xreal_air_slam, "XREAL_AIR_SLAM", true)
DEBUG_GET_ONCE_BOOL_OPTION(xreal_air_handtracking, "XREAL_AIR_HANDTRACKING", true)

static xrt_result_t
xreal_air_tracker_get_tracked_pose_imu(struct xrt_device *xdev,
                                    enum xrt_input_name name,
                                    int64_t at_timestamp_ns,
                                    struct xrt_space_relation *out_relation);

static void
xreal_air_tracker_switch_method_cb(void *t_ptr)
{
	DRV_TRACE_MARKER();

	struct xreal_air_tracker *t = t_ptr;
	t->slam_over_3dof = !t->slam_over_3dof;
	struct u_var_button *btn = &t->gui.switch_tracker_btn;

	if (t->slam_over_3dof) { // Use SLAM
		snprintf(btn->label, sizeof(btn->label), "Switch to 3DoF Tracking");
	} else { // Use 3DoF
		snprintf(btn->label, sizeof(btn->label), "Switch to SLAM Tracking");

		/* XXX: Reset the pose here! */
#if 0
		os_mutex_lock(&t->mutex);
		m_imu_3dof_reset(&t->fusion.i3dof);
		t->fusion.i3dof.rot = t->pose.orientation;
		os_mutex_unlock(&t->mutex);
#endif
	}
}

static struct xrt_slam_sinks *
xreal_air_create_slam_tracker(struct xreal_air_tracker *t, struct xrt_frame_context *xfctx)
{
	DRV_TRACE_MARKER();

	struct xrt_slam_sinks *sinks = NULL;

#ifdef XRT_FEATURE_SLAM
	struct t_slam_tracker_config config = {0};
	t_slam_fill_default_config(&config);

	/* No need to refcount these parameters */
	config.cam_count = 2;
	config.slam_calib = &t->slam_calib;

	int create_status = t_slam_create(xfctx, &config, &t->tracking.slam, &sinks);
	if (create_status != 0) {
		return NULL;
	}

	int start_status = t_slam_start(t->tracking.slam);
	if (start_status != 0) {
		return NULL;
	}

	XREAL_AIR_DEBUG("Rift S SLAM tracker successfully started");
#endif

	return sinks;
}

static int
xreal_air_create_hand_tracker(struct xreal_air_tracker *t,
                              struct xrt_frame_context *xfctx,
                              struct xrt_hand_masks_sink *masks_sink,
                              struct xrt_slam_sinks **out_sinks,
                              struct xrt_device **out_device)
{
	DRV_TRACE_MARKER();

	struct xrt_slam_sinks *sinks = NULL;
	struct xrt_device *device = NULL;

#ifdef XRT_BUILD_DRIVER_HANDTRACKING

	//!@todo What's a sensible boundary for Xreal Air?
	struct t_camera_extra_info extra_camera_info = {
	    0,
	};
	extra_camera_info.views[0].boundary_type = HT_IMAGE_BOUNDARY_NONE;
	extra_camera_info.views[1].boundary_type = HT_IMAGE_BOUNDARY_NONE;

	extra_camera_info.views[0].camera_orientation = CAMERA_ORIENTATION_90;
	extra_camera_info.views[1].camera_orientation = CAMERA_ORIENTATION_90;

	struct t_hand_tracking_create_info create_info = {.cams_info = extra_camera_info, .masks_sink = masks_sink};

	int create_status = ht_device_create(xfctx,
	                                     t->stereo_calib,
	                                     create_info,
	                                     &sinks,
	                                     &device);
	if (create_status != 0) {
		return create_status;
	}

	if (device != NULL) {
		device = multi_create_tracking_override(XRT_TRACKING_OVERRIDE_ATTACHED, device, &t->base,
		                                        XRT_INPUT_GENERIC_TRACKER_POSE, &t->left_cam_from_imu);
	}

	XREAL_AIR_DEBUG("Rift S HMD hand tracker successfully created");
#endif

	*out_sinks = sinks;
	*out_device = device;

	return 0;
}

void
xreal_air_tracker_add_debug_ui(struct xreal_air_tracker *t, void *root)
{
	u_var_add_gui_header(root, NULL, "Tracking");

	if (t->tracking.slam_enabled) {
		t->gui.switch_tracker_btn.cb = xreal_air_tracker_switch_method_cb;
		t->gui.switch_tracker_btn.ptr = t;
		u_var_add_button(root, &t->gui.switch_tracker_btn, "Switch to 3DoF Tracking");
	}

	u_var_add_pose(root, &t->pose, "Tracked Pose");

	u_var_add_gui_header(root, NULL, "3DoF Tracking");
	m_imu_3dof_add_vars(&t->fusion.i3dof, root, "");

	u_var_add_gui_header(root, NULL, "SLAM Tracking");
	u_var_add_ro_text(root, t->gui.slam_status, "Tracker status");

	u_var_add_gui_header(root, NULL, "Hand Tracking");
	u_var_add_ro_text(root, t->gui.hand_status, "Tracker status");
}

/*!
 * Allocate and populate an OpenCV-compatible @ref t_stereo_camera_calibration pointer from
 * the Rift S config.
 *
 * This requires fitting a KB4 fisheye polynomial to the 6 radial + 2 tangential 'Fisheye62'
 * parameters provided by the Rift S.
 *
 */
static void
xreal_air_create_stereo_camera_calib_rotated(struct xreal_air_tracker *t)
{
	t_stereo_camera_calibration_alloc(&t->stereo_calib, T_DISTORTION_FISHEYE_KB4);

#if 0
	calib->view[0].intrinsics[3][3];
	calib->view[0].kb4.k1 = 0;
	calib->view[0].kb4.k2 = 0;
	calib->view[0].kb4.k3 = 0;
	calib->view[0].kb4.k4 = 0;
#endif

	t->stereo_calib->camera_translation[0] =
		t->hmd_calib.slam_camera[1].imu_pose.position.x - t->hmd_calib.slam_camera[0].imu_pose.position.x;
	t->stereo_calib->camera_translation[1] =
		t->hmd_calib.slam_camera[1].imu_pose.position.y - t->hmd_calib.slam_camera[0].imu_pose.position.y;
	t->stereo_calib->camera_translation[2] =
		t->hmd_calib.slam_camera[1].imu_pose.position.z - t->hmd_calib.slam_camera[0].imu_pose.position.z;

	/* XXX: Is this actually correct? */
	struct xrt_quat left_cam_q_imu;
	struct xrt_quat left_q_right;
	struct xrt_matrix_3x3 left_rot_right;

	math_quat_invert(&t->hmd_calib.slam_camera[0].imu_pose.orientation, &left_cam_q_imu);
	math_quat_rotate(&left_cam_q_imu, &t->hmd_calib.slam_camera[1].imu_pose.orientation, &left_q_right);
	math_matrix_3x3_from_quat(&left_cam_q_imu, &left_rot_right);

	t->stereo_calib->camera_rotation[0][0] = left_rot_right.v[0];
	t->stereo_calib->camera_rotation[0][1] = left_rot_right.v[1];
	t->stereo_calib->camera_rotation[0][2] = left_rot_right.v[2];
	t->stereo_calib->camera_rotation[1][0] = left_rot_right.v[3];
	t->stereo_calib->camera_rotation[1][1] = left_rot_right.v[4];
	t->stereo_calib->camera_rotation[1][2] = left_rot_right.v[5];
	t->stereo_calib->camera_rotation[2][0] = left_rot_right.v[6];
	t->stereo_calib->camera_rotation[2][1] = left_rot_right.v[7];
	t->stereo_calib->camera_rotation[2][2] = left_rot_right.v[8];
}

/*!
 * Procedure to setup trackers: 3dof, SLAM and hand tracking.
 *
 * Determines which trackers to initialize
 *
 * @param sys struct xreal_air_system
 *
 * @return initialised tracker on success, NULL if creation fails
 */
struct xreal_air_tracker *
xreal_air_tracker_create(struct xreal_air_system *sys)
{
	struct xreal_air_tracker *t = U_DEVICE_ALLOCATE(struct xreal_air_tracker, U_DEVICE_ALLOC_TRACKING_NONE, 1, 0);
	if (t == NULL) {
		return NULL;
	}

	t->sys = sys;
	t->base.tracking_origin = &sys->base;
	t->base.get_tracked_pose = xreal_air_tracker_get_tracked_pose_imu;

	// Pose / state lock
	int ret = os_mutex_init(&t->mutex);
	if (ret != 0) {
		XREAL_AIR_ERROR("Failed to init mutex!");
		xreal_air_tracker_destroy(t);
		return NULL;
	}

	// XXX: Set the IMU to be our device pose. It is pretty much centered, but that is
	// probably still not actually what we want to do.
	math_pose_identity(&t->device_from_imu);

	// Copy the value for convenience
	t->left_cam_from_imu = t->sys->calibration.slam_camera[0].imu_pose;

	// Decide whether to initialize the SLAM tracker
	bool slam_wanted = debug_get_bool_option_xreal_air_slam();
	bool slam_enabled = slam_supported && slam_wanted;

	// Decide whether to initialize the hand tracker
	bool hand_wanted = debug_get_bool_option_xreal_air_handtracking();
	bool hand_enabled = hand_supported && hand_wanted;

	t->tracking.slam_enabled = slam_enabled;
	t->tracking.hand_enabled = hand_enabled;

	t->slam_over_3dof = slam_enabled; // We prefer SLAM over 3dof tracking if possible

	const char *slam_status = t->tracking.slam_enabled ? "Enabled"
	                          : !slam_wanted           ? "Disabled by the user (envvar set to false)"
	                          : !slam_supported        ? "Unavailable (not built)"
	                                                   : NULL;

	const char *hand_status = t->tracking.hand_enabled ? "Enabled"
	                          : !hand_wanted           ? "Disabled by the user (envvar set to false)"
	                          : !hand_supported        ? "Unavailable (not built)"
	                                                   : NULL;

	assert(slam_status != NULL && hand_status != NULL);

	(void)snprintf(t->gui.slam_status, sizeof(t->gui.slam_status), "%s", slam_status);
	(void)snprintf(t->gui.hand_status, sizeof(t->gui.hand_status), "%s", hand_status);

	// Initialize 3DoF tracker
	m_imu_3dof_init(&t->fusion.i3dof, M_IMU_3DOF_USE_GRAVITY_DUR_20MS);

	t->pose.orientation.w = 1.0f; // All other values set to zero by U_DEVICE_ALLOCATE (which calls U_CALLOC)

	// Construct the stereo camera calibration for the front cameras
	xreal_air_create_stereo_camera_calib_rotated(t);

	// Initialize the input sinks for the camera to send to

	// Initialize SLAM tracker
	struct xrt_slam_sinks *slam_sinks = NULL;
	if (t->tracking.slam_enabled) {
		slam_sinks = xreal_air_create_slam_tracker(t, sys->xfctx);
		if (slam_sinks == NULL) {
			XREAL_AIR_WARN("Unable to setup the SLAM tracker");
			xreal_air_tracker_destroy(t);
			return NULL;
		}
	}

	// Initialize hand tracker
	struct xrt_slam_sinks *hand_sinks = NULL;
	struct xrt_device *hand_device = NULL;
	struct xrt_hand_masks_sink *masks_sink = slam_sinks ? slam_sinks->hand_masks : NULL;
	if (t->tracking.hand_enabled) {
		int hand_status = xreal_air_create_hand_tracker(t, sys->xfctx, masks_sink, &hand_sinks, &hand_device);
		if (hand_status != 0 || hand_sinks == NULL || hand_device == NULL) {
			XREAL_AIR_WARN("Unable to setup the hand tracker");
			xreal_air_tracker_destroy(t);
			return NULL;
		}
	}

	// Setup sinks depending on tracking configuration
	struct xrt_slam_sinks entry_sinks = {0};
	if (slam_enabled && hand_enabled) {
		struct xrt_frame_sink *entry_cam0_sink = NULL;
		struct xrt_frame_sink *entry_cam1_sink = NULL;

		u_sink_split_create(sys->xfctx, slam_sinks->cams[0], hand_sinks->cams[0], &entry_cam0_sink);
		u_sink_split_create(sys->xfctx, slam_sinks->cams[1], hand_sinks->cams[1], &entry_cam1_sink);

		entry_sinks = *slam_sinks;
		entry_sinks.cams[0] = entry_cam0_sink;
		entry_sinks.cams[1] = entry_cam1_sink;
	} else if (slam_enabled) {
		entry_sinks = *slam_sinks;
	} else if (hand_enabled) {
		entry_sinks = *hand_sinks;
	} else {
		entry_sinks = (struct xrt_slam_sinks){0};
	}

	t->slam_sinks = entry_sinks;
	t->handtracker = hand_device;

	return t;
}

void
xreal_air_tracker_destroy(struct xreal_air_tracker *t)
{
	t_stereo_camera_calibration_reference(&t->stereo_calib, NULL);

	m_imu_3dof_close(&t->fusion.i3dof);
	os_mutex_destroy(&t->mutex);
}

struct xrt_device *
xreal_air_tracker_get_hand_tracking_device(struct xreal_air_tracker *t)
{
	return t->handtracker;
}

void
xreal_air_tracker_clock_update(struct xreal_air_tracker *t, uint64_t device_timestamp_ns, timepoint_ns local_timestamp_ns)
{
	os_mutex_lock(&t->mutex);
	time_duration_ns last_hw2mono = t->hw2mono;
	/*
	 * FIXME: Decide on how often to call this, and maybe move into HMD?
	 * Also, is a sample every 1ms correct?
	 */
	const float freq = 1000.0;

	t->seen_clock_observations++;
	if (t->seen_clock_observations < 100)
		goto done;

	m_clock_offset_a2b(freq, device_timestamp_ns, local_timestamp_ns, &t->hw2mono);

	if (!t->have_hw2mono) {
		time_duration_ns change_ns = last_hw2mono - t->hw2mono;
		if (change_ns >= -U_TIME_HALF_MS_IN_NS && change_ns <= U_TIME_HALF_MS_IN_NS) {
			XREAL_AIR_INFO("HMD device to local clock map stabilised");
			t->have_hw2mono = true;
		}
	}
done:
	os_mutex_unlock(&t->mutex);
}

//! Camera specific logic for clock conversion
static void
clock_hw2mono_get(struct xreal_air_tracker *t, uint64_t device_ts, timepoint_ns *out)
{
	*out = t->hw2mono + device_ts;
}

void
xreal_air_tracker_imu_update(struct xreal_air_tracker *t,
                             uint64_t device_timestamp_ns,
                             const struct xrt_vec3 *accel,
                             const struct xrt_vec3 *gyro)
{
	os_mutex_lock(&t->mutex);

	/* Ignore packets before we're ready and clock is stable */
	if (!t->ready_for_data || !t->have_hw2mono) {
		os_mutex_unlock(&t->mutex);
		return;
	}

	/* Get the smoothed monotonic time estimate for this IMU sample */
	timepoint_ns local_timestamp_ns;

	clock_hw2mono_get(t, device_timestamp_ns, &local_timestamp_ns);

	if (t->fusion.last_imu_local_timestamp_ns != 0 && local_timestamp_ns < t->fusion.last_imu_local_timestamp_ns) {
		XREAL_AIR_WARN("IMU time went backward by %" PRId64 " ns",
		            local_timestamp_ns - t->fusion.last_imu_local_timestamp_ns);
	} else {
		m_imu_3dof_update(&t->fusion.i3dof, local_timestamp_ns, accel, gyro);
	}

	XREAL_AIR_TRACE("IMU timestamp %" PRIu64 " (dt %f) hw2mono local ts %" PRIu64 " (dt %f) offset %" PRId64,
	             device_timestamp_ns,
	             (double)(device_timestamp_ns - t->fusion.last_imu_timestamp_ns) / 1000000000.0, local_timestamp_ns,
	             (double)(local_timestamp_ns - t->fusion.last_imu_local_timestamp_ns) / 1000000000.0, t->hw2mono);

	t->fusion.last_angular_velocity = *gyro;
	t->fusion.last_imu_timestamp_ns = device_timestamp_ns;
	t->fusion.last_imu_local_timestamp_ns = local_timestamp_ns;

	t->pose.orientation = t->fusion.i3dof.rot;

	os_mutex_unlock(&t->mutex);

	if (t->slam_sinks.imu) {
		/* Push IMU sample to the SLAM tracker */
		struct xrt_vec3_f64 accel64 = {accel->x, accel->y, accel->z};
		struct xrt_vec3_f64 gyro64 = {gyro->x, gyro->y, gyro->z};
		struct xrt_imu_sample sample = {
		    .timestamp_ns = local_timestamp_ns, .accel_m_s2 = accel64, .gyro_rad_secs = gyro64};

		xrt_sink_push_imu(t->slam_sinks.imu, &sample);
	}
}

#define UPPER_32BITS(x) ((x)&0xffffffff00000000ULL)

void
xreal_air_tracker_push_slam_frames(struct xreal_air_tracker *t,
                                   struct xrt_frame *left,
                                   struct xrt_frame *right)
{
	timepoint_ns frame_time;

	os_mutex_lock(&t->mutex);

	/* Ignore packets before we're ready */
	if (!t->ready_for_data) {
		os_mutex_unlock(&t->mutex);
		return;
	}

	if (!t->have_hw2mono) {
		/* Drop any frames before we have IMU */
		os_mutex_unlock(&t->mutex);
		return;
	}

	/* Generate a capture timestamp using the system monotonic clock */
	clock_hw2mono_get(t, left->source_timestamp, &frame_time);

	if (frame_time < t->last_frame_time) {
		XREAL_AIR_WARN("Camera frame time went backward by %" PRId64 " ns", frame_time - t->last_frame_time);
		os_mutex_unlock(&t->mutex);
		return;
	}

	XREAL_AIR_TRACE("SLAM frame timestamp %" PRIu64 " local %" PRIu64, left->source_timestamp, frame_time);

	t->last_frame_time = frame_time;
	os_mutex_unlock(&t->mutex);

	left->timestamp = frame_time;
	right->timestamp = frame_time;

	if (t->slam_sinks.cams[0])
		xrt_sink_push_frame(t->slam_sinks.cams[0], left);
	if (t->slam_sinks.cams[1])
		xrt_sink_push_frame(t->slam_sinks.cams[1], right);
}

//! Specific pose correction for Basalt to OpenXR coordinates
XRT_MAYBE_UNUSED static inline void
xreal_air_tracker_correct_pose_from_basalt(struct xrt_pose *pose)
{
	struct xrt_quat q = {0.70710678, 0, 0, -0.70710678};
	math_quat_rotate(&q, &pose->orientation, &pose->orientation);
	math_quat_rotate_vec3(&q, &pose->position, &pose->position);
}

static xrt_result_t
xreal_air_tracker_get_tracked_pose_imu(struct xrt_device *xdev,
                                       enum xrt_input_name name,
                                       int64_t at_timestamp_ns,
                                       struct xrt_space_relation *out_relation)
{
	struct xreal_air_tracker *tracker = (struct xreal_air_tracker *)(xdev);
	if (name != XRT_INPUT_GENERIC_TRACKER_POSE) {
		/* FIXME: logging */
		U_LOG_XDEV_UNSUPPORTED_INPUT(&tracker->base, U_LOGGING_ERROR, name);
		return XRT_ERROR_INPUT_UNSUPPORTED;
	}

	xreal_air_tracker_get_tracked_pose(tracker, XREAL_AIR_TRACKER_POSE_IMU, at_timestamp_ns, out_relation);

	return XRT_SUCCESS;
}

void
xreal_air_tracker_get_tracked_pose(struct xreal_air_tracker *t,
                                enum xreal_air_tracker_pose pose,
                                uint64_t at_timestamp_ns,
                                struct xrt_space_relation *out_relation)
{
	struct xrt_relation_chain xrc = {0};

	if (pose == XREAL_AIR_TRACKER_POSE_DEVICE) {
		m_relation_chain_push_pose(&xrc, &t->device_from_imu);
	} else if (pose == XREAL_AIR_TRACKER_POSE_LEFT_CAMERA) {
		m_relation_chain_push_pose(&xrc, &t->left_cam_from_imu);
	}

	if (t->tracking.slam_enabled && t->slam_over_3dof) {
		struct xrt_space_relation imu_relation = XRT_SPACE_RELATION_ZERO;

		// Get the IMU pose from the SLAM tracker
		xrt_tracked_slam_get_tracked_pose(t->tracking.slam, at_timestamp_ns, &imu_relation);
#ifdef XRT_FEATURE_SLAM
		// !todo Correct pose depending on the VIT system in use, this should be done in the system itself.
		// For now, assume that we are using Basalt.
		xreal_air_tracker_correct_pose_from_basalt(&imu_relation.pose);
#endif
		imu_relation.relation_flags = (enum xrt_space_relation_flags)(
		    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
		    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT | XRT_SPACE_RELATION_POSITION_TRACKED_BIT);

		m_relation_chain_push_relation(&xrc, &imu_relation);
	} else {
		struct xrt_space_relation imu_relation = XRT_SPACE_RELATION_ZERO;

		os_mutex_lock(&t->mutex);
		// TODO: Estimate pose at timestamp at_timestamp_ns
		math_quat_normalize(&t->pose.orientation);
		imu_relation.pose = t->pose;
		imu_relation.angular_velocity = t->fusion.last_angular_velocity;
		imu_relation.relation_flags = (enum xrt_space_relation_flags)(
		    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
		    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);

		m_relation_chain_push_relation(&xrc, &imu_relation);

		os_mutex_unlock(&t->mutex);
	}

	m_relation_chain_resolve(&xrc, out_relation);
}

void
xreal_air_tracker_start(struct xreal_air_tracker *t)
{
	os_mutex_lock(&t->mutex);
	t->ready_for_data = true;
	os_mutex_unlock(&t->mutex);
}
