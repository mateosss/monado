// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Daydream prober code.
 * @author Pete Black <pete.black@collabora.com>
 * @ingroup drv_daydream
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "daydream_interface.h"
#include "daydream_device.h"



/*
 *
 * Defines & structs.
 *
 */

// Should the experimental Daydream driver be enabled.
DEBUG_GET_ONCE_BOOL_OPTION(daydream_enable, "DAYDREAM_ENABLE", true)

/*!
 * Daydream prober struct.
 *
 * @ingroup drv_daydream
 * @implements xrt_auto_prober
 */
struct daydream_prober
{
	struct xrt_auto_prober base;
	bool enabled;
};


/*
 *
 * Static functions.
 *
 */

//! @private @memberof daydream_prober
static inline struct daydream_prober *
daydream_prober(struct xrt_auto_prober *p)
{
	return (struct daydream_prober *)p;
}

//! @public @memberof daydream_prober
static void
daydream_prober_destroy(struct xrt_auto_prober *p)
{
	struct daydream_prober *pdaydream = daydream_prober(p);

	free(pdaydream);
}

//! @public @memberof daydream_prober
static int
daydream_prober_autoprobe(struct xrt_auto_prober *xap,
                          cJSON *attached_data,
                          bool no_hmds,
                          struct xrt_prober *xp,
                          struct xrt_device **out_xdevs)
{
	struct daydream_prober *pdaydream = daydream_prober(xap);
	if (!pdaydream->enabled) {
		return 0;
	}

	const char *dev_uuid = "0000fe55-0000-1000-8000-00805f9b34fb";
	const char *char_uuid = "00000001-1000-1000-8000-00805f9b34fb";

	struct os_ble_device *ble = NULL;
	os_ble_notify_open(dev_uuid, char_uuid, &ble);
	if (ble == NULL) {
		return 0;
	}

	struct daydream_device *dd = daydream_device_create(ble);

	out_xdevs[0] = &dd->base;
	return 1;
}


/*
 *
 * Exported functions.
 *
 */

struct xrt_auto_prober *
daydream_create_auto_prober()
{
	struct daydream_prober *pdaydream = U_TYPED_CALLOC(struct daydream_prober);
	pdaydream->base.name = "DayDream";
	pdaydream->base.destroy = daydream_prober_destroy;
	pdaydream->base.lelo_dallas_autoprobe = daydream_prober_autoprobe;
	pdaydream->enabled = debug_get_bool_option_daydream_enable();

	return &pdaydream->base;
}
