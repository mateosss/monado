// Copyright 2019-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Common interface to probe for devices.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt/xrt_device.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 *
 * Prober and device manager.
 *
 */

struct cJSON;
typedef struct cJSON cJSON;

struct xrt_fs;
struct xrt_frame_context;
struct xrt_prober;
struct xrt_prober_device;
struct xrt_tracking_factory;
struct os_hid_device;

/*!
 * The maximum number of devices that a single
 * @ref xrt_prober_entry::found or
 * @ref xrt_auto_prober::lelo_dallas_autoprobe
 * function called by the prober can create per-call.
 *
 * @ingroup xrt_iface
 */
#define XRT_MAX_DEVICES_PER_PROBE 16

/*!
 * Entry for a single device.
 *
 * @ingroup xrt_iface
 */
struct xrt_prober_entry
{
	uint16_t vendor_id;
	uint16_t product_id;

	int (*found)(struct xrt_prober *xp,
	             struct xrt_prober_device **devices,
	             size_t num_devices,
	             size_t index,
	             cJSON *attached_data,
	             struct xrt_device **out_xdevs);

	const char *name;
};

/*!
 * Function for creating a auto prober.
 *
 * @ingroup xrt_iface
 */
typedef struct xrt_auto_prober *(*xrt_auto_prober_creator)();

/*!
 * Main root of all of the probing device.
 *
 * @ingroup xrt_iface
 */
struct xrt_prober_entry_lists
{
	/*!
	 * A a null terminated list of null terminated lists of
	 * @ref xrt_prober_entry.
	 */
	struct xrt_prober_entry **entries;

	/*!
	 * A null terminated list of @ref xrt_auto_prober creation functions.
	 */
	xrt_auto_prober_creator *auto_probers;

	/*!
	 * Allows you to chain multiple prober entry lists.
	 */
	struct xrt_prober_entry_lists *next;
};

/*!
 * Bus type of a device.
 */
enum xrt_bus_type
{
	XRT_BUS_TYPE_UNKNOWN,
	XRT_BUS_TYPE_USB,
	XRT_BUS_TYPE_BLUETOOTH,
};

/*!
 * String descriptor types
 */
enum xrt_prober_string
{
	XRT_PROBER_STRING_MANUFACTURER,
	XRT_PROBER_STRING_PRODUCT,
	XRT_PROBER_STRING_SERIAL_NUMBER,
};

const char *
xrt_prober_string_to_string(enum xrt_prober_string t);

const char *
xrt_bus_type_to_string(enum xrt_bus_type t);

/*!
 * A probed device, may or may not be opened.
 *
 * @ingroup xrt_iface
 */
struct xrt_prober_device
{
	uint16_t vendor_id;
	uint16_t product_id;

	enum xrt_bus_type bus;

	uint8_t usb_dev_class;
};

/*!
 * Callback for listing video devices.
 *
 * @ingroup xrt_iface
 */
typedef void (*xrt_prober_list_video_cb)(struct xrt_prober *xp,
                                         struct xrt_prober_device *pdev,
                                         const char *product,
                                         const char *manufacturer,
                                         const char *serial,
                                         void *ptr);

/*!
 * The main prober that probes and manages found but not opened HMD devices
 * that are connected to the system.
 *
 * @ingroup xrt_iface
 */
struct xrt_prober
{
	//! Factory for producing tracked objects.
	struct xrt_tracking_factory *tracking;

	/*!
	 * Enumerate all connected devices, whether or not we have an associated
	 * driver.
	 *
	 * @note Code consuming this interface should use
	 * xrt_prober_probe()
	 */
	int (*probe)(struct xrt_prober *xp);

	/*!
	 * Dump a listing of all devices found on the system to platform
	 * dependent output (stdout).
	 *
	 * @note Code consuming this interface should use xrt_prober_dump()
	 */
	int (*dump)(struct xrt_prober *xp);

