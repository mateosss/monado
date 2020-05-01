// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Common protocol definition.
 * @author Pete Black <pblack@collabora.com>
 * @ingroup ipc
 */

#pragma once

#include "xrt/xrt_device.h"
#include "xrt/xrt_compiler.h"

#define IPC_MSG_SOCK_FILE "/tmp/monado_comp_ipc"
#define IPC_MAX_SWAPCHAIN_FDS 8
#define IPC_CRED_SIZE 1    // auth not implemented
#define IPC_BUF_SIZE 512   // must be >= largest message length in bytes
#define IPC_MAX_VIEWS 8    // max views we will return configs for
#define IPC_MAX_FORMATS 32 // max formats our server-side compositor supports
#define IPC_MAX_DEVICES 8  // max number of devices we will map via shared mem
#define IPC_MAX_LAYERS 16
#define IPC_MAX_SLOTS 3

#define IPC_SHARED_MAX_DEVICES 8
#define IPC_SHARED_MAX_INPUTS 1024
#define IPC_SHARED_MAX_OUTPUTS 128

/*
 *
 * Shared memory structs.
 *
 */

struct ipc_shared_device
{
	//! Enum identifier of the device.
	enum xrt_device_name name;

	//! A string describing the device.
	char str[XRT_DEVICE_NAME_LEN];

	//! Number of inputs.
	uint32_t num_inputs;
	//! 'Offset' into the array of inputs where this devices inputs starts.
	uint32_t first_input_index;

	//! Number of outputs.
	uint32_t num_outputs;
	//! 'Offset' into the array of outputs where this devices outputs
	//! starts.
	uint32_t first_output_index;
};

struct ipc_layer_stereo_projection
{
	uint64_t timestamp;

	uint32_t xdev_id;
	enum xrt_input_name name;
#if 0 /* LAYERS */
	enum xrt_layer_composition_flags layer_flags;
#endif

	struct
	{
		uint32_t swapchain_id;
		uint32_t image_index;
#if 0 /* LAYERS */
		struct xrt_rect rect;
#endif
		uint32_t array_index;
		struct xrt_fov fov;
		struct xrt_pose pose;
	} l, r;
};

struct ipc_layer_quad
{
	uint64_t timestamp;

	uint32_t xdev_id;
	enum xrt_input_name name;
#if 0 /* LAYERS */
	enum xrt_layer_composition_flags layer_flags;
#endif

	uint32_t swapchain_id;
	uint32_t image_index;
#if 0 /* LAYERS */
	struct xrt_rect rect;
#endif
	uint32_t array_index;
	struct xrt_pose pose;
	struct xrt_vec2 size;
};

enum ipc_layer_type
{
	IPC_LAYER_STEREO_PROJECTION,
	IPC_LAYER_QUAD,
};

struct ipc_layer_entry
{
	enum ipc_layer_type type;

	union {
		struct ipc_layer_quad quad;
		struct ipc_layer_stereo_projection stereo;
	};
};

struct ipc_layer_slot
{
	enum xrt_blend_mode env_blend_mode;
	uint32_t num_layers;
	struct ipc_layer_entry layers[IPC_MAX_LAYERS];
};

/*!
 * A big struct that contains all data that is shared to a client, no pointers
 * allowed in this. To get the inputs of a device you go:
 *
 * ```C++
 * struct xrt_input *
 * helper(struct ipc_shared_memory *ism, uin32_t device_id, size_t input)
 * {
 * 	size_t index = ism->idevs[device_id]->first_input_index + input;
 * 	return &ism->inputs[index];
 * }
 * ```
 */
struct ipc_shared_memory
{
	size_t num_idevs;
	struct ipc_shared_device idevs[IPC_SHARED_MAX_DEVICES];

	struct
	{
		struct
		{
			/*!
			 * Pixel properties of this display, not in absolute
			 * screen coordinates that the compositor sees. So
			 * before any rotation is applied by xrt_view::rot.
			 *
			 * The xrt_view::display::w_pixels &
			 * xrt_view::display::h_pixels become the recommdnded
			 * image size for this view.
			 */
			struct
			{
				uint32_t w_pixels;
				uint32_t h_pixels;
			} display;

			/*!
			 * Fov expressed in OpenXR.
			 */
			struct xrt_fov fov;
		} views[2];
	} hmd;

	struct xrt_input inputs[IPC_SHARED_MAX_INPUTS];

	struct xrt_output outputs[IPC_SHARED_MAX_OUTPUTS];

	struct ipc_layer_slot slots[IPC_MAX_SLOTS];
};


/*
 *
 * Enums
 *
 */

typedef enum ipc_result
{
	IPC_SUCCESS = 0,
	IPC_FAILURE,
} ipc_result_t;


/*
 *
 * Reset of protocol is generated.
 *
 */

#include "ipc_protocol_generated.h"