// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Server process functions.
 * @author Pete Black <pblack@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup ipc_server
 */

#include "xrt/xrt_device.h"
#include "xrt/xrt_instance.h"
#include "xrt/xrt_compositor.h"
#include "xrt/xrt_config_have.h"
#include "xrt/xrt_config_os.h"

#include "os/os_time.h"
#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_trace_marker.h"

#include "shared/ipc_shmem.h"
#include "server/ipc_server.h"

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ---- HACK ---- */
extern int
oxr_sdl2_hack_create(void **out_hack);

extern void
oxr_sdl2_hack_start(void *hack, struct xrt_instance *xinst, struct xrt_device **xdevs);

extern void
oxr_sdl2_hack_stop(void **hack_ptr);
/* ---- HACK ---- */

/*
 *
 * Defines and helpers.
 *
 */

DEBUG_GET_ONCE_BOOL_OPTION(exit_on_disconnect, "IPC_EXIT_ON_DISCONNECT", false)
DEBUG_GET_ONCE_LOG_OPTION(ipc_log, "IPC_LOG", U_LOGGING_WARN)

struct _z_sort_data
{
	int32_t index;
	int32_t z_order;
};

static void
init_idev(struct ipc_device *idev, struct xrt_device *xdev)
{
	if (xdev != NULL) {
		idev->io_active = true;
		idev->xdev = xdev;
	} else {
		idev->io_active = false;
	}
}

static void
teardown_idev(struct ipc_device *idev)
{
	xrt_device_destroy(&idev->xdev);
	idev->io_active = false;
}


/*
 *
 * Static functions.
 *
 */
static void
teardown_all(struct ipc_server *s)
{
	u_var_remove_root(s);

	xrt_comp_native_destroy(&s->xcn);

	xrt_syscomp_destroy(&s->xsysc);

	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		teardown_idev(&s->idevs[i]);
	}

	xrt_instance_destroy(&s->xinst);

	ipc_server_mainloop_deinit(&s->ml);

	os_mutex_destroy(&s->global_state_lock);
}

static int
init_tracking_origins(struct ipc_server *s)
{
	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		struct xrt_device *xdev = s->idevs[i].xdev;
		if (xdev == NULL) {
			continue;
		}

		struct xrt_tracking_origin *xtrack = xdev->tracking_origin;
		assert(xtrack != NULL);
		size_t index = 0;

		for (; index < IPC_SERVER_NUM_XDEVS; index++) {
			if (s->xtracks[index] == NULL) {
				s->xtracks[index] = xtrack;
				break;
			}
			if (s->xtracks[index] == xtrack) {
				break;
			}
		}
	}

	return 0;
}

static void
handle_binding(struct ipc_shared_memory *ism,
               struct xrt_binding_profile *xbp,
               struct ipc_shared_binding_profile *isbp,
               uint32_t *input_pair_index_ptr,
               uint32_t *output_pair_index_ptr)
{
	uint32_t input_pair_index = *input_pair_index_ptr;
	uint32_t output_pair_index = *output_pair_index_ptr;

	isbp->name = xbp->name;

	// Copy the initial state and also count the number in input_pairs.
	size_t input_pair_start = input_pair_index;
	for (size_t k = 0; k < xbp->num_inputs; k++) {
		ism->input_pairs[input_pair_index++] = xbp->inputs[k];
	}

	// Setup the 'offsets' and number of input_pairs.
	if (input_pair_start != input_pair_index) {
		isbp->num_inputs = input_pair_index - input_pair_start;
		isbp->first_input_index = input_pair_start;
	}

	// Copy the initial state and also count the number in outputs.
	size_t output_pair_start = output_pair_index;
	for (size_t k = 0; k < xbp->num_outputs; k++) {
		ism->output_pairs[output_pair_index++] = xbp->outputs[k];
	}

	// Setup the 'offsets' and number of output_pairs.
	if (output_pair_start != output_pair_index) {
		isbp->num_outputs = output_pair_index - output_pair_start;
		isbp->first_output_index = output_pair_start;
	}

	*input_pair_index_ptr = input_pair_index;
	*output_pair_index_ptr = output_pair_index;
}

