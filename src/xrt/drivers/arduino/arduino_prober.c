// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Arduino felxable input device prober code.
 * @author Pete Black <pete.black@collabora.com>
 * @ingroup drv_arduino
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "os/os_ble.h"

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "arduino_interface.h"


/*
 *
 * Defines & structs.
 *
 */

DEBUG_GET_ONCE_BOOL_OPTION(arduino_enable, "ARDUINO_ENABLE", true)

/*!
 * Arduino prober struct.
 *
 * @ingroup drv_arduino
 * @implements xrt_auto_prober
 */
struct arduino_prober
{
	struct xrt_auto_prober base;

	bool enabled;
};


/*
 *
 * Static functions.
 *
 */

//! @private @memberof arduino_prober
static inline struct arduino_prober *
arduino_prober(struct xrt_auto_prober *p)
{
	return (struct arduino_prober *)p;
}

//! @public @memberof arduino_prober
static void
arduino_prober_destroy(struct xrt_auto_prober *p)
{
	struct arduino_prober *ap = arduino_prober(p);

	free(ap);
}

//! @public @memberof arduino_prober
static int
arduino_prober_autoprobe(struct xrt_auto_prober *xap,
                         cJSON *attached_data,
                         bool no_hmds,
                         struct xrt_prober *xp,
                         struct xrt_device **out_xdevs)
{
	struct arduino_prober *ap = arduino_prober(xap);
	if (!ap->enabled) {
		return 0;
	}

	const char *dev_uuid = "00004242-0000-1000-8000-004242424242";
	const char *char_uuid = "00000001-1000-1000-8000-004242424242";

	struct os_ble_device *ble = NULL;
	os_ble_notify_open(dev_uuid, char_uuid, &ble);
	if (ble == NULL) {
		return 0;
	}

	out_xdevs[0] = arduino_device_create(ble);
	return 1;
}


/*
 *
 * Exported functions.
 *
 */

struct xrt_auto_prober *
arduino_create_auto_prober()
{
	struct arduino_prober *ap = U_TYPED_CALLOC(struct arduino_prober);
	ap->base.name = "Arduino";
	ap->base.destroy = arduino_prober_destroy;
	ap->base.lelo_dallas_autoprobe = arduino_prober_autoprobe;
	ap->enabled = debug_get_bool_option_arduino_enable();

	return &ap->base;
}
