// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Just does a probe.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */

#include <string.h>
#include <stdio.h>

#include "xrt/xrt_prober.h"
#include "xrt/xrt_instance.h"
#include "xrt/xrt_device.h"
#include "cli_common.h"

#include "xrt/xrt_config_drivers.h"

static int
do_exit(struct xrt_instance **xi_ptr, int ret)
{
	xrt_instance_destroy(xi_ptr);

	printf(" :: Exiting '%i'\n", ret);

	return ret;
}

#define NUM_XDEVS 32

int
cli_cmd_probe(int argc, const char **argv)
{
	struct xrt_device *xdevs[NUM_XDEVS] = {0};
	struct xrt_instance *xi = NULL;
	int ret = 0;

	printf(" :: Built in drivers:");

#ifdef XRT_BUILD_DRIVER_PSVR
	printf(" PSVR,");
#endif

#ifdef XRT_BUILD_DRIVER_RS
	printf(" RealSense,");
#endif

#ifdef XRT_BUILD_DRIVER_VIVE
	printf(" Vive,");
#endif

#ifdef XRT_BUILD_DRIVER_OHMD
	printf(" OpenHMD,");
#endif

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
	printf(" Hand Tracking,");
#endif

#ifdef XRT_BUILD_DRIVER_DAYDREAM
	printf(" Daydream,");
#endif

#ifdef XRT_BUILD_DRIVER_ARDUINO
	printf(" Arduino,");
#endif

#ifdef XRT_BUILD_DRIVER_DUMMY
	printf(" Dummy,");
#endif

#ifdef XRT_BUILD_DRIVER_REMOTE
	printf(" Remote Debugging,");
#endif

#ifdef XRT_BUILD_DRIVER_HDK
	printf(" OSVR HDK 1.x/2.x,");
#endif

#ifdef XRT_BUILD_DRIVER_PSMV
	printf(" PS Move,");
#endif

#ifdef XRT_BUILD_DRIVER_HYDRA
	printf(" Razer Hydra,");
#endif

#ifdef XRT_BUILD_DRIVER_NS
	printf(" Project Northstar,");
#endif

#ifdef XRT_BUILD_DRIVER_SURVIVE
	printf(" libsurvive,");
#endif

#ifdef XRT_BUILD_DRIVER_ILLIXR
	printf(" ILLIXR,");
#endif

#ifdef XRT_BUILD_DRIVER_QWERTY
	printf(" QWERTY,");
#endif

	printf("\n");

	// Initialize the prober.
	printf(" :: Creating instance!\n");

	ret = xrt_instance_create(NULL, &xi);
	if (ret != 0) {
		return do_exit(&xi, 0);
	}

	// XXX BEGIN
	printf(">>> Begin dump\n");
	struct xrt_prober *xp = NULL;
	xrt_instance_get_prober(xi, &xp);
	// 3: 0x1a2c:0x2c27; usb.product USB Keyboard
	xrt_prober_probe(xp);
	xrt_prober_dump(xp);
	printf(">>> End dump\n");
	// XXX END

	// Need to prime the prober with devices before dumping and listing.
	printf(" :: Probing and selecting!\n");

	ret = xrt_instance_select(xi, xdevs, NUM_XDEVS);
	if (ret != 0) {
		return do_exit(&xi, ret);
	}

	// End of program
	printf(" :: All ok, shutting down.\n");

	for (size_t i = 0; i < NUM_XDEVS; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}

		printf("\tDestroying '%s' [%s]\n", xdevs[i]->str, xdevs[i]->serial);
		xrt_device_destroy(&xdevs[i]);
	}

	// Finally done
	return do_exit(&xi, 0);
}