static int
init_shm(struct ipc_server *s)
{
	const size_t size = sizeof(struct ipc_shared_memory);
	xrt_shmem_handle_t handle;
	xrt_result_t result = ipc_shmem_create(size, &handle, (void **)&s->ism);
	if (result != XRT_SUCCESS) {
		return -1;
	}

	// we have a filehandle, we will pass this to
	// our client rather than access via filesystem
	s->ism_handle = handle;


	/*
	 *
	 * Setup the shared memory state.
	 *
	 */

	uint32_t count = 0;
	struct ipc_shared_memory *ism = s->ism;

	count = 0;
	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		struct xrt_tracking_origin *xtrack = s->xtracks[i];
		if (xtrack == NULL) {
			continue;
		}

		// The position of the tracking origin matches that in the
		// servers memory.
		assert(i < IPC_SHARED_MAX_DEVICES);

		struct ipc_shared_tracking_origin *itrack = &ism->itracks[count++];
		memcpy(itrack->name, xtrack->name, sizeof(itrack->name));
		itrack->type = xtrack->type;
		itrack->offset = xtrack->offset;
	}

	ism->num_itracks = count;

	count = 0;
	uint32_t input_index = 0;
	uint32_t output_index = 0;
	uint32_t binding_index = 0;
	uint32_t input_pair_index = 0;
	uint32_t output_pair_index = 0;

	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		struct xrt_device *xdev = s->idevs[i].xdev;
		if (xdev == NULL) {
			continue;
		}

		struct ipc_shared_device *isdev = &ism->isdevs[count++];

		isdev->name = xdev->name;
		memcpy(isdev->str, xdev->str, sizeof(isdev->str));

		isdev->orientation_tracking_supported = xdev->orientation_tracking_supported;
		isdev->position_tracking_supported = xdev->position_tracking_supported;
		isdev->device_type = xdev->device_type;
		isdev->hand_tracking_supported = xdev->hand_tracking_supported;

		// Is this a HMD?
		if (xdev->hmd != NULL) {
			ism->hmd.views[0].display.w_pixels = xdev->hmd->views[0].display.w_pixels;
			ism->hmd.views[0].display.h_pixels = xdev->hmd->views[0].display.h_pixels;
			ism->hmd.views[0].fov = xdev->hmd->views[0].fov;
			ism->hmd.views[1].display.w_pixels = xdev->hmd->views[1].display.w_pixels;
			ism->hmd.views[1].display.h_pixels = xdev->hmd->views[1].display.h_pixels;
			ism->hmd.views[1].fov = xdev->hmd->views[1].fov;

			ism->hmd.blend_mode = xdev->hmd->blend_mode;
		}

		// Setup the tracking origin.
		isdev->tracking_origin_index = (uint32_t)-1;
		for (size_t k = 0; k < IPC_SERVER_NUM_XDEVS; k++) {
			if (xdev->tracking_origin != s->xtracks[k]) {
				continue;
			}

			isdev->tracking_origin_index = k;
			break;
		}

		assert(isdev->tracking_origin_index != (uint32_t)-1);

		// Initial update.
		xrt_device_update_inputs(xdev);

		// Bindings
		size_t binding_start = binding_index;
		for (size_t k = 0; k < xdev->num_binding_profiles; k++) {
			handle_binding(ism, &xdev->binding_profiles[k], &ism->binding_profiles[binding_index++],
			               &input_pair_index, &output_pair_index);
		}

		// Setup the 'offsets' and number of bindings.
		if (binding_start != binding_index) {
			isdev->num_binding_profiles = binding_index - binding_start;
			isdev->first_binding_profile_index = binding_start;
		}

		// Copy the initial state and also count the number in inputs.
		size_t input_start = input_index;
		for (size_t k = 0; k < xdev->num_inputs; k++) {
			ism->inputs[input_index++] = xdev->inputs[k];
		}

		// Setup the 'offsets' and number of inputs.
		if (input_start != input_index) {
			isdev->num_inputs = input_index - input_start;
			isdev->first_input_index = input_start;
		}

		// Copy the initial state and also count the number in outputs.
		size_t output_start = output_index;
		for (size_t k = 0; k < xdev->num_outputs; k++) {
			ism->outputs[output_index++] = xdev->outputs[k];
		}

		// Setup the 'offsets' and number of outputs.
		if (output_start != output_index) {
			isdev->num_outputs = output_index - output_start;
			isdev->first_output_index = output_start;
		}
	}

	// Finally tell the client how many devices we have.
	s->ism->num_isdevs = count;

	return 0;
}