	/*!
	 * Iterate through drivers (by ID and auto-probers) checking to see if
	 * they can handle any connected devices from the last xrt_prober::probe
	 * call, opening those devices to create instances of xrt_device
	 * implementations.
	 *
	 * If no HMD (not even a dummy HMD) is found, then no devices will be
	 * returned (all xdevs will be NULL). Otherwise, xdevs will be populated
	 * with the HMD in xdevs[0], and any subsequent non-NULL values
	 * referring to additional non-HMD devices.
	 *
	 * @param xp Pointer to self
	 * @param[in,out] xdevs Pointer to xrt_device array. Array elements will
	 * be populated.
	 * @param[in] num_xdevs The capacity of the @p xdevs array.
	 *
	 * @return 0 on success (including "no HMD found"), <0 on error.
	 *
	 * Returned devices have their ownership transferred to the caller: all
	 * should be cleaned up with xrt_device_destroy().
	 *
	 * @note Code consuming this interface should use
	 * xrt_prober_select(). Typically used through an xrt_instance and the
	 * xrt_instance_select() method which usually calls xrt_prober_probe()
	 * and xrt_prober_select().
	 */
	int (*select)(struct xrt_prober *xp, struct xrt_device **xdevs, size_t num_xdevs);

	int (*open_hid_interface)(struct xrt_prober *xp,
	                          struct xrt_prober_device *xpdev,
	                          int interface,
	                          struct os_hid_device **out_hid_dev);

	int (*open_video_device)(struct xrt_prober *xp,
	                         struct xrt_prober_device *xpdev,
	                         struct xrt_frame_context *xfctx,
	                         struct xrt_fs **out_xfs);

	int (*list_video_devices)(struct xrt_prober *xp, xrt_prober_list_video_cb cb, void *ptr);

	/*!
	 * Returns a string property on the device of the given type
	 * @p which_string in @p out_buffer.
	 *
	 * @param[in] xp             Prober.
	 * @param[in] xpdev          Device to get string property from.
	 * @param[in] which_string   Which string property to query.
	 * @param[in,out] out_buffer Target buffer.
	 * @param[in] max_length     Max length of the target buffer.
	 *
	 * @return The length of the string, or negative on error.
	 *
	 */
	int (*get_string_descriptor)(struct xrt_prober *xp,
	                             struct xrt_prober_device *xpdev,
	                             enum xrt_prober_string which_string,
	                             unsigned char *out_buffer,
	                             size_t max_length);

	bool (*can_open)(struct xrt_prober *xp, struct xrt_prober_device *xpdev);

	/*!
	 * Destroy the prober and set the pointer to null.
	 *
	 * Code consuming this interface should use xrt_prober_destroy().
	 *
	 * @param xp_ptr pointer to self-pointer
	 */
	void (*destroy)(struct xrt_prober **xp_ptr);
};

/*!
 * @copydoc xrt_prober::probe
 *
 * Helper function for @ref xrt_prober::probe.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_probe(struct xrt_prober *xp)
{
	return xp->probe(xp);
}

/*!
 * @copydoc xrt_prober::dump
 *
 * Helper function for @ref xrt_prober::dump.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_dump(struct xrt_prober *xp)
{
	return xp->dump(xp);
}

/*!
 * @copydoc xrt_prober::select
 *
 * Helper function for @ref xrt_prober::select.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_select(struct xrt_prober *xp, struct xrt_device **xdevs, size_t num_xdevs)
{
	return xp->select(xp, xdevs, num_xdevs);
}

/*!
 * @copydoc xrt_prober::open_hid_interface
 *
 * Helper function for @ref xrt_prober::open_hid_interface.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_open_hid_interface(struct xrt_prober *xp,
                              struct xrt_prober_device *xpdev,
                              int interface,
                              struct os_hid_device **out_hid_dev)
{
	return xp->open_hid_interface(xp, xpdev, interface, out_hid_dev);
}

/*!
 * @copydoc xrt_prober::get_string_descriptor
 *
 * Helper function for @ref xrt_prober::get_string_descriptor.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_get_string_descriptor(struct xrt_prober *xp,
                                 struct xrt_prober_device *xpdev,
                                 enum xrt_prober_string which_string,
                                 unsigned char *out_buffer,
                                 size_t max_length)
{
	return xp->get_string_descriptor(xp, xpdev, which_string, out_buffer, max_length);
}

/*!
 * @copydoc xrt_prober::can_open
 *
 * Helper function for @ref xrt_prober::can_open.
 *
 * @public @memberof xrt_prober
 */
