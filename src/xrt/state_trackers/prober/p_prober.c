// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Main prober code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup st_prober
 */

#include "xrt/xrt_config_drivers.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_config_json.h"
#include "util/u_debug.h"
#include "os/os_hid.h"
#include "p_prober.h"

#ifdef XRT_HAVE_V4L2
#include "v4l2/v4l2_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_VF
#include "vf/vf_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_REMOTE
#include "remote/r_interface.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "multi_wrapper/multi.h"

/*
 *
 * Pre-declare functions.
 *
 */

DEBUG_GET_ONCE_LOG_OPTION(prober_log, "PROBER_LOG", U_LOGGING_WARN)

static void
add_device(struct prober *p, struct prober_device **out_dev);

static int
initialize(struct prober *p, struct xrt_prober_entry_lists *lists);

static void
teardown_devices(struct prober *p);

static void
teardown(struct prober *p);

static int
probe(struct xrt_prober *xp);

static int
dump(struct xrt_prober *xp);

static int
select_device(struct xrt_prober *xp, struct xrt_device **xdevs, size_t num_xdevs);

static int
open_hid_interface(struct xrt_prober *xp,
                   struct xrt_prober_device *xpdev,
                   int interface,
                   struct os_hid_device **out_hid_dev);

static int
open_video_device(struct xrt_prober *xp,
                  struct xrt_prober_device *xpdev,
                  struct xrt_frame_context *xfctx,
                  struct xrt_fs **out_xfs);

static int
list_video_devices(struct xrt_prober *xp, xrt_prober_list_video_cb cb, void *ptr);

static int
get_entries(struct xrt_prober *xp,
            size_t *out_num_entries,
            struct xrt_prober_entry ***out_entries,
            struct xrt_auto_prober ***out_auto_probers);

static int
get_string_descriptor(struct xrt_prober *xp,
                      struct xrt_prober_device *xpdev,
                      enum xrt_prober_string which_string,
                      unsigned char *buffer,
                      size_t length);

static bool
can_open(struct xrt_prober *xp, struct xrt_prober_device *xpdev);

static void
destroy(struct xrt_prober **xp);


/*
 *
 * "Exported" functions.
 *
 */

int
xrt_prober_create_with_lists(struct xrt_prober **out_xp, struct xrt_prober_entry_lists *lists)
{
	struct prober *p = U_TYPED_CALLOC(struct prober);

	int ret = initialize(p, lists);
	if (ret != 0) {
		free(p);
		return ret;
	}

	*out_xp = &p->base;

	return 0;
}

#define ENUM_TO_STR(r)                                                                                                 \
	case r: return #r

const char *
xrt_prober_string_to_string(enum xrt_prober_string t)
{
	switch (t) {
		ENUM_TO_STR(XRT_PROBER_STRING_MANUFACTURER);
		ENUM_TO_STR(XRT_PROBER_STRING_PRODUCT);
		ENUM_TO_STR(XRT_PROBER_STRING_SERIAL_NUMBER);
	}
	return "";
}

const char *
xrt_bus_type_to_string(enum xrt_bus_type t)
{
	switch (t) {
		ENUM_TO_STR(XRT_BUS_TYPE_UNKNOWN);
		ENUM_TO_STR(XRT_BUS_TYPE_USB);
		ENUM_TO_STR(XRT_BUS_TYPE_BLUETOOTH);
	}
	return "";
}

bool
xrt_prober_match_string(struct xrt_prober *xp,
                        struct xrt_prober_device *dev,
                        enum xrt_prober_string type,
                        const char *to_match)
{
	unsigned char s[256] = {0};
	int len = xrt_prober_get_string_descriptor(xp, dev, type, s, sizeof(s));
	if (len <= 0) {
		return false;
	}

	return 0 == strncmp(to_match, (const char *)s, sizeof(s));
}