void
ipc_server_handle_failure(struct ipc_server *vs)
{
	// Right now handled just the same as a graceful shutdown.
	vs->running = false;
}

void
ipc_server_handle_shutdown_signal(struct ipc_server *vs)
{
	vs->running = false;
}

void
ipc_server_start_client_listener_thread(struct ipc_server *vs, int fd)
{
	volatile struct ipc_client_state *ics = NULL;
	int32_t cs_index = -1;

	os_mutex_lock(&vs->global_state_lock);

	// find the next free thread in our array (server_thread_index is -1)
	// and have it handle this connection
	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		volatile struct ipc_client_state *_cs = &vs->threads[i].ics;
		if (_cs->server_thread_index < 0) {
			ics = _cs;
			cs_index = i;
			break;
		}
	}
	if (ics == NULL) {
		close(fd);

		// Unlock when we are done.
		os_mutex_unlock(&vs->global_state_lock);

		U_LOG_E("Max client count reached!");
		return;
	}

	struct ipc_thread *it = &vs->threads[cs_index];
	if (it->state != IPC_THREAD_READY && it->state != IPC_THREAD_STOPPING) {
		// we should not get here
		close(fd);

		// Unlock when we are done.
		os_mutex_unlock(&vs->global_state_lock);

		U_LOG_E("Client state management error!");
		return;
	}

	if (it->state != IPC_THREAD_READY) {
		os_thread_join(&it->thread);
		os_thread_destroy(&it->thread);
		it->state = IPC_THREAD_READY;
	}

	it->state = IPC_THREAD_STARTING;
	ics->imc.socket_fd = fd;
	ics->server = vs;
	ics->server_thread_index = cs_index;
	ics->io_active = true;
	os_thread_start(&it->thread, ipc_server_client_thread, (void *)ics);

	// Unlock when we are done.
	os_mutex_unlock(&vs->global_state_lock);
}

static int
init_all(struct ipc_server *s)
{
	// Yes we should be running.
	s->running = true;
	s->exit_on_disconnect = debug_get_bool_option_exit_on_disconnect();

	int ret = xrt_instance_create(NULL, &s->xinst);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	struct xrt_device *xdevs[IPC_SERVER_NUM_XDEVS] = {0};
	ret = xrt_instance_select(s->xinst, xdevs, IPC_SERVER_NUM_XDEVS);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	// Copy the devices over into the idevs array.
	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}

		init_idev(&s->idevs[i], xdevs[i]);
		xdevs[i] = NULL;
	}

	// If we don't have a HMD shutdown.
	if (s->idevs[0].xdev == NULL) {
		teardown_all(s);
		return -1;
	}

	ret = init_tracking_origins(s);
	if (ret < 0) {
		teardown_all(s);
		return -1;
	}

	ret = xrt_instance_create_system_compositor(s->xinst, s->idevs[0].xdev, &s->xsysc);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	struct xrt_session_info xsi = {
	    .is_overlay = false,
	    .flags = 0,
	    .z_order = 0,
	};
	ret = xrt_syscomp_create_native_compositor(s->xsysc, &xsi, &s->xcn);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	ret = init_shm(s);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	ret = ipc_server_mainloop_init(&s->ml);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	// Init all of the render timing helpers.
	for (size_t i = 0; i < ARRAY_SIZE(s->threads); i++) {
		u_rt_helper_init((struct u_rt_helper *)&s->threads[i].ics.urth);
	}

	ret = os_mutex_init(&s->global_state_lock);
	if (ret < 0) {
		teardown_all(s);
		return ret;
	}

	s->ll = debug_get_log_option_ipc_log();

	u_var_add_root(s, "IPC Server", false);
	u_var_add_ro_u32(s, &s->ll, "log level");
	u_var_add_bool(s, &s->exit_on_disconnect, "exit_on_disconnect");
	u_var_add_bool(s, (void *)&s->running, "running");

	return 0;
}