static inline bool
xrt_prober_can_open(struct xrt_prober *xp, struct xrt_prober_device *xpdev)
{
	return xp->can_open(xp, xpdev);
}


/*!
 * @copydoc xrt_prober::xrt_prober_open_video_device
 *
 * Helper function for @ref xrt_prober::xrt_prober_open_video_device.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_open_video_device(struct xrt_prober *xp,
                             struct xrt_prober_device *xpdev,
                             struct xrt_frame_context *xfctx,
                             struct xrt_fs **out_xfs)
{
	return xp->open_video_device(xp, xpdev, xfctx, out_xfs);
}

/*!
 * @copydoc xrt_prober::list_video_devices
 *
 * Helper function for @ref xrt_prober::list_video_devices.
 *
 * @public @memberof xrt_prober
 */
static inline int
xrt_prober_list_video_devices(struct xrt_prober *xp, xrt_prober_list_video_cb cb, void *ptr)
{
	return xp->list_video_devices(xp, cb, ptr);
}

/*!
 * @copydoc xrt_prober::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xp_ptr to null if freed.
 *
 * @public @memberof xrt_prober
 */
static inline void
xrt_prober_destroy(struct xrt_prober **xp_ptr)
{
	struct xrt_prober *xp = *xp_ptr;
	if (xp == NULL) {
		return;
	}

	xp->destroy(xp_ptr);
	*xp_ptr = NULL;
}

/*!
 * Create a prober with a list of known devices and autoprobers.
 *
 * Typically used by xrt_instance_create implementations to create the prober,
 * often with a shared list called `target_list`.
 *
 * @param[out] out_xp Pointer to xrt_prober pointer, will be populated with
 * created xrt_prober instance.
 * @param[in] list Prober entry list
 *
 * @public @memberof xrt_prober
 */
int
xrt_prober_create_with_lists(struct xrt_prober **out_xp, struct xrt_prober_entry_lists *list);

/*!
 * @public @memberof xrt_prober
 */
bool
xrt_prober_match_string(struct xrt_prober *xp,
                        struct xrt_prober_device *dev,
                        enum xrt_prober_string type,
                        const char *to_match);

/*
 *
 * Auto prober.
 *
 */

/*!
 * @interface xrt_auto_prober
 *
 * An interface to be exposed by a device driver that should probe for the
 * existence of its own device on the system, rather than using shared probers
 * with vendor/product IDs, etc.
 *
 * @ingroup xrt_iface
 */
struct xrt_auto_prober
{
	const char *name;

	/*!
	 * Do the internal probing that the driver needs to do in order to find
	 * devices.
	 *
	 * @param xap Self pointer
	 * @param attached_data JSON "attached data" for this device from
	 * config, if any.
	 * @param[in] no_hmds If true, do not probe for HMDs, only other
	 * devices.
	 * @param[in] xp Prober: provided for access to the tracking factory,
	 * among other reasons.
	 * @param[out] out_xdevs Array of @ref XRT_MAX_DEVICES_PER_PROBE @c NULL
	 * @ref xrt_device pointers. First elements will be populated with new
	 * devices.
	 *
	 * @return The amount of devices written into @p out_xdevs, 0 if none.
	 *
	 * @note Leeloo Dallas is a reference to The Fifth Element.
	 */
	int (*lelo_dallas_autoprobe)(struct xrt_auto_prober *xap,
	                             cJSON *attached_data,
	                             bool no_hmds,
	                             struct xrt_prober *xp,
	                             struct xrt_device **out_xdevs);
	/*!
	 * Destroy this auto-prober.
	 *
	 * @param xap Self pointer
	 */
	void (*destroy)(struct xrt_auto_prober *xap);
};


#ifdef __cplusplus
}
#endif
