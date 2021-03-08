// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Shared default implementation of the instance: pieces that are used
 * whether or not there's a compositor.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */
#pragma once

#include "target_lists.h"

#include "xrt/xrt_prober.h"
#include "xrt/xrt_instance.h"

#include "util/u_misc.h"


/*
 *
 * Struct and helpers.
 *
 */

/*!
 * Main "real" instance implementation.
 *
 * Used in instances both with and without compositor access.
 *
 * @implements xrt_instance
 */
struct t_instance
{
	struct xrt_instance base;
	struct xrt_prober *xp;
};

static inline struct t_instance *
t_instance(struct xrt_instance *xinst)
{
	return (struct t_instance *)xinst;
}


/*
 *
 * Member functions.
 *
 */

static int
t_instance_select(struct xrt_instance *xinst, struct xrt_device **xdevs, size_t num_xdevs)
{
	struct t_instance *tinst = t_instance(xinst);

	int ret = xrt_prober_probe(tinst->xp);
	if (ret < 0) {
		return ret;
	}

	return xrt_prober_select(tinst->xp, xdevs, num_xdevs);
}

static int
t_instance_get_prober(struct xrt_instance *xinst, struct xrt_prober **out_xp)
{
	struct t_instance *tinst = t_instance(xinst);

	*out_xp = tinst->xp;

	return 0;
}

static void
t_instance_destroy(struct xrt_instance *xinst)
{
	struct t_instance *tinst = t_instance(xinst);

	xrt_prober_destroy(&tinst->xp);
	free(tinst);
}
