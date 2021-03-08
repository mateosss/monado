// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Just does a probe.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */

#include <string.h>
#include <stdio.h>

#include "xrt/xrt_instance.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_prober.h"
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

	// Initialize the prober.
	printf(" :: Creating instance!\n");

	ret = xrt_instance_create(NULL, &xi);
	if (ret != 0) {
		return do_exit(&xi, 0);
	}

	// Need to prime the prober with devices before dumping and listing.
	printf(" :: Probing and selecting!\n");

	ret = xrt_instance_select(xi, xdevs, NUM_XDEVS);
	if (ret != 0) {
		return do_exit(&xi, ret);
	}

	struct xrt_prober *xp = NULL;
	xrt_instance_get_prober(xi, &xp);

	size_t num_entries;
	struct xrt_prober_entry **entries;
	struct xrt_auto_prober **auto_probers;
	ret = xrt_prober_get_entries(xp, &num_entries, &entries, &auto_probers);
	if (ret != 0) {
		do_exit(&xi, ret);
	}

	printf(" :: Regular built in drivers\n");
	for (size_t i = 0; i < num_entries; i++) {
		if (entries[i] == NULL) {
			continue;
		}

		// devices with the same driver name are usually grouped, don't print duplicates
		if (i > 0 && strcmp(entries[i - 1]->driver_name, entries[i]->driver_name) == 0) {
			continue;
		}

		printf("\t%s\n", entries[i]->driver_name);
	}

	for (size_t i = 0; i < MAX_AUTO_PROBERS; i++) {
		if (auto_probers[i] == NULL) {
			continue;
		}

		printf("\t%s\n", auto_probers[i]->name);
	}

	printf(" :: Additional built in drivers\n");
// special cased drivers that are not probed through prober entries/autoprobers
#ifdef XRT_BUILD_DRIVER_REMOTE
	printf("\tRemote Debugging\n");
#endif

#ifdef XRT_BUILD_DRIVER_V4L2
	printf("\tv4l2\n");
#endif

#ifdef XRT_BUILD_DRIVER_VF
	printf("\tvf\n");
#endif

	printf(" :: Destroying probed devices\n");
	for (size_t i = 0; i < NUM_XDEVS; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}

		printf("\tDestroying '%s' [%s]\n", xdevs[i]->str, xdevs[i]->serial);
		xrt_device_destroy(&xdevs[i]);
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
