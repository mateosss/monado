// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  OpenHMD prober code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_ohmd
 */

#include <stdio.h>
#include <stdlib.h>

#include "openhmd.h"
#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "oh_interface.h"
#include "oh_device.h"


DEBUG_GET_ONCE_BOOL_OPTION(ohmd_external, "OHMD_EXTERNAL_DRIVER", false)

/*!
 * @implements xrt_auto_prober
 */
struct oh_prober
{
	struct xrt_auto_prober base;
	ohmd_context *ctx;
};

//! @private @memberof oh_prober
static inline struct oh_prober *
oh_prober(struct xrt_auto_prober *p)
{
	return (struct oh_prober *)p;
}

//! @public @memberof oh_prober
static void
oh_prober_destroy(struct xrt_auto_prober *p)
{
	struct oh_prober *ohp = oh_prober(p);

	if (ohp->ctx != NULL) {
		ohmd_ctx_destroy(ohp->ctx);
		ohp->ctx = NULL;
	}

	free(ohp);
}

//! @public @memberof oh_prober
static int
oh_prober_autoprobe(struct xrt_auto_prober *xap,
                    cJSON *attached_data,
                    bool no_hmds,
                    struct xrt_prober *xp,
                    struct xrt_device **out_xdevs)
{
	struct oh_prober *ohp = oh_prober(xap);

	// Do not use OpenHMD if we are not looking for HMDs.
	if (no_hmds) {
		return 0;
	}

	int device_idx = -1;

	/* Probe for devices */
	int num_devices = ohmd_ctx_probe(ohp->ctx);

	bool orientation_tracking_supported = false;
	bool position_tracking_supported = false;
	/* Then loop */
	for (int i = 0; i < num_devices; i++) {
		int device_class = 0, device_flags = 0;
		const char *prod = NULL;

		ohmd_list_geti(ohp->ctx, i, OHMD_DEVICE_CLASS, &device_class);
		ohmd_list_geti(ohp->ctx, i, OHMD_DEVICE_FLAGS, &device_flags);

		if (device_class != OHMD_DEVICE_CLASS_HMD) {
			U_LOG_D("Rejecting device idx %i, is not a HMD.", i);
			continue;
		}

		if (device_flags & OHMD_DEVICE_FLAGS_NULL_DEVICE) {
			U_LOG_D("Rejecting device idx %i, is a NULL device.", i);
			continue;
		}

		prod = ohmd_list_gets(ohp->ctx, i, OHMD_PRODUCT);
		if (strcmp(prod, "External Device") == 0 && !debug_get_bool_option_ohmd_external()) {
			U_LOG_D("Rejecting device idx %i, is a External device.", i);
			continue;
		}

		U_LOG_D("Selecting device idx %i", i);
		device_idx = i;

		orientation_tracking_supported = (device_flags & OHMD_DEVICE_FLAGS_ROTATIONAL_TRACKING) != 0;
		position_tracking_supported = (device_flags & OHMD_DEVICE_FLAGS_POSITIONAL_TRACKING) != 0;
		break;
	}

	if (device_idx < 0) {
		return 0;
	}

	const char *prod = ohmd_list_gets(ohp->ctx, device_idx, OHMD_PRODUCT);
	ohmd_device *dev = ohmd_list_open_device(ohp->ctx, device_idx);
	if (dev == NULL) {
		return 0;
	}

	struct xrt_device *xdev = oh_device_create(ohp->ctx, dev, prod);

	xdev->orientation_tracking_supported = orientation_tracking_supported;
	xdev->position_tracking_supported = position_tracking_supported;
	xdev->device_type = XRT_DEVICE_TYPE_HMD;

	out_xdevs[0] = xdev;
	return 1;
}

struct xrt_auto_prober *
oh_create_auto_prober()
{
	struct oh_prober *ohp = U_TYPED_CALLOC(struct oh_prober);
	ohp->base.name = "OpenHMD";
	ohp->base.destroy = oh_prober_destroy;
	ohp->base.lelo_dallas_autoprobe = oh_prober_autoprobe;
	ohp->ctx = ohmd_ctx_create();

	return &ohp->base;
}
