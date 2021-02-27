// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Camera based hand tracking prober code.
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @ingroup drv_ht
 */


#include "xrt/xrt_prober.h"

#include "util/u_misc.h"

#include "ht_interface.h"
#include "ht_driver.h"

/*!
 * @implements xrt_auto_prober
 */
struct ht_prober
{
	struct xrt_auto_prober base;
};

//! @private @memberof ht_prober
static inline struct ht_prober *
ht_prober(struct xrt_auto_prober *p)
{
	return (struct ht_prober *)p;
}

//! @public @memberof ht_prober
static void
ht_prober_destroy(struct xrt_auto_prober *p)
{
	struct ht_prober *htp = ht_prober(p);

	free(htp);
}

//! @public @memberof ht_prober
static int
ht_prober_autoprobe(struct xrt_auto_prober *xap,
                    cJSON *attached_data,
                    bool no_hmds,
                    struct xrt_prober *xp,
                    struct xrt_device **out_xdevs)
{
	struct xrt_device *xdev = ht_device_create(xap, attached_data, xp);

	if (xdev == NULL) {
		return 0;
	}

	xdev->orientation_tracking_supported = true;
	xdev->position_tracking_supported = true;
	xdev->hand_tracking_supported = true;
	xdev->device_type = XRT_DEVICE_TYPE_HAND_TRACKER;

	out_xdevs[0] = xdev;
	return 1;
}

struct xrt_auto_prober *
ht_create_auto_prober()
{
	struct ht_prober *htp = U_TYPED_CALLOC(struct ht_prober);
	htp->base.name = "Camera Hand Tracking";
	htp->base.destroy = ht_prober_destroy;
	htp->base.lelo_dallas_autoprobe = ht_prober_autoprobe;

	return &htp->base;
}