int
p_dev_get_usb_dev(struct prober *p,
                  uint16_t bus,
                  uint16_t addr,
                  uint16_t vendor_id,
                  uint16_t product_id,
                  struct prober_device **out_pdev)
{
	struct prober_device *pdev;

	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];

		if (pdev->base.bus != XRT_BUS_TYPE_USB || pdev->usb.bus != bus || pdev->usb.addr != addr) {
			continue;
		}

		if (pdev->base.vendor_id != vendor_id || pdev->base.product_id != product_id) {
			P_ERROR(p,
			        "USB device with same address but different "
			        "vendor and product found!\n"
			        "\tvendor:  %04x %04x\n"
			        "\tproduct: %04x %04x",
			        pdev->base.vendor_id, vendor_id, pdev->base.product_id, product_id);
			continue;
		}

		*out_pdev = pdev;
		return 0;
	}

	add_device(p, &pdev);
	pdev->base.vendor_id = vendor_id;
	pdev->base.product_id = product_id;
	pdev->base.bus = XRT_BUS_TYPE_USB;
	pdev->usb.bus = bus;
	pdev->usb.addr = addr;
	*out_pdev = pdev;

	return 0;
}

int
p_dev_get_bluetooth_dev(
    struct prober *p, uint64_t id, uint16_t vendor_id, uint16_t product_id, struct prober_device **out_pdev)
{
	struct prober_device *pdev;

	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];

		if (pdev->base.bus != XRT_BUS_TYPE_BLUETOOTH || pdev->bluetooth.id != id) {
			continue;
		}

		if (pdev->base.vendor_id != vendor_id || pdev->base.product_id != product_id) {
			P_ERROR(p,
			        "Bluetooth device with same address but "
			        "different vendor and product found!\n"
			        "\tvendor:  %04x %04x\n"
			        "\tproduct: %04x %04x",
			        pdev->base.vendor_id, vendor_id, pdev->base.product_id, product_id);
			continue;
		}

		*out_pdev = pdev;
		return 0;
	}

	add_device(p, &pdev);
	pdev->base.vendor_id = vendor_id;
	pdev->base.product_id = product_id;
	pdev->base.bus = XRT_BUS_TYPE_BLUETOOTH;
	pdev->bluetooth.id = id;

	*out_pdev = pdev;

	return 0;
}


/*
 *
 * Internal functions.
 *
 */

static void
fill_out_product(struct prober *p, struct prober_device *pdev)
{
	const char *bus = pdev->base.bus == XRT_BUS_TYPE_BLUETOOTH ? "bluetooth" : "usb";

	char *str = NULL;
	int ret = 0;
	do {
		ret = snprintf(str, ret, "Unknown %s device: %04x:%04x", bus, pdev->base.vendor_id,
		               pdev->base.product_id);
		if (ret <= 0) {
			return;
		}

		if (str == NULL) {
			str = U_CALLOC_WITH_CAST(char, ret + 1);
		} else {
			pdev->usb.product = str;
			return;
		}
	} while (true);
}

static void
add_device(struct prober *p, struct prober_device **out_dev)
{
	U_ARRAY_REALLOC_OR_FREE(p->devices, struct prober_device, (p->num_devices + 1));

	struct prober_device *dev = &p->devices[p->num_devices++];
	U_ZERO(dev);

	*out_dev = dev;
}

static void
add_usb_entry(struct prober *p, struct xrt_prober_entry *entry)
{
	U_ARRAY_REALLOC_OR_FREE(p->entries, struct xrt_prober_entry *, (p->num_entries + 1));
	p->entries[p->num_entries++] = entry;
}

static int
collect_entries(struct prober *p)
{
	struct xrt_prober_entry_lists *lists = p->lists;
	while (lists) {
		for (size_t j = 0; lists->entries != NULL && lists->entries[j]; j++) {
			struct xrt_prober_entry *entry = lists->entries[j];
			for (size_t k = 0; entry[k].found != NULL; k++) {
				add_usb_entry(p, &entry[k]);
			}
		}

		lists = lists->next;
	}

	return 0;
}


#define num_driver_conflicts 1
char *driver_conflicts[num_driver_conflicts][2] = {{"survive", "vive"}};