static uint32_t
find_event_slot(volatile struct ipc_client_state *ics)
{
	uint64_t oldest_event_timestamp = UINT64_MAX;
	uint32_t oldest_event_index = 0;
	for (uint32_t i = 0; i < IPC_EVENT_QUEUE_SIZE; i++) {
		if (ics->queued_events->timestamp < oldest_event_timestamp) {
			oldest_event_index = i;
		}
		if (!ics->queued_events[i].pending) {
			return i;
		}
	}

	U_LOG_E("Event queue full - unconsumed event lost!");
	return oldest_event_index;
}

static void
transition_overlay_visibility(volatile struct ipc_client_state *ics, bool visible)
{
	uint32_t event_slot = find_event_slot(ics);
	uint64_t timestamp = os_monotonic_get_ns();

	volatile struct ipc_queued_event *qe = &ics->queued_events[event_slot];

	qe->timestamp = timestamp;
	qe->pending = true;
	qe->event.type = XRT_COMPOSITOR_EVENT_OVERLAY_CHANGE;
	qe->event.overlay.visible = visible;
}

static void
send_client_state(volatile struct ipc_client_state *ics)
{
	uint32_t event_slot = find_event_slot(ics);
	uint64_t timestamp = os_monotonic_get_ns();

	volatile struct ipc_queued_event *qe = &ics->queued_events[event_slot];

	qe->timestamp = timestamp;
	qe->pending = true;
	qe->event.type = XRT_COMPOSITOR_EVENT_STATE_CHANGE;
	qe->event.state.visible = ics->client_state.session_visible;
	qe->event.state.focused = ics->client_state.session_focused;
}

static bool
_update_projection_layer(struct xrt_compositor *xc,
                         volatile struct ipc_client_state *ics,
                         volatile struct ipc_layer_entry *layer,
                         uint32_t i)
{
	// xdev
	uint32_t device_id = layer->xdev_id;
	// left
	uint32_t lxsci = layer->swapchain_ids[0];
	// right
	uint32_t rxsci = layer->swapchain_ids[1];

	struct xrt_device *xdev = get_xdev(ics, device_id);
	struct xrt_swapchain *lxcs = ics->xscs[lxsci];
	struct xrt_swapchain *rxcs = ics->xscs[rxsci];

	if (lxcs == NULL || rxcs == NULL) {
		U_LOG_E("Invalid swap chain for projection layer!");
		return false;
	}

	if (xdev == NULL) {
		U_LOG_E("Invalid xdev for projection layer!");
		return false;
	}

	// Cast away volatile.
	struct xrt_layer_data *data = (struct xrt_layer_data *)&layer->data;

	xrt_comp_layer_stereo_projection(xc, xdev, lxcs, rxcs, data);

	return true;
}

