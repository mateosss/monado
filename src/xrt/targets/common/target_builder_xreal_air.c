// Copyright 2023-2025, Tobias Frisch
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Xreal Air prober code.
 * @author Tobias Frisch <jacki@thejackimonster.de>
 * @ingroup drv_xreal_air
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "os/os_hid.h"

#include "xrt/xrt_config_drivers.h"
#include "xrt/xrt_prober.h"

#include "util/u_builders.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_logging.h"
#include "util/u_system_helpers.h"
#include "util/u_trace_marker.h"

#include "xreal_air/xreal_air.h"
#include "xreal_air/xreal_air_interface.h"

enum u_logging_level xreal_air_log_level;

#define XREAL_AIR_WARN(...) U_LOG_IFL_W(xreal_air_log_level, __VA_ARGS__)
#define XREAL_AIR_ERROR(...) U_LOG_IFL_E(xreal_air_log_level, __VA_ARGS__)


/*
 *
 * Misc stuff.
 *
 */

DEBUG_GET_ONCE_LOG_OPTION(xreal_air_log, "XREAL_AIR_LOG", U_LOGGING_WARN)

static const char *driver_list[] = {
    "xreal_air",
};

#define XREAL_AIR_DRIVER_PRODUCT_IDS 4

static const uint16_t driver_product_ids[XREAL_AIR_DRIVER_PRODUCT_IDS] = {
    XREAL_AIR_PID,
    XREAL_AIR_2_PID,
    XREAL_AIR_2_PRO_PID,
    XREAL_AIR_2_ULTRA_PID,
};

static const uint16_t driver_handle_ifaces[XREAL_AIR_DRIVER_PRODUCT_IDS] = {
    3,
    3,
    3,
    2,
};

static const uint16_t driver_control_ifaces[XREAL_AIR_DRIVER_PRODUCT_IDS] = {
    4,
    4,
    4,
    0,
};

static const uint16_t driver_max_sensor_buffer_sizes[XREAL_AIR_DRIVER_PRODUCT_IDS] = {
    64,
    64,
    64,
    512,
};


/*
 *
 * Member functions.
 *
 */

static xrt_result_t
xreal_air_estimate_system(struct xrt_builder *xb,
                          cJSON *config,
                          struct xrt_prober *xp,
                          struct xrt_builder_estimate *estimate)
{
	struct xrt_prober_device **xpdevs = NULL;
	size_t xpdev_count = 0;
	xrt_result_t xret = XRT_SUCCESS;

	U_ZERO(estimate);

	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);

	if (xret != XRT_SUCCESS) {
		return xret;
	}

	struct xrt_prober_device *dev = NULL;
	uint16_t product_index = 0;

	while (product_index < XREAL_AIR_DRIVER_PRODUCT_IDS) {
		dev = u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_AIR_VID,
		                                   driver_product_ids[product_index], XRT_BUS_TYPE_USB);

		if (dev != NULL)
			break;

		product_index++;
	}

	if (dev != NULL) {
		estimate->certain.head = true;
	}

	xret = xrt_prober_unlock_list(xp, &xpdevs);
	assert(xret == XRT_SUCCESS);

	return XRT_SUCCESS;
}

static xrt_result_t
xreal_air_open_system_impl(struct xrt_builder *xb,
                           cJSON *config,
                           struct xrt_prober *xp,
                           struct xrt_tracking_origin *origin,
                           struct xrt_system_devices *xsysd,
                           struct xrt_frame_context *xfctx,
                           struct u_builder_roles_helper *ubrh)
{
	struct os_hid_device *hid_imu = NULL;
	struct os_hid_device *hid_control = NULL;
	struct xrt_prober_device **xpdevs = NULL;
	xrt_result_t xret = XRT_SUCCESS;
	size_t xpdev_count = 0;

	DRV_TRACE_MARKER();

	xreal_air_log_level = debug_get_log_option_xreal_air_log();

	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);
	if (xret != XRT_SUCCESS) {
		goto unlock_and_fail;
	}

	struct xrt_prober_device *dev_hmd = NULL;
	uint16_t product_index = 0;

	while (product_index < XREAL_AIR_DRIVER_PRODUCT_IDS) {
		dev_hmd = u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_AIR_VID,
		                                       driver_product_ids[product_index], XRT_BUS_TYPE_USB);

		if (dev_hmd != NULL)
			break;

		product_index++;
	}

	if (dev_hmd == NULL) {
		goto unlock_and_fail;
	}

	int result = xrt_prober_open_hid_interface(xp, dev_hmd, driver_handle_ifaces[product_index], &hid_imu);

	if (result != 0) {
		XREAL_AIR_ERROR("Failed to open Xreal Air imu interface");
		goto unlock_and_fail;
	}

	result = xrt_prober_open_hid_interface(xp, dev_hmd, driver_control_ifaces[product_index], &hid_control);

	if (result != 0) {
		XREAL_AIR_ERROR("Failed to open Xreal Air control interface");
		goto unlock_and_fail;
	}

	unsigned char hmd_serial_no[XRT_DEVICE_NAME_LEN];
	result = xrt_prober_get_string_descriptor(xp, dev_hmd, XRT_PROBER_STRING_SERIAL_NUMBER, hmd_serial_no,
	                                          XRT_DEVICE_NAME_LEN);
	if (result < 0) {
		XREAL_AIR_WARN("Could not read Xreal Air serial number from USB");
		snprintf((char *)hmd_serial_no, XRT_DEVICE_NAME_LEN, "Unknown");
	}

	xret = xrt_prober_unlock_list(xp, &xpdevs);
	if (xret != XRT_SUCCESS) {
		goto fail;
	}

	struct xreal_air_system *system =
		xreal_air_system_create(xp, xfctx, hmd_serial_no, hid_imu, hid_control,
					driver_max_sensor_buffer_sizes[product_index], xreal_air_log_level);
	if (system == NULL) {
		XREAL_AIR_ERROR("Failed to create Xreal Air system");
		// Do *not* free hid_imu and hid_control.
		return XRT_ERROR_DEVICE_CREATION_FAILED;
	}

	// Add HMD to device list.
	xsysd->xdevs[xsysd->xdev_count++] = (struct xrt_device*)system->hmd;

	// Assign to role(s).
	ubrh->head = (struct xrt_device*)system->hmd;

	return XRT_SUCCESS;


	/*
	 * Error path.
	 */

unlock_and_fail:
	xret = xrt_prober_unlock_list(xp, &xpdevs);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	/* Fallthrough */
fail:
	if (hid_imu)
		os_hid_destroy(hid_imu);
	if (hid_control)
		os_hid_destroy(hid_control);

	return XRT_ERROR_DEVICE_CREATION_FAILED;
}

static void
xreal_air_destroy(struct xrt_builder *xb)
{
	free(xb);
}


/*
 *
 * 'Exported' functions.
 *
 */

struct xrt_builder *
xreal_air_builder_create(void)
{
	struct u_builder *ub = U_TYPED_CALLOC(struct u_builder);

	// xrt_builder fields.
	ub->base.estimate_system = xreal_air_estimate_system;
	ub->base.open_system = u_builder_open_system_static_roles;
	ub->base.destroy = xreal_air_destroy;
	ub->base.identifier = "xreal_air";
	ub->base.name = "Xreal Air";
	ub->base.driver_identifiers = driver_list;
	ub->base.driver_identifier_count = ARRAY_SIZE(driver_list);

	// u_builder fields.
	ub->open_system_static_roles = xreal_air_open_system_impl;

	return &ub->base;
}
