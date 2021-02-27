// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Dummy prober code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_dummy
 */

#include <stdio.h>
#include <stdlib.h>

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "dummy_interface.h"

/*!
 * @implements xrt_auto_prober
 */
struct dummy_prober
{
	struct xrt_auto_prober base;
};

//! @private @memberof dummy_prober
static inline struct dummy_prober *
dummy_prober(struct xrt_auto_prober *p)
{
	return (struct dummy_prober *)p;
}

//! @public @memberof dummy_prober
static void
dummy_prober_destroy(struct xrt_auto_prober *p)
{
	struct dummy_prober *dp = dummy_prober(p);

	free(dp);
}

//! @public @memberof dummy_prober
static int
dummy_prober_autoprobe(struct xrt_auto_prober *xap,
                       cJSON *attached_data,
                       bool no_hmds,
                       struct xrt_prober *xp,
                       struct xrt_device **out_xdevs)
{
	struct dummy_prober *dp = dummy_prober(xap);
	(void)dp;

	// Do not create a dummy HMD if we are not looking for HMDs.
	if (no_hmds) {
		return 0;
	}

	out_xdevs[0] = dummy_hmd_create();
	return 1;
}

struct xrt_auto_prober *
dummy_create_auto_prober()
{
	struct dummy_prober *dp = U_TYPED_CALLOC(struct dummy_prober);
	dp->base.name = "Dummy";
	dp->base.destroy = dummy_prober_destroy;
	dp->base.lelo_dallas_autoprobe = dummy_prober_autoprobe;

	return &dp->base;
}