static void
disable_drivers_from_conflicts(struct prober *p)
{
	for (size_t i = 0; i < num_driver_conflicts; i++) {
		bool have_first = false;
		bool have_second = false;

		char *first = driver_conflicts[i][0];
		char *second = driver_conflicts[i][1];

		// disable second driver if we have first driver
		for (size_t entry = 0; entry < p->num_entries; entry++) {
			if (strcmp(p->entries[entry]->driver_name, first) == 0) {
				have_first = true;
			}
			if (strcmp(p->entries[entry]->driver_name, second) == 0) {
				have_second = true;
			}
		}

		for (size_t ap = 0; ap < MAX_AUTO_PROBERS; ap++) {
			if (p->auto_probers[ap] == NULL) {
				continue;
			}
			if (strcmp(p->auto_probers[ap]->name, first) == 0) {
				have_first = true;
			}
			if (strcmp(p->auto_probers[ap]->name, second) == 0) {
				have_second = true;
			}
		}

		if (have_first && have_second) {

			// except don't disable second driver, if first driver is already disabled'
			bool first_already_disabled = false;
			;
			for (size_t disabled = 0; disabled < p->num_disabled_drivers; disabled++) {
				if (strcmp(p->disabled_drivers[disabled], first) == 0) {
					first_already_disabled = true;
					break;
				}
			}
			if (first_already_disabled) {
				P_INFO(p, "Not disabling %s because %s is disabled", second, first);
				continue;
			}

			P_INFO(p, "Disabling %s because we have %s", second, first);
			size_t index = p->num_disabled_drivers++;
			U_ARRAY_REALLOC_OR_FREE(p->disabled_drivers, char *, p->num_disabled_drivers);
			p->disabled_drivers[index] = second;
		}
	}
}

static void
parse_disabled_drivers(struct prober *p)
{
	cJSON *disabled_drivers = cJSON_GetObjectItemCaseSensitive(p->json.root, "disabled");
	if (!disabled_drivers) {
		return;
	}

	cJSON *disabled_driver = NULL;
	cJSON_ArrayForEach(disabled_driver, disabled_drivers)
	{
		if (!cJSON_IsString(disabled_driver)) {
			continue;
		}

		size_t index = p->num_disabled_drivers++;
		U_ARRAY_REALLOC_OR_FREE(p->disabled_drivers, char *, p->num_disabled_drivers);
		p->disabled_drivers[index] = disabled_driver->valuestring;
	}
}

static int
initialize(struct prober *p, struct xrt_prober_entry_lists *lists)
{
	p->base.probe = probe;
	p->base.dump = dump;
	p->base.select = select_device;
	p->base.open_hid_interface = open_hid_interface;
	p->base.open_video_device = open_video_device;
	p->base.list_video_devices = list_video_devices;
	p->base.get_entries = get_entries;
	p->base.get_string_descriptor = get_string_descriptor;
	p->base.can_open = can_open;
	p->base.destroy = destroy;
	p->lists = lists;
	p->ll = debug_get_log_option_prober_log();

	p->json.file_loaded = false;
	p->json.root = NULL;

	u_var_add_root((void *)p, "Prober", true);
	u_var_add_ro_u32(p, (uint32_t *)&p->ll, "Log Level");

	int ret;

	u_config_json_open_or_create_main_file(&p->json);

	ret = collect_entries(p);
	if (ret != 0) {
		teardown(p);
		return -1;
	}

#ifdef XRT_HAVE_LIBUSB
	ret = p_libusb_init(p);
	if (ret != 0) {
		teardown(p);
		return -1;
	}
#endif

#ifdef XRT_HAVE_LIBUVC
	ret = p_libuvc_init(p);
	if (ret != 0) {
		teardown(p);
		return -1;
	}
#endif

	ret = p_tracking_init(p);
	if (ret != 0) {
		teardown(p);
		return -1;
	}

	for (int i = 0; i < MAX_AUTO_PROBERS && lists->auto_probers[i]; i++) {
		p->auto_probers[i] = lists->auto_probers[i]();
	}


	p->num_disabled_drivers = 0;
	parse_disabled_drivers(p);
	disable_drivers_from_conflicts(p);

	return 0;
}

static void
teardown_devices(struct prober *p)
{
	// Need to free all devices.
	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];

		if (pdev->usb.product != NULL) {
			free((char *)pdev->usb.product);
			pdev->usb.product = NULL;
		}

		if (pdev->usb.manufacturer != NULL) {
			free((char *)pdev->usb.manufacturer);
			pdev->usb.manufacturer = NULL;
		}

		if (pdev->usb.serial != NULL) {
			free((char *)pdev->usb.serial);
			pdev->usb.serial = NULL;
		}

		if (pdev->usb.path != NULL) {
			free((char *)pdev->usb.path);
			pdev->usb.path = NULL;
		}

#ifdef XRT_HAVE_LIBUSB
		if (pdev->usb.dev != NULL) {
			//! @todo Free somewhere else
		}
#endif

#ifdef XRT_HAVE_LIBUVC
		if (pdev->uvc.dev != NULL) {
			//! @todo Free somewhere else
		}
#endif

