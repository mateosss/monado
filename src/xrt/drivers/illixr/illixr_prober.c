// Copyright 2020-2021, The Board of Trustees of the University of Illinois.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  ILLIXR prober
 * @author RSIM Group <illixr@cs.illinois.edu>
 * @ingroup drv_illixr
 */

#include "xrt/xrt_prober.h"
#include "util/u_misc.h"
#include "util/u_debug.h"

#include "illixr_interface.h"


struct illixr_prober
{
	struct xrt_auto_prober base;
};

static inline struct illixr_prober *
illixr_prober(struct xrt_auto_prober *p)
{
	return (struct illixr_prober *)p;
}

static void
illixr_prober_destroy(struct xrt_auto_prober *p)
{
	struct illixr_prober *dp = illixr_prober(p);

	free(dp);
}

static int
illixr_prober_autoprobe(struct xrt_auto_prober *xap,
                        cJSON *attached_data,
                        bool no_hmds,
                        struct xrt_prober *xp,
                        struct xrt_device **out_xdevs)
{
	struct illixr_prober *dp = illixr_prober(xap);
	(void)dp;

	if (no_hmds) {
		return 0;
	}

	const char *illixr_path, *illixr_comp;
	illixr_path = getenv("ILLIXR_PATH");
	illixr_comp = getenv("ILLIXR_COMP");
	if (!illixr_path || !illixr_comp) {
		return 0;
	}

	out_xdevs[0] = illixr_hmd_create(illixr_path, illixr_comp);
	return 1;
}

struct xrt_auto_prober *
illixr_create_auto_prober()
{
	struct illixr_prober *dp = U_TYPED_CALLOC(struct illixr_prober);
	dp->base.destroy = illixr_prober_destroy;
	dp->base.lelo_dallas_autoprobe = illixr_prober_autoprobe;

	return &dp->base;
}
