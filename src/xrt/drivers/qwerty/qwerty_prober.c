#include "qwerty_device.h"
#include "util/u_misc.h"
#include "xrt/xrt_prober.h"

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
	struct qwerty_device *qctrl_left = qwerty_controller_create(true);
	struct qwerty_device *qctrl_right = qwerty_controller_create(false);

	// All devices should be able to reference other ones, qdevs should only be written here.
	struct qwerty_devices qdevs = {qctrl_left, qctrl_right};
	qctrl_left->qdevs = qdevs;
	qctrl_right->qdevs = qdevs;

	out_xdevs[0] = &qctrl_left->base;
	out_xdevs[1] = &qctrl_right->base;

	int num_qwerty_devices = 2;
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