#ifdef XRT_HAVE_V4L2
		for (size_t j = 0; j < pdev->num_v4ls; j++) {
			struct prober_v4l *v4l = &pdev->v4ls[j];
			free((char *)v4l->path);
			v4l->path = NULL;
		}

		if (pdev->v4ls != NULL) {
			free(pdev->v4ls);
			pdev->v4ls = NULL;
			pdev->num_v4ls = 0;
		}
#endif

#ifdef XRT_OS_LINUX
		for (size_t j = 0; j < pdev->num_hidraws; j++) {
			struct prober_hidraw *hidraw = &pdev->hidraws[j];
			free((char *)hidraw->path);
			hidraw->path = NULL;
		}

		if (pdev->hidraws != NULL) {
			free(pdev->hidraws);
			pdev->hidraws = NULL;
			pdev->num_hidraws = 0;
		}
#endif
	}

	if (p->devices != NULL) {
		free(p->devices);
		p->devices = NULL;
		p->num_devices = 0;
	}
}

static void
teardown(struct prober *p)
{
	// First remove the variable tracking.
	u_var_remove_root((void *)p);

	// Clean up all auto_probers.
	for (int i = 0; i < MAX_AUTO_PROBERS && p->auto_probers[i]; i++) {
		p->auto_probers[i]->destroy(p->auto_probers[i]);
		p->auto_probers[i] = NULL;
	}

	// Need to turn off tracking early.
	p_tracking_teardown(p);

	// Need to free all entries.
	if (p->entries != NULL) {
		free(p->entries);
		p->entries = NULL;
		p->num_entries = 0;
	}

	teardown_devices(p);

#ifdef XRT_HAVE_LIBUVC
	p_libuvc_teardown(p);
#endif

#ifdef XRT_HAVE_LIBUSB
	p_libusb_teardown(p);
#endif

	u_config_json_close(&p->json);

	free(p->disabled_drivers);
}


/*
 *
 * Member functions.
 *
 */

static int
probe(struct xrt_prober *xp)
{
	struct prober *p = (struct prober *)xp;
	XRT_MAYBE_UNUSED int ret = 0;

	// Free old list first.
	teardown_devices(p);

#ifdef XRT_HAVE_LIBUDEV
	ret = p_udev_probe(p);
	if (ret != 0) {
		P_ERROR(p, "Failed to enumerate udev devices\n");
		return -1;
	}
#endif

#ifdef XRT_HAVE_LIBUSB
	ret = p_libusb_probe(p);
	if (ret != 0) {
		P_ERROR(p, "Failed to enumerate libusb devices\n");
		return -1;
	}
#endif

#ifdef XRT_HAVE_LIBUVC
	ret = p_libuvc_probe(p);
	if (ret != 0) {
		P_ERROR(p, "Failed to enumerate libuvc devices\n");
		return -1;
	}
#endif

	return 0;
}

static int
dump(struct xrt_prober *xp)
{
	struct prober *p = (struct prober *)xp;
	XRT_MAYBE_UNUSED ssize_t k = 0;
	XRT_MAYBE_UNUSED size_t j = 0;

	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];
		p_dump_device(p, pdev, (int)i);
	}

	return 0;
}

static void
handle_found_device(
    struct prober *p, struct xrt_device **xdevs, size_t num_xdevs, bool *have_hmd, struct xrt_device *xdev)
{
	P_DEBUG(p, "Found '%s' %p", xdev->str, (void *)xdev);

	size_t i = 0;
	for (; i < num_xdevs; i++) {
		if (xdevs[i] == NULL) {
			break;
		}
	}

	if (i + 1 > num_xdevs) {
		P_ERROR(p, "Too many devices, closing '%s'", xdev->str);
		xdev->destroy(xdev);
		return;
	}

	// we can have only one HMD
	if (xdev->device_type == XRT_DEVICE_TYPE_HMD) {
		if (*have_hmd) {
			P_ERROR(p, "Too many HMDs, closing '%s'", xdev->str);
			xdev->destroy(xdev);
			return;
		}
		*have_hmd = true;
	}
	xdevs[i] = xdev;
}

