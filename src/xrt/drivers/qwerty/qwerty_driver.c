#include <stddef.h>
#include <stdio.h>
#include "xrt/xrt_prober.h"

int
qwerty_found(struct xrt_prober *xp,
           struct xrt_prober_device **devices,
           size_t num_devices,
           size_t index,
           cJSON *attached_data,
           struct xrt_device **out_xdevs) {
	printf(">>> qwerty_found()\n");
	return 0;
}