static bool
_update_projection_layer_depth(struct xrt_compositor *xc,
                               volatile struct ipc_client_state *ics,
                               volatile struct ipc_layer_entry *layer,
                               uint32_t i)
{
	// xdev
	uint32_t xdevi = layer->xdev_id;
	// left
	uint32_t l_xsci = layer->swapchain_ids[0];
	// right
	uint32_t r_xsci = layer->swapchain_ids[1];
	// left
	uint32_t l_d_xsci = layer->swapchain_ids[2];
	// right
	uint32_t r_d_xsci = layer->swapchain_ids[3];

	struct xrt_device *xdev = get_xdev(ics, xdevi);
	struct xrt_swapchain *l_xcs = ics->xscs[l_xsci];
	struct xrt_swapchain *r_xcs = ics->xscs[r_xsci];
	struct xrt_swapchain *l_d_xcs = ics->xscs[l_d_xsci];
	struct xrt_swapchain *r_d_xcs = ics->xscs[r_d_xsci];

	if (l_xcs == NULL || r_xcs == NULL || l_d_xcs == NULL || r_d_xcs == NULL) {
		U_LOG_E("Invalid swap chain for projection layer!");
		return false;
	}

	if (xdev == NULL) {
		U_LOG_E("Invalid xdev for projection layer!");
		return false;
	}

	// Cast away volatile.
	struct xrt_layer_data *data = (struct xrt_layer_data *)&layer->data;

	xrt_comp_layer_stereo_projection_depth(xc, xdev, l_xcs, r_xcs, l_d_xcs, r_d_xcs, data);

	return true;
}

static bool
do_single(struct xrt_compositor *xc,
          volatile struct ipc_client_state *ics,
          volatile struct ipc_layer_entry *layer,
          uint32_t i,
          const char *name,
          struct xrt_device **out_xdev,
          struct xrt_swapchain **out_xcs,
          struct xrt_layer_data **out_data)
{
	uint32_t device_id = layer->xdev_id;
	uint32_t sci = layer->swapchain_ids[0];

	struct xrt_device *xdev = get_xdev(ics, device_id);
	struct xrt_swapchain *xcs = ics->xscs[sci];

	if (xcs == NULL) {
		U_LOG_E("Invalid swapchain for '%u' layer, '%s'!", i, name);
		return false;
	}

	if (xdev == NULL) {
		U_LOG_E("Invalid xdev for '%u' layer, '%s'!", i, name);
		return false;
	}

	// Cast away volatile.
	struct xrt_layer_data *data = (struct xrt_layer_data *)&layer->data;

	*out_xdev = xdev;
	*out_xcs = xcs;
	*out_data = data;

	return true;
}

static bool
_update_quad_layer(struct xrt_compositor *xc,
                   volatile struct ipc_client_state *ics,
                   volatile struct ipc_layer_entry *layer,
                   uint32_t i)
{
	struct xrt_device *xdev;
	struct xrt_swapchain *xcs;
	struct xrt_layer_data *data;

	if (!do_single(xc, ics, layer, i, "quad", &xdev, &xcs, &data)) {
		return false;
	}

	xrt_comp_layer_quad(xc, xdev, xcs, data);

	return true;
}

static bool
_update_cube_layer(struct xrt_compositor *xc,
                   volatile struct ipc_client_state *ics,
                   volatile struct ipc_layer_entry *layer,
                   uint32_t i)
{
	struct xrt_device *xdev;
	struct xrt_swapchain *xcs;
	struct xrt_layer_data *data;

	if (!do_single(xc, ics, layer, i, "cube", &xdev, &xcs, &data)) {
		return false;
	}

	xrt_comp_layer_cube(xc, xdev, xcs, data);

	return true;
}

static bool
_update_cylinder_layer(struct xrt_compositor *xc,
                       volatile struct ipc_client_state *ics,
                       volatile struct ipc_layer_entry *layer,
                       uint32_t i)
{
	struct xrt_device *xdev;
	struct xrt_swapchain *xcs;
	struct xrt_layer_data *data;

	if (!do_single(xc, ics, layer, i, "cylinder", &xdev, &xcs, &data)) {
		return false;
	}

	xrt_comp_layer_cylinder(xc, xdev, xcs, data);

	return true;
}

