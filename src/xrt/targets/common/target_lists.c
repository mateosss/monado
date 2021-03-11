// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Common things to pull into a target.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */

#include "xrt/xrt_config_drivers.h"

#include "target_lists.h"

#ifdef XRT_BUILD_DRIVER_ARDUINO
#include "arduino/arduino_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_DUMMY
#include "dummy/dummy_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_HDK
#include "hdk/hdk_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_OHMD
#include "ohmd/oh_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
#include "ht/ht_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_NS
#include "north_star/ns_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_PSMV
#include "psmv/psmv_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_PSVR
#include "psvr/psvr_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_HYDRA
#include "hydra/hydra_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_SURVIVE
#include "survive/survive_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_VIVE
#include "vive/vive_prober.h"
#endif

#ifdef XRT_BUILD_DRIVER_DAYDREAM
#include "daydream/daydream_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_ANDROID
#include "android/android_prober.h"
#endif

#ifdef XRT_BUILD_DRIVER_ILLIXR
#include "illixr/illixr_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_RS
#include "realsense/rs_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_QWERTY
#include "qwerty/qwerty_interface.h"
#endif

/*!
 * Each entry should be a vendor ID (VID), product ID (PID), a "found" function,
 * and a string literal name.
 *
 * The "found" function must return `int` and take as parameters:
 *
 * - `struct xrt_prober *xp`
 * - `struct xrt_prober_device **devices`
 * - `size_t index`
 * - `struct xrt_device **out_xdevs` (an array of XRT_MAX_DEVICES_PER_PROBE
 * xrt_device pointers)
 *
 * It is called when devices[index] match the VID and PID in the list.
 * It should return 0 if it decides not to create any devices, negative on
 * error, and the number of devices created if it creates one or more: it should
 * assign sequential elements of out_xdevs to the created devices.
 */
struct xrt_prober_entry target_entry_list[] = {
#ifdef XRT_BUILD_DRIVER_PSMV
    {PSMV_VID, PSMV_PID_ZCM1, psmv_found, "PS Move Controller (ZCM1)", "psmv"},
    {PSMV_VID, PSMV_PID_ZCM2, psmv_found, "PS Move Controller (ZCM2)", "psmv"},
#endif // XRT_BUILD_DRIVER_PSMV

#ifdef XRT_BUILD_DRIVER_HYDRA
    {HYDRA_VID, HYDRA_PID, hydra_found, "Razer Hydra", "hydra"},
#endif // XRT_BUILD_DRIVER_HYDRA

#ifdef XRT_BUILD_DRIVER_HDK
    {HDK_VID, HDK_PID, hdk_found, "OSVR HDK", "osvr"},
#endif // XRT_BUILD_DRIVER_HDK

#ifdef XRT_BUILD_DRIVER_VIVE
    {HTC_VID, VIVE_PID, vive_found, "HTC Vive", "vive"},
    {HTC_VID, VIVE_PRO_MAINBOARD_PID, vive_found, "HTC Vive Pro", "vive"},
    {VALVE_VID, VIVE_PRO_LHR_PID, vive_found, "Valve Index", "vive"},
    {VALVE_VID, VIVE_WATCHMAN_DONGLE, vive_controller_found, "HTC Vive Watchman Wireless Device", "vive"},
    {VALVE_VID, VIVE_WATCHMAN_DONGLE_GEN2, vive_controller_found, "Valve Watchman Wireless Device", "vive"},
#endif

    {0x0000, 0x0000, NULL, NULL, NULL}, // Terminate
};

struct xrt_prober_entry *target_entry_lists[] = {
    target_entry_list,
    NULL, // Terminate
};

xrt_auto_prober_creator target_auto_list[] = {
#ifdef XRT_BUILD_DRIVER_PSVR
    psvr_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_ARDUINO
    // Before OpenHMD
    arduino_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_DAYDREAM
    // Before OpenHMD
    daydream_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_SURVIVE
    survive_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_OHMD
    // OpenHMD almost as the end as we want to override it with native drivers.
    oh_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
    ht_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_NS
    // North star driver here for now.
    ns_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_ANDROID
    android_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_ILLIXR
    illixr_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_RS
    rs_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_QWERTY
    qwerty_create_auto_prober,
#endif

#ifdef XRT_BUILD_DRIVER_DUMMY
    // Dummy headset driver last.
    dummy_create_auto_prober,
#endif

    NULL, // Terminate
};

struct xrt_prober_entry_lists target_lists = {
    target_entry_lists,
    target_auto_list,
    NULL,
};
