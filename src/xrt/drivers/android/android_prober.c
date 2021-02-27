// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to Android sensors prober code.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @ingroup drv_android
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_logging.h"

#include "android_prober.h"
#include "android_sensors.h"



/*
 *
 * Defines & structs.
 *
 */

/*!
 * Android prober struct.
 *
 * @ingroup drv_android
 * @implements xrt_auto_prober
 */
struct android_prober
{
	struct xrt_auto_prober base;
};


/*
 *
 * Static functions.
 *
 */

//! @private @memberof android_prober
static inline struct android_prober *
android_prober(struct xrt_auto_prober *p)
{
	return (struct android_prober *)p;
}

//! @public @memberof android_prober
static void
android_prober_destroy(struct xrt_auto_prober *p)
{
	struct android_prober *pandroid = android_prober(p);
	free(pandroid);
}

//! @public @memberof android_prober
static int
android_prober_autoprobe(struct xrt_auto_prober *xap,
                         cJSON *attached_data,
                         bool no_hmds,
                         struct xrt_prober *xp,
                         struct xrt_device **out_xdevs)
{
	struct android_device *dd = android_device_create();
	out_xdevs[0] = &dd->base;
	return 1;
}


/*
 *
 * Exported functions.
 *
 */

struct xrt_auto_prober *
android_create_auto_prober()
{
	struct android_prober *p = U_TYPED_CALLOC(struct android_prober);
	p->base.name = "Android";
	p->base.destroy = android_prober_destroy;
	p->base.lelo_dallas_autoprobe = android_prober_autoprobe;

	return &p->base;
}