static bool
_update_equirect1_layer(struct xrt_compositor *xc,
                        volatile struct ipc_client_state *ics,
                        volatile struct ipc_layer_entry *layer,
                        uint32_t i)
{
	struct xrt_device *xdev;
	struct xrt_swapchain *xcs;
	struct xrt_layer_data *data;

	if (!do_single(xc, ics, layer, i, "equirect1", &xdev, &xcs, &data)) {
		return false;
	}

	xrt_comp_layer_equirect1(xc, xdev, xcs, data);

	return true;
}

static bool
_update_equirect2_layer(struct xrt_compositor *xc,
                        volatile struct ipc_client_state *ics,
                        volatile struct ipc_layer_entry *layer,
                        uint32_t i)
{
	struct xrt_device *xdev;
	struct xrt_swapchain *xcs;
	struct xrt_layer_data *data;

	if (!do_single(xc, ics, layer, i, "equirect2", &xdev, &xcs, &data)) {
		return false;
	}

	xrt_comp_layer_equirect2(xc, xdev, xcs, data);

	return true;
}

static int
_overlay_sort_func(const void *a, const void *b)
{
	struct _z_sort_data *oa = (struct _z_sort_data *)a;
	struct _z_sort_data *ob = (struct _z_sort_data *)b;
	if (oa->z_order < ob->z_order) {
		return -1;
	}
	if (oa->z_order > ob->z_order) {
		return 1;
	}
	return 0;
}

