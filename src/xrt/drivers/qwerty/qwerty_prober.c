#include "qwerty_device.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_logging.h"
#include "xrt/xrt_prober.h"

// Using INFO as default to inform events real devices could report physically
DEBUG_GET_ONCE_LOG_OPTION(qwerty_log, "QWERTY_LOG", U_LOGGING_INFO)

// Driver disabled by default as it does not work with arbitrary configurations
DEBUG_GET_ONCE_BOOL_OPTION(qwerty_enable, "QWERTY_ENABLE", false)

struct qwerty_prober
{
	struct xrt_auto_prober base;
};

static struct qwerty_prober *
qwerty_prober(struct xrt_auto_prober *p)
{
	return (struct qwerty_prober *)p;
}

static void
qwerty_prober_destroy(struct xrt_auto_prober *p)
{
	struct qwerty_prober *qp = qwerty_prober(p);
	free(qp);
}

static int
qwerty_prober_autoprobe(struct xrt_auto_prober *xap,
                        cJSON *attached_data,
                        bool no_hmds,
                        struct xrt_prober *xp,
                        struct xrt_device **out_xdevs)
{
	bool qwerty_enabled = debug_get_bool_option_qwerty_enable();
	if (!qwerty_enabled)
		return 0;

	bool hmd_wanted = !no_hmds; // Hopefully easier to reason about

	// XXXFUT: How fine grained can the user control what controllers/hmd combination
	// they want? It would be nice to provide total control of that to the user.

	struct qwerty_hmd *qhmd = hmd_wanted ? qwerty_hmd_create() : NULL;
	struct qwerty_controller *qleft = qwerty_controller_create(true, qhmd);
	struct qwerty_controller *qright = qwerty_controller_create(false, qhmd);

	enum u_logging_level log_level = debug_get_log_option_qwerty_log();
	qwerty_system_create(qhmd, qleft, qright, log_level);

	struct qwerty_device *qd_hmd = &qhmd->base;
	struct qwerty_device *qd_left = &qleft->base;
	struct qwerty_device *qd_right = &qright->base;

	if (hmd_wanted)
		out_xdevs[0] = &qd_hmd->base;
	out_xdevs[1 - !hmd_wanted] = &qd_left->base;
	out_xdevs[2 - !hmd_wanted] = &qd_right->base;

	int num_qwerty_devices = hmd_wanted + 2;
	return num_qwerty_devices;
}

struct xrt_auto_prober *
qwerty_create_auto_prober()
{
	struct qwerty_prober *qp = U_TYPED_CALLOC(struct qwerty_prober);
	qp->base.name = "Qwerty";
	qp->base.destroy = qwerty_prober_destroy;
	qp->base.lelo_dallas_autoprobe = qwerty_prober_autoprobe;

	return &qp->base;
}