static void
add_from_devices(struct prober *p, struct xrt_device **xdevs, size_t num_xdevs, bool *have_hmd)
{
	// Build a list of all current probed devices.
	struct xrt_prober_device **dev_list = U_TYPED_ARRAY_CALLOC(struct xrt_prober_device *, p->num_devices);
	for (size_t i = 0; i < p->num_devices; i++) {
		dev_list[i] = &p->devices[i].base;
	}

	// Loop over all devices and entries that might match them.
	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];

		for (size_t k = 0; k < p->num_entries; k++) {
			struct xrt_prober_entry *entry = p->entries[k];
			if (pdev->base.vendor_id != entry->vendor_id || pdev->base.product_id != entry->product_id) {
				continue;
			}

			bool skip = false;
			for (size_t disabled = 0; disabled < p->num_disabled_drivers; disabled++) {
				if (strcmp(entry->driver_name, p->disabled_drivers[disabled]) == 0) {
					P_INFO(p, "Skipping disabled driver %s", entry->driver_name);
					skip = true;
					break;
					;
				}
			}
			if (skip) {
				continue;
			}

			struct xrt_device *new_xdevs[XRT_MAX_DEVICES_PER_PROBE] = {NULL};
			int num_found = entry->found(&p->base, dev_list, p->num_devices, i, NULL, &(new_xdevs[0]));

			if (num_found <= 0) {
				continue;
			}
			for (int created_idx = 0; created_idx < num_found; ++created_idx) {
				if (new_xdevs[created_idx] == NULL) {
					P_DEBUG(p,
					        "Leaving device creation loop "
					        "early: found function reported %i "
					        "created, but only %i non-null",
					        num_found, created_idx);
					continue;
				}
				handle_found_device(p, xdevs, num_xdevs, have_hmd, new_xdevs[created_idx]);
			}
		}
	}

	// Free the temporary list.
	free(dev_list);
}

static void
add_from_auto_probers(struct prober *p, struct xrt_device **xdevs, size_t num_xdevs, bool *have_hmd)
{
	for (int i = 0; i < MAX_AUTO_PROBERS && p->auto_probers[i]; i++) {

		bool skip = false;
		for (size_t disabled = 0; disabled < p->num_disabled_drivers; disabled++) {
			if (strcmp(p->auto_probers[i]->name, p->disabled_drivers[disabled]) == 0) {
				P_INFO(p, "Skipping disabled driver %s", p->auto_probers[i]->name);
				skip = true;
				break;
			}
		}
		if (skip) {
			continue;
		}

		/*
		 * If we have found a HMD, tell the auto probers not to open
		 * any more HMDs. This is mostly to stop OpenHMD and Monado
		 * fighting over devices.
		 */
		bool no_hmds = *have_hmd;

		struct xrt_device *new_xdevs[XRT_MAX_DEVICES_PER_PROBE] = {NULL};
		int num_found =
		    p->auto_probers[i]->lelo_dallas_autoprobe(p->auto_probers[i], NULL, no_hmds, &p->base, new_xdevs);

		if (num_found <= 0) {
			continue;
		}

		for (int created_idx = 0; created_idx < num_found; ++created_idx) {
			if (new_xdevs[created_idx] == NULL) {
				P_DEBUG(p,
				        "Leaving device creation loop early: %s autoprobe function reported %i "
				        "created, but only %i non-null",
				        p->auto_probers[i]->name, num_found, created_idx);
				continue;
			}
			handle_found_device(p, xdevs, num_xdevs, have_hmd, new_xdevs[created_idx]);
		}
	}
}

static void
add_from_remote(struct prober *p, struct xrt_device **xdevs, size_t num_xdevs, bool *have_hmd)
{
	if (num_xdevs < 3) {
		return;
	}

#ifdef XRT_BUILD_DRIVER_REMOTE
	int port = 4242;
	if (!u_config_json_get_remote_port(&p->json, &port)) {
		port = 4242;
	}

	r_create_devices(port, &xdevs[0], &xdevs[1], &xdevs[2]);
	*have_hmd = xdevs[0] != NULL;
#endif
}

