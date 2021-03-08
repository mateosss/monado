// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header defining the tracking system integration in Monado.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#define XRT_TRACKING_NAME_LEN 256

#include "xrt/xrt_defines.h"

#include "util/u_hand_tracking.h"

#ifdef __cplusplus
extern "C" {
#endif


struct time_state;
struct xrt_device;
struct xrt_tracking;
struct xrt_tracking_factory;
struct xrt_tracked_psmv;
struct xrt_tracked_psvr;
struct xrt_tracked_hand;

//! @todo This is from u_time, duplicated to avoid layer violation.
typedef int64_t timepoint_ns;

/*!
 * @addtogroup xrt_iface
 * @{
 */

/*!
 * What kind of tracking system is this.
 *
 * @todo Is none, Colour, IR, Magnetic the kind of type we need to know about?
 */
enum xrt_tracking_type
{
	// The device(s) are never tracked.
	XRT_TRACKING_TYPE_NONE,

	// The device(s) are tracked by RGB camera(s).
	XRT_TRACKING_TYPE_RGB,

	// The device(s) are tracked by Ligthhouse
	XRT_TRACKING_TYPE_LIGHTHOUSE,

	// The device(s) are tracked by Hydra
	XRT_TRACKING_TYPE_HYDRA,

	// The device(s) are tracked by external SLAM
	XRT_TRACKING_TYPE_EXTERNAL_SLAM,

	// The device(s) are tracked by other methods.
	XRT_TRACKING_TYPE_OTHER,
};

/*!
 * A tracking system or device origin.
 *
 * Tracking systems will typically extend this structure.
 */
struct xrt_tracking_origin
{
	//! For debugging.
	char name[XRT_TRACKING_NAME_LEN];

	//! What can the state tracker expect from this tracking system.
	enum xrt_tracking_type type;

	/*!
	 * Read and written to by the state-tracker using the device(s)
	 * this tracking system is tracking.
	 */
	struct xrt_pose offset;
};

/*!
 * @interface xrt_tracking_factory
 * Tracking factory.
 */
struct xrt_tracking_factory
{
	//! Internal frame context, exposed for debugging purposes.
	struct xrt_frame_context *xfctx;

	/*!
	 * Create a tracked PSMV ball.
	 */
	int (*create_tracked_psmv)(struct xrt_tracking_factory *,
	                           struct xrt_device *xdev,
	                           struct xrt_tracked_psmv **out_psmv);

	/*!
	 * Create a tracked PSVR HMD.
	 */
	int (*create_tracked_psvr)(struct xrt_tracking_factory *,
	                           struct xrt_device *xdev,
	                           struct xrt_tracked_psvr **out_psvr);

	/*!
	 * Create a tracked hand.
	 */
	int (*create_tracked_hand)(struct xrt_tracking_factory *,
	                           struct xrt_device *xdev,
	                           struct xrt_tracked_hand **out_hand);
};

/*!
 * IMU Sample.
 */
struct xrt_tracking_sample
{
	struct xrt_vec3 accel_m_s2;
	struct xrt_vec3 gyro_rad_secs;
};

/*!
 * @interface xrt_tracked_psmv
 *
 * A single tracked PS Move controller, camera and ball are not synced.
 *
 * @todo How do we communicate ball colour change?
 */
struct xrt_tracked_psmv
{
	//! The tracking system origin for this ball.
	struct xrt_tracking_origin *origin;

	//! Device owning this ball.
	struct xrt_device *xdev;

	//! Colour of the ball.
	struct xrt_colour_rgb_f32 colour;

	/*!
	 * Push a IMU sample into the tracking system.
	 */
	void (*push_imu)(struct xrt_tracked_psmv *, timepoint_ns timestamp_ns, struct xrt_tracking_sample *sample);

	/*!
	 * Called by the owning @ref xrt_device @ref xdev to get the pose of
	 * the ball in the tracking space at the given time.
	 *
	 * @todo Should we add a out_time argument as a way to signal min and
	 * maximum, and as such only do interpelation between different captured
	 * frames.
	 */
	void (*get_tracked_pose)(struct xrt_tracked_psmv *,
	                         enum xrt_input_name name,
	                         timepoint_ns when_ns,
	                         struct xrt_space_relation *out_relation);

	/*!
	 * Destroy this tracked ball.
	 */
	void (*destroy)(struct xrt_tracked_psmv *);
};

/*!
 * @interface xrt_tracked_psvr
 *
 * A tracked PSVR headset.
 *
 * @todo How do we communicate led lighting status?
 */
struct xrt_tracked_psvr
{
	//! The tracking system origin for this ball.
	struct xrt_tracking_origin *origin;

	//! Device owning this ball.
	struct xrt_device *xdev;

	/*!
	 * Push a IMU sample into the tracking system.
	 */
	void (*push_imu)(struct xrt_tracked_psvr *, timepoint_ns timestamp_ns, struct xrt_tracking_sample *sample);

	/*!
	 * Called by the owning @ref xrt_device @ref xdev to get the pose of
	 * the psvr in the tracking space at the given time.
	 */
	void (*get_tracked_pose)(struct xrt_tracked_psvr *,
	                         timepoint_ns when_ns,
	                         struct xrt_space_relation *out_relation);

	/*!
	 * Destroy this tracked psvr.
	 */
	void (*destroy)(struct xrt_tracked_psvr *);
};

/*!
 * @interface xrt_tracked_hand
 *
 * A single tracked Hand
 */
struct xrt_tracked_hand
{
	//! The tracking system origin for this hand.
	struct xrt_tracking_origin *origin;

	//! Device owning this hand.
	struct xrt_device *xdev;

	/*!
	 * Called by the owning @ref xrt_device @ref xdev to get the pose of
	 * the hand in the tracking space at the given time.
	 */
	void (*get_tracked_joints)(struct xrt_tracked_hand *,
	                           enum xrt_input_name name,
	                           timepoint_ns when_ns,
	                           struct u_hand_joint_default_set *out_joints,
	                           struct xrt_space_relation *out_relation);

	/*!
	 * Destroy this tracked hand.
	 */
	void (*destroy)(struct xrt_tracked_hand *);
};

/*
 *
 * Helper functions.
 *
 */

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psmv_get_tracked_pose(struct xrt_tracked_psmv *psmv,
                                  enum xrt_input_name name,
                                  timepoint_ns when_ns,
                                  struct xrt_space_relation *out_relation)
{
	psmv->get_tracked_pose(psmv, name, when_ns, out_relation);
}

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psmv_push_imu(struct xrt_tracked_psmv *psmv, timepoint_ns timestamp_ns, struct xrt_tracking_sample *sample)
{
	psmv->push_imu(psmv, timestamp_ns, sample);
}

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psmv_destroy(struct xrt_tracked_psmv **xtmv_ptr)
{
	struct xrt_tracked_psmv *xtmv = *xtmv_ptr;
	if (xtmv == NULL) {
		return;
	}

	xtmv->destroy(xtmv);
	*xtmv_ptr = NULL;
}

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psvr_get_tracked_pose(struct xrt_tracked_psvr *psvr,
                                  timepoint_ns when_ns,
                                  struct xrt_space_relation *out_relation)
{
	psvr->get_tracked_pose(psvr, when_ns, out_relation);
}

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psvr_push_imu(struct xrt_tracked_psvr *psvr, timepoint_ns timestamp_ns, struct xrt_tracking_sample *sample)
{
	psvr->push_imu(psvr, timestamp_ns, sample);
}

//! @public @memberof xrt_tracked_psmv
static inline void
xrt_tracked_psvr_destroy(struct xrt_tracked_psvr **xtvr_ptr)
{
	struct xrt_tracked_psvr *xtvr = *xtvr_ptr;
	if (xtvr == NULL) {
		return;
	}

	xtvr->destroy(xtvr);
	*xtvr_ptr = NULL;
}


//! @public @memberof xrt_tracked_hand
static inline void
xrt_tracked_hand_get_joints(struct xrt_tracked_hand *h,
                            enum xrt_input_name name,
                            timepoint_ns when_ns,
                            struct u_hand_joint_default_set *out_joints,
                            struct xrt_space_relation *out_relation)
{
	h->get_tracked_joints(h, name, when_ns, out_joints, out_relation);
}

/*!
 * @}
 */


#ifdef __cplusplus
}
#endif
