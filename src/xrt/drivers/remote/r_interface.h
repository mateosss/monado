// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to remote driver.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_remote
 */

#pragma once

#include "xrt/xrt_defines.h"

#ifdef __cplusplus
extern "C" {
#endif


struct xrt_device;

/*!
 * @defgroup drv_remote Remote debugging driver
 * @ingroup drv
 *
 * @brief Driver for creating remote debugging devices.
 */

/*!
 * @dir drivers/remote
 *
 * @brief @ref drv_remote files.
 */

/*!
 * Header value to be set in the packet.
 *
 * @ingroup drv_remote
 */
#define R_HEADER_VALUE (*(uint64_t *)"mndrmt1\0")

/*!
 * Data per controller.
 */
struct r_remote_controller_data
{
	struct xrt_pose pose;
	struct xrt_vec3 linear_velocity;
	struct xrt_vec3 angular_velocity;

	float hand_curl[5];

	bool active;
	bool select;
	bool menu;
	bool _pad;
};

/*!
 * Remote data sent from the debugger to the hub.
 *
 * @ingroup drv_remote
 */
struct r_remote_data
{
	uint64_t header;

	struct
	{
		struct xrt_pose pose;
	} hmd;

	struct r_remote_controller_data left, right;
};

/*!
 * Shared connection.
 *
 * @ingroup drv_remote
 */
struct r_remote_connection
{
	int fd;
};

/*!
 * Creates the remote devices.
 *
 * @ingroup drv_remote
 */
int
r_create_devices(uint16_t port,
                 struct xrt_device **out_hmd,
                 struct xrt_device **out_controller_left,
                 struct xrt_device **out_controller_right);

/*!
 * Initializes and connects the connection.
 *
 * @ingroup drv_remote
 */
int
r_remote_connection_init(struct r_remote_connection *rc, const char *addr, uint16_t port);

int
r_remote_connection_read_one(struct r_remote_connection *rc, struct r_remote_data *data);

int
r_remote_connection_write_one(struct r_remote_connection *rc, const struct r_remote_data *data);


#ifdef __cplusplus
}
#endif