static void
apply_tracking_override(struct prober *p, struct xrt_device **xdevs, size_t num_xdevs, struct xrt_tracking_override *o)
{
	struct xrt_device *target_xdev = NULL;
	size_t target_idx = 0;
	struct xrt_device *tracker_xdev = NULL;

	for (size_t i = 0; i < num_xdevs; i++) {
		struct xrt_device *xdev = xdevs[i];
		if (xdev == NULL) {
			continue;
		}

		if (strncmp(xdev->serial, o->target_device_serial, XRT_DEVICE_NAME_LEN) == 0) {
			target_xdev = xdev;
			target_idx = i;
		}
		if (strncmp(xdev->serial, o->tracker_device_serial, XRT_DEVICE_NAME_LEN) == 0) {
			tracker_xdev = xdev;
		}
	}

	if (target_xdev == NULL) {
		P_WARN(p, "Tracking override target xdev %s not found", o->target_device_serial);
	}

	if (tracker_xdev == NULL) {
		P_WARN(p, "Tracking override tracker xdev %s not found", o->tracker_device_serial);
	}


	if (target_xdev != NULL && tracker_xdev != NULL) {
		struct xrt_device *multi =
		    multi_create_tracking_override(target_xdev, tracker_xdev, o->input_name, &o->offset);

		if (multi) {
			// drops the target device from the list, but keeps the tracker
			// a tracker could be attached to multiple targets with different names
			xdevs[target_idx] = multi;
		} else {
			P_ERROR(p, "Failed to create tracking override multi device");
		}
	}
}

static int
select_device(struct xrt_prober *xp, struct xrt_device **xdevs, size_t num_xdevs)
{
	struct prober *p = (struct prober *)xp;
	enum u_config_json_active_config active;
	bool have_hmd = false;

	u_config_json_get_active(&p->json, &active);

	switch (active) {
	case U_ACTIVE_CONFIG_NONE:
	case U_ACTIVE_CONFIG_TRACKING:
		add_from_devices(p, xdevs, num_xdevs, &have_hmd);
		add_from_auto_probers(p, xdevs, num_xdevs, &have_hmd);
		break;
	case U_ACTIVE_CONFIG_REMOTE: add_from_remote(p, xdevs, num_xdevs, &have_hmd); break;
	default: assert(false);
	}

	// It's easier if we just put the first hmd first,
	// but keep other internal ordering of devices.
	for (size_t i = 1; i < num_xdevs; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}
		if (xdevs[i]->hmd == NULL) {
			continue;
		}

		// This is a HMD, but it's not in the first slot.
		struct xrt_device *hmd = xdevs[i];
		for (size_t k = i; k > 0; k--) {
			xdevs[k] = xdevs[k - 1];
		}
		xdevs[0] = hmd;
		break;
	}

	struct xrt_tracking_override overrides[XRT_MAX_TRACKING_OVERRIDES];
	size_t num_overrides = 0;
	if (u_config_json_get_tracking_overrides(&p->json, overrides, &num_overrides)) {
		for (size_t i = 0; i < num_overrides; i++) {
			struct xrt_tracking_override *o = &overrides[i];
			apply_tracking_override(p, xdevs, num_xdevs, o);
		}
	}

	if (have_hmd) {
		P_DEBUG(p, "Found HMD! '%s'", xdevs[0]->str);
		return 0;
	}

	P_DEBUG(p, "Didn't find any HMD devices");

	// Even if we've found some controllers, we don't use them without an
	// HMD. So, destroy all other found devices.
	for (size_t i = 1; i < num_xdevs; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}

		P_DEBUG(p, "Destroying '%s'", xdevs[i]->str);
		xrt_device_destroy(&xdevs[i]);
	}

	return 0;
}

static int
open_hid_interface(struct xrt_prober *xp,
                   struct xrt_prober_device *xpdev,
                   int interface,
                   struct os_hid_device **out_hid_dev)
{
	struct prober_device *pdev = (struct prober_device *)xpdev;
	int ret;

#ifdef XRT_OS_LINUX
	for (size_t j = 0; j < pdev->num_hidraws; j++) {
		struct prober_hidraw *hidraw = &pdev->hidraws[j];

		if (hidraw->interface != interface) {
			continue;
		}

		ret = os_hid_open_hidraw(hidraw->path, out_hid_dev);
		if (ret != 0) {
			U_LOG_E("Failed to open device '%s' got '%i'", hidraw->path, ret);
			return ret;
		}

		return 0;
	}
#endif // XRT_OS_LINUX

	U_LOG_E(
	    "Could not find the requested "
	    "hid interface (%i) on the device!",
	    interface);
	return -1;
}

DEBUG_GET_ONCE_OPTION(vf_path, "VF_PATH", NULL)