static bool
_update_layers(struct ipc_server *s, struct xrt_compositor *xc)
{
	IPC_TRACE_MARKER();

	struct _z_sort_data z_data[IPC_MAX_CLIENTS];

	// initialise, and fill in overlay app data
	for (int32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		volatile struct ipc_client_state *ics = &s->threads[i].ics;
		z_data[i].index = -1;
		z_data[i].z_order = -1;
		// we need to create a list of overlay applications, sorted by z
		if (ics->client_state.session_overlay) {
			if (ics->client_state.session_active) {
				z_data[i].index = i;
				z_data[i].z_order = ics->client_state.z_order;
			}
		}
	}

	// ensure our primary application is enabled,
	// and rendered first in the stack
	if (s->active_client_index >= 0) {
		z_data[s->active_client_index].index = s->active_client_index;
		z_data[s->active_client_index].z_order = INT32_MIN;
	}

	// sort the stack array
	qsort(z_data, IPC_MAX_CLIENTS, sizeof(struct _z_sort_data), _overlay_sort_func);

	// render the layer stack
	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		struct _z_sort_data *zd = &z_data[i];
		if (zd->index < 0) {
			continue;
		}

		volatile struct ipc_client_state *ics = &s->threads[zd->index].ics;

		for (uint32_t j = 0; j < ics->render_state.num_layers; j++) {
			volatile struct ipc_layer_entry *layer = &ics->render_state.layers[j];

			switch (layer->data.type) {
			case XRT_LAYER_STEREO_PROJECTION:
				if (!_update_projection_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_STEREO_PROJECTION_DEPTH:
				if (!_update_projection_layer_depth(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_QUAD:
				if (!_update_quad_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_CUBE:
				if (!_update_cube_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_CYLINDER:
				if (!_update_cylinder_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_EQUIRECT1:
				if (!_update_equirect1_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			case XRT_LAYER_EQUIRECT2:
				if (!_update_equirect2_layer(xc, ics, layer, i)) {
					return false;
				}
				break;
			default: U_LOG_E("Unhandled layer type '%i'!", layer->data.type); break;
			}
		}
	}

	return true;
}

static void
broadcast_timings(struct ipc_server *s,
                  uint64_t predicted_display_time_ns,
                  uint64_t predicted_display_period_ns,
                  uint64_t diff_ns)
{
	IPC_TRACE_MARKER();

	os_mutex_lock(&s->global_state_lock);

	// Broadcast the new timing information to the helpers.
	for (size_t i = 0; i < ARRAY_SIZE(s->threads); i++) {
		struct u_rt_helper *urth = (struct u_rt_helper *)&s->threads[i].ics.urth;
		u_rt_helper_new_sample(          //
		    urth,                        //
		    predicted_display_time_ns,   //
		    predicted_display_period_ns, //
		    diff_ns);                    //
	}

	os_mutex_unlock(&s->global_state_lock);
}

static int
main_loop(struct ipc_server *s)
{
	struct xrt_compositor *xc = &s->xcn->base;

	// make sure all our client connections have a handle to the
	// compositor and consistent initial state

	while (s->running) {
		int64_t frame_id;
		uint64_t predicted_display_time_ns;
		uint64_t predicted_display_period_ns;

		xrt_comp_wait_frame(xc, &frame_id, &predicted_display_time_ns, &predicted_display_period_ns);

		uint64_t now_ns = os_monotonic_get_ns();
		uint64_t diff_ns = predicted_display_time_ns - now_ns;

		broadcast_timings(s, predicted_display_time_ns, predicted_display_period_ns, diff_ns);

		xrt_comp_begin_frame(xc, frame_id);
		xrt_comp_layer_begin(xc, frame_id, 0);

		_update_layers(s, xc);

		xrt_comp_layer_commit(xc, frame_id, XRT_GRAPHICS_SYNC_HANDLE_INVALID);

		// Check polling last, so we know we have valid timing data.
		ipc_server_mainloop_poll(s, &s->ml);
	}

	return 0;
}


static void
handle_overlay_client_events(volatile struct ipc_client_state *ics, int active_id, int prev_active_id)
{
	// this is an overlay session.
	if (ics->client_state.session_overlay) {

		// switch between main applications
		if (active_id >= 0 && prev_active_id >= 0) {
			transition_overlay_visibility(ics, false);
			transition_overlay_visibility(ics, true);
		}

		// switch from idle to active application
		if (active_id >= 0 && prev_active_id < 0) {
			transition_overlay_visibility(ics, true);
		}

		// switch from active application to idle
		if (active_id < 0 && prev_active_id >= 0) {
			transition_overlay_visibility(ics, false);
		}
	}
}

static void
handle_focused_client_events(volatile struct ipc_client_state *ics, int active_id, int prev_active_id)
{

	// if our prev active id is -1 and our cur active id is -1, we
	// can bail out early

	if (active_id == -1 && prev_active_id == -1) {
		return;
	}

	// set visibility/focus to false on all applications
	ics->client_state.session_focused = false;
	ics->client_state.session_visible = false;

	// do we have a primary application?
	if (active_id >= 0) {

		// if we are an overlay, we are always visible
		// if we have a primary application
		if (ics->client_state.session_overlay) {
			ics->client_state.session_visible = true;
		}

		// set visible + focused if we are the primary
		// application
		if (ics->server_thread_index == active_id) {
			ics->client_state.session_visible = true;
			ics->client_state.session_focused = true;
		}
		send_client_state(ics);
		return;
	}

	// no primary application, set all overlays to synchronised
	// state
	if (ics->client_state.session_overlay) {
		ics->client_state.session_focused = false;
		ics->client_state.session_visible = false;
		send_client_state(ics);
	}
}

void
init_server_state(struct ipc_server *s)
{

	// set up initial state for global vars, and each client state

	s->active_client_index = -1; // we start off with no active client.
	s->last_active_client_index = -1;
	s->current_slot_index = 0;

	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		volatile struct ipc_client_state *ics = &s->threads[i].ics;
		ics->server = s;
		ics->xc = &s->xcn->base;
		ics->server_thread_index = -1;
	}
}


/*
 *
 * Exported functions.
 *
 */

void
update_server_state(struct ipc_server *s)
{
	// multiple threads could call this at the same time.
	os_mutex_lock(&s->global_state_lock);

	// if our client that is set to active is still active,
	// and it is the same as our last active client, we can
	// early-out, as no events need to be sent

	if (s->active_client_index >= 0) {

		volatile struct ipc_client_state *ics = &s->threads[s->active_client_index].ics;

		if (ics->client_state.session_active && s->active_client_index == s->last_active_client_index) {
			os_mutex_unlock(&s->global_state_lock);
			return;
		}
	}


	// our active application has changed - this would typically be
	// switched by the monado-ctl application or other app making a
	// 'set active application' ipc call, or it could be a
	// connection loss resulting in us needing to 'fall through' to
	// the first active application
	//, or finally to the idle 'wallpaper' images.


	bool set_idle = true;
	int fallback_active_application = -1;

	// do we have a fallback application?
	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		volatile struct ipc_client_state *ics = &s->threads[i].ics;
		if (ics->client_state.session_overlay == false && ics->server_thread_index >= 0 &&
		    ics->client_state.session_active) {
			fallback_active_application = i;
			set_idle = false;
		}
	}

	// if our currently-set active primary application is not
	// actually active/displayable, use the fallback application
	// instead.
	volatile struct ipc_client_state *ics = &s->threads[s->active_client_index].ics;
	if (!(ics->client_state.session_overlay == false && s->active_client_index >= 0 &&
	      ics->client_state.session_active)) {
		s->active_client_index = fallback_active_application;
	}


	// if we have no applications to fallback to, enable the idle
	// wallpaper.
	if (set_idle) {
		s->active_client_index = -1;
	}

	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {

		volatile struct ipc_client_state *ics = &s->threads[i].ics;
		if (ics->server_thread_index >= 0) {

			handle_focused_client_events(ics, s->active_client_index, s->last_active_client_index);

			handle_overlay_client_events(ics, s->active_client_index, s->last_active_client_index);
		}
	}

	s->last_active_client_index = s->active_client_index;

	os_mutex_unlock(&s->global_state_lock);
}

#ifndef XRT_OS_ANDROID
int
ipc_server_main(int argc, char **argv)
{
	struct ipc_server *s = U_TYPED_CALLOC(struct ipc_server);

	/* ---- HACK ---- */
	// need to create early before any vars are added
	oxr_sdl2_hack_create(&s->hack);
	/* ---- HACK ---- */

	int ret = init_all(s);
	if (ret < 0) {
		free(s);
		return ret;
	}

	init_server_state(s);

	struct xrt_device *xdevs[IPC_SERVER_NUM_XDEVS];
	for (size_t i = 0; i < IPC_SERVER_NUM_XDEVS; i++) {
		xdevs[i] = s->idevs[i].xdev;
	}

	/* ---- HACK ---- */
	oxr_sdl2_hack_start(s->hack, s->xinst, xdevs);
	/* ---- HACK ---- */

	ret = main_loop(s);

	/* ---- HACK ---- */
	oxr_sdl2_hack_stop(&s->hack);
	/* ---- HACK ---- */

	teardown_all(s);
	free(s);

	U_LOG_I("Server exiting: '%i'!", ret);

	return ret;
}

#endif // !XRT_OS_ANDROID

#ifdef XRT_OS_ANDROID
int
ipc_server_main_android(struct ipc_server **ps, void (*startup_complete_callback)(void *data), void *data)
{
	struct ipc_server *s = U_TYPED_CALLOC(struct ipc_server);
	U_LOG_D("Created IPC server!");

	int ret = init_all(s);
	if (ret < 0) {
		free(s);
		return ret;
	}

	init_server_state(s);

	*ps = s;
	startup_complete_callback(data);

	ret = main_loop(s);

	teardown_all(s);
	free(s);

	U_LOG_I("Server exiting '%i'!", ret);

	return ret;
}
#endif // XRT_OS_ANDROID