static int
open_video_device(struct xrt_prober *xp,
                  struct xrt_prober_device *xpdev,
                  struct xrt_frame_context *xfctx,
                  struct xrt_fs **out_xfs)
{
	XRT_MAYBE_UNUSED struct prober_device *pdev = (struct prober_device *)xpdev;

#if defined(XRT_BUILD_DRIVER_VF)
	const char *path = debug_get_option_vf_path();
	if (path != NULL) {
		struct xrt_fs *xfs = vf_fs_open_file(xfctx, path);
		if (xfs) {
			*out_xfs = xfs;
			return 0;
		}
	}
#endif

#if defined(XRT_HAVE_V4L2)
	if (pdev->num_v4ls == 0) {
		return -1;
	}

	struct xrt_fs *xfs =
	    v4l2_fs_create(xfctx, pdev->v4ls[0].path, pdev->usb.product, pdev->usb.manufacturer, pdev->usb.serial);
	if (xfs == NULL) {
		return -1;
	}

	*out_xfs = xfs;
	return 0;
#else
	return -1;
#endif
}

static int
list_video_devices(struct xrt_prober *xp, xrt_prober_list_video_cb cb, void *ptr)
{
	struct prober *p = (struct prober *)xp;

	const char *path = debug_get_option_vf_path();
	if (path != NULL) {
		cb(xp, NULL, "Video File", "Collabora", path, ptr);
	}

	// Loop over all devices and find video devices.
	for (size_t i = 0; i < p->num_devices; i++) {
		struct prober_device *pdev = &p->devices[i];

		bool has = false;
#ifdef XRT_HAVE_LIBUVC
		has |= pdev->uvc.dev != NULL;
#endif

#ifdef XRT_HAVE_V4L2
		has |= pdev->num_v4ls > 0;
#endif
		if (!has) {
			continue;
		}

		if (pdev->usb.product == NULL) {
			fill_out_product(p, pdev);
		}

		cb(xp, &pdev->base, pdev->usb.product, pdev->usb.manufacturer, pdev->usb.serial, ptr);
	}

	return 0;
}

static int
get_entries(struct xrt_prober *xp,
            size_t *out_num_entries,
            struct xrt_prober_entry ***out_entries,
            struct xrt_auto_prober ***out_auto_probers)
{
	struct prober *p = (struct prober *)xp;
	*out_num_entries = p->num_entries;
	*out_entries = p->entries;
	*out_auto_probers = p->auto_probers;
	return 0;
}

static int
get_string_descriptor(struct xrt_prober *xp,
                      struct xrt_prober_device *xpdev,
                      enum xrt_prober_string which_string,
                      unsigned char *buffer,
                      size_t max_length)
{
	XRT_MAYBE_UNUSED struct prober *p = (struct prober *)xp;
	XRT_MAYBE_UNUSED struct prober_device *pdev = (struct prober_device *)xpdev;
	XRT_MAYBE_UNUSED int ret;
#ifdef XRT_HAVE_LIBUSB
	if (pdev->base.bus == XRT_BUS_TYPE_USB && pdev->usb.dev != NULL) {
		ret = p_libusb_get_string_descriptor(p, pdev, which_string, buffer, max_length);
		if (ret >= 0) {
			return ret;
		}
	}
#endif
	if (pdev->base.bus == XRT_BUS_TYPE_BLUETOOTH && which_string == XRT_PROBER_STRING_SERIAL_NUMBER) {
		union {
			uint8_t arr[8];
			uint64_t v;
		} u;
		u.v = pdev->bluetooth.id;
		return snprintf((char *)buffer, max_length, "%02X:%02X:%02X:%02X:%02X:%02X", u.arr[5], u.arr[4],
		                u.arr[3], u.arr[2], u.arr[1], u.arr[0]);
	}

	//! @todo add more backends
	//! @todo make this unicode (utf-16)? utf-8 would be better...
	return 0;
}

static bool
can_open(struct xrt_prober *xp, struct xrt_prober_device *xpdev)
{
	XRT_MAYBE_UNUSED struct prober *p = (struct prober *)xp;
	XRT_MAYBE_UNUSED struct prober_device *pdev = (struct prober_device *)xpdev;
#ifdef XRT_HAVE_LIBUSB
	if (pdev->usb.dev != NULL) {
		return p_libusb_can_open(p, pdev);
	}
#endif
	//! @todo add more backends
	return false;
}


static void
destroy(struct xrt_prober **xp)
{
	struct prober *p = (struct prober *)*xp;
	if (p == NULL) {
		return;
	}

	teardown(p);
	free(p);

	*xp = NULL;
}
