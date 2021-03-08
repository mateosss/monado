// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Handling functions called from generated dispatch function.
 * @author Pete Black <pblack@collabora.com>
 * @ingroup ipc_server
 */

#include "xrt/xrt_gfx_native.h"

#include "util/u_misc.h"

#include "server/ipc_server.h"
#include "ipc_server_generated.h"

#ifdef XRT_GRAPHICS_SYNC_HANDLE_IS_FD
#include <unistd.h>
#endif


/*
 *
 * Helper functions.
 *
 */

static xrt_result_t
validate_swapchain_state(volatile struct ipc_client_state *ics, uint32_t *out_index)
{
	// Our handle is just the index for now.
	uint32_t index = 0;
	for (; index < IPC_MAX_CLIENT_SWAPCHAINS; index++) {
		if (!ics->swapchain_data[index].active) {
			break;
		}
	}

	if (index >= IPC_MAX_CLIENT_SWAPCHAINS) {
		IPC_ERROR(ics->server, "Too many swapchains!");
		return XRT_ERROR_IPC_FAILURE;
	}

	*out_index = index;

	return XRT_SUCCESS;
}

static void
set_swapchain_info(volatile struct ipc_client_state *ics,
                   uint32_t index,
                   const struct xrt_swapchain_create_info *info,
                   struct xrt_swapchain *xsc)
{
	ics->xscs[index] = xsc;
	ics->swapchain_data[index].active = true;
	ics->swapchain_data[index].width = info->width;
	ics->swapchain_data[index].height = info->height;
	ics->swapchain_data[index].format = info->format;
	ics->swapchain_data[index].num_images = xsc->num_images;
}


/*
 *
 * Handle functions.
 *
 */

xrt_result_t
ipc_handle_instance_get_shm_fd(volatile struct ipc_client_state *ics,
                               uint32_t max_num_handles,
                               xrt_shmem_handle_t *out_handles,
                               uint32_t *out_num_handles)
{
	assert(max_num_handles >= 1);

	out_handles[0] = ics->server->ism_handle;
	*out_num_handles = 1;
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_compositor_get_info(volatile struct ipc_client_state *ics,
                                      struct xrt_system_compositor_info *out_info)
{
	*out_info = ics->server->xsysc->info;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_session_create(volatile struct ipc_client_state *ics, const struct xrt_session_info *xsi)
{
	ics->client_state.session_active = false;
	ics->client_state.session_overlay = false;
	ics->client_state.session_visible = false;

	if (xsi->is_overlay) {
		ics->client_state.session_overlay = true;
		ics->client_state.z_order = xsi->z_order;
	}

	update_server_state(ics->server);
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_session_begin(volatile struct ipc_client_state *ics)
{
	// ics->client_state.session_active = true;
	// update_server_state(ics->server);
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_session_end(volatile struct ipc_client_state *ics)
{
	ics->client_state.session_active = false;
	update_server_state(ics->server);
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_get_info(volatile struct ipc_client_state *ics, struct xrt_compositor_info *out_info)
{
	*out_info = ics->xc->info;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_wait_frame(volatile struct ipc_client_state *ics,
                                 int64_t *out_frame_id,
                                 uint64_t *predicted_display_time,
                                 uint64_t *wake_up_time,
                                 uint64_t *predicted_display_period,
                                 uint64_t *min_display_period)
{
	os_mutex_lock(&ics->server->global_state_lock);

	u_rt_helper_predict((struct u_rt_helper *)&ics->urth, out_frame_id, predicted_display_time, wake_up_time,
	                    predicted_display_period, min_display_period);

	os_mutex_unlock(&ics->server->global_state_lock);

	ics->client_state.session_active = true;
	update_server_state(ics->server);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_wait_woke(volatile struct ipc_client_state *ics, int64_t frame_id)
{
	os_mutex_lock(&ics->server->global_state_lock);

	u_rt_helper_mark_wait_woke((struct u_rt_helper *)&ics->urth, frame_id);

	os_mutex_unlock(&ics->server->global_state_lock);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_begin_frame(volatile struct ipc_client_state *ics, int64_t frame_id)
{
	os_mutex_lock(&ics->server->global_state_lock);

	u_rt_helper_mark_begin((struct u_rt_helper *)&ics->urth, frame_id);

	os_mutex_unlock(&ics->server->global_state_lock);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_discard_frame(volatile struct ipc_client_state *ics, int64_t frame_id)
{
	os_mutex_lock(&ics->server->global_state_lock);

	u_rt_helper_mark_discarded((struct u_rt_helper *)&ics->urth, frame_id);

	os_mutex_unlock(&ics->server->global_state_lock);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_layer_sync(volatile struct ipc_client_state *ics,
                                 int64_t frame_id,
                                 uint32_t slot_id,
                                 uint32_t *out_free_slot_id,
                                 const xrt_graphics_sync_handle_t *handles,
                                 const uint32_t num_handles)
{
	struct ipc_shared_memory *ism = ics->server->ism;
	struct ipc_layer_slot *slot = &ism->slots[slot_id];

	for (uint32_t i = 0; i < num_handles; i++) {
		if (!xrt_graphics_sync_handle_is_valid(handles[i])) {
			continue;
		}
#ifdef XRT_GRAPHICS_SYNC_HANDLE_IS_FD
		close(handles[i]);
#else
#error "Need port to transport these graphics buffers"
#endif
	}

	// Copy current slot data to our state.
	ics->render_state = *slot;
	ics->rendering_state = true;

	os_mutex_lock(&ics->server->global_state_lock);

	*out_free_slot_id = (ics->server->current_slot_index + 1) % IPC_MAX_SLOTS;
	ics->server->current_slot_index = *out_free_slot_id;

	// Also protected by the global lock.
	u_rt_helper_mark_delivered((struct u_rt_helper *)&ics->urth, frame_id);

	os_mutex_unlock(&ics->server->global_state_lock);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_compositor_poll_events(volatile struct ipc_client_state *ics, union xrt_compositor_event *out_xce)
{
	uint64_t l_timestamp = UINT64_MAX;
	volatile struct ipc_queued_event *event_to_send = NULL;
	for (uint32_t i = 0; i < IPC_EVENT_QUEUE_SIZE; i++) {
		volatile struct ipc_queued_event *e = &ics->queued_events[i];
		if (e->pending == true && e->timestamp < l_timestamp) {
			event_to_send = e;
		}
	}

	// We always return an event in response to this call -
	// We signal no events with a special event type.
	out_xce->type = XRT_COMPOSITOR_EVENT_NONE;

	if (event_to_send) {
		*out_xce = event_to_send->event;
		event_to_send->pending = false;
	}

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_get_client_info(volatile struct ipc_client_state *_ics,
                                  uint32_t id,
                                  struct ipc_app_state *out_client_desc)
{
	if (id >= IPC_MAX_CLIENTS) {
		return XRT_ERROR_IPC_FAILURE;
	}
	volatile struct ipc_client_state *ics = &_ics->server->threads[id].ics;

	if (ics->imc.socket_fd <= 0) {
		return XRT_ERROR_IPC_FAILURE;
	}

	*out_client_desc = ics->client_state;
	out_client_desc->io_active = ics->io_active;

	//@todo: track this data in the ipc_client_state struct
	out_client_desc->primary_application = false;
	if (ics->server->active_client_index == (int)id) {
		out_client_desc->primary_application = true;
	}

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_set_client_info(volatile struct ipc_client_state *ics, struct ipc_app_state *client_desc)
{
	ics->client_state.info = client_desc->info;
	ics->client_state.pid = client_desc->pid;

	IPC_INFO(ics->server,
	         "Client info\n"
	         "\tapplication_name: '%s'\n"
	         "\tpid: %i",
	         client_desc->info.application_name, //
	         client_desc->pid);                  //

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_get_clients(volatile struct ipc_client_state *_ics, struct ipc_client_list *list)
{
	for (uint32_t i = 0; i < IPC_MAX_CLIENTS; i++) {
		list->ids[i] = _ics->server->threads[i].ics.server_thread_index;
	}
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_set_primary_client(volatile struct ipc_client_state *ics, uint32_t client_id)
{

	ics->server->active_client_index = client_id;
	IPC_INFO(ics->server, "System setting active client to %d.", client_id);
	update_server_state(ics->server);
	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_set_focused_client(volatile struct ipc_client_state *ics, uint32_t client_id)
{
	IPC_INFO(ics->server, "UNIMPLEMENTED: system setting focused client to %d.", client_id);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_toggle_io_client(volatile struct ipc_client_state *_ics, uint32_t client_id)
{
	volatile struct ipc_client_state *ics = NULL;

	if (client_id >= IPC_MAX_CLIENTS) {
		return XRT_ERROR_IPC_FAILURE;
	}

	ics = &_ics->server->threads[client_id].ics;

	if (ics->imc.socket_fd <= 0) {
		return XRT_ERROR_IPC_FAILURE;
	}

	ics->io_active = !ics->io_active;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_system_toggle_io_device(volatile struct ipc_client_state *ics, uint32_t device_id)
{
	if (device_id >= IPC_MAX_DEVICES) {
		return XRT_ERROR_IPC_FAILURE;
	}

	struct ipc_device *idev = &ics->server->idevs[device_id];

	idev->io_active = !idev->io_active;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_create(volatile struct ipc_client_state *ics,
                            const struct xrt_swapchain_create_info *info,
                            uint32_t *out_id,
                            uint32_t *out_num_images,
                            uint64_t *out_size,
                            uint32_t max_num_handles,
                            xrt_graphics_buffer_handle_t *out_handles,
                            uint32_t *out_num_handles)
{
	xrt_result_t xret = XRT_SUCCESS;
	uint32_t index = 0;

	xret = validate_swapchain_state(ics, &index);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	// Create the swapchain
	struct xrt_swapchain *xsc = NULL;
	xret = xrt_comp_create_swapchain(ics->xc, info, &xsc);
	if (xret != XRT_SUCCESS) {
		IPC_ERROR(ics->server, "Error xrt_comp_create_swapchain failed!");
		return xret;
	}

	// It's now safe to increment the number of swapchains.
	ics->num_swapchains++;

	IPC_TRACE(ics->server, "Created swapchain %d.", index);

	set_swapchain_info(ics, index, info, xsc);

	// return our result to the caller.
	struct xrt_swapchain_native *xscn = (struct xrt_swapchain_native *)xsc;

	// Sanity checking.
	assert(xsc->num_images <= IPC_MAX_SWAPCHAIN_HANDLES);
	assert(xsc->num_images <= max_num_handles);

	*out_id = index;
	*out_size = xscn->images[0].size;
	*out_num_images = xsc->num_images;

	// Setup the fds.
	*out_num_handles = xsc->num_images;
	for (size_t i = 0; i < xsc->num_images; i++) {
		out_handles[i] = xscn->images[i].handle;
	}

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_import(volatile struct ipc_client_state *ics,
                            const struct xrt_swapchain_create_info *info,
                            struct ipc_arg_swapchain_from_native *args,
                            uint32_t *out_id,
                            const xrt_graphics_buffer_handle_t *handles,
                            uint32_t num_handles)
{
	xrt_result_t xret = XRT_SUCCESS;
	uint32_t index = 0;

	xret = validate_swapchain_state(ics, &index);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	struct xrt_image_native xins[IPC_MAX_SWAPCHAIN_HANDLES] = {0};
	for (uint32_t i = 0; i < num_handles; i++) {
		xins[i].handle = handles[i];
		xins[i].size = args->sizes[i];
	}

	// create the swapchain
	struct xrt_swapchain *xsc = NULL;
	xret = xrt_comp_import_swapchain(ics->xc, info, xins, num_handles, &xsc);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	// It's now safe to increment the number of swapchains.
	ics->num_swapchains++;

	IPC_TRACE(ics->server, "Created swapchain %d.", index);

	set_swapchain_info(ics, index, info, xsc);
	*out_id = index;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_wait_image(volatile struct ipc_client_state *ics, uint32_t id, uint64_t timeout, uint32_t index)
{
	//! @todo Look up the index.
	uint32_t sc_index = id;
	struct xrt_swapchain *xsc = ics->xscs[sc_index];

	xrt_swapchain_wait_image(xsc, timeout, index);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_acquire_image(volatile struct ipc_client_state *ics, uint32_t id, uint32_t *out_index)
{
	//! @todo Look up the index.
	uint32_t sc_index = id;
	struct xrt_swapchain *xsc = ics->xscs[sc_index];

	xrt_swapchain_acquire_image(xsc, out_index);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_release_image(volatile struct ipc_client_state *ics, uint32_t id, uint32_t index)
{
	//! @todo Look up the index.
	uint32_t sc_index = id;
	struct xrt_swapchain *xsc = ics->xscs[sc_index];

	xrt_swapchain_release_image(xsc, index);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_swapchain_destroy(volatile struct ipc_client_state *ics, uint32_t id)
{
	//! @todo Implement destroy swapchain.
	ics->num_swapchains--;

	xrt_swapchain_destroy((struct xrt_swapchain **)&ics->xscs[id]);
	ics->swapchain_data[id].active = false;

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_device_update_input(volatile struct ipc_client_state *ics, uint32_t id)
{
	// To make the code a bit more readable.
	uint32_t device_id = id;
	struct ipc_shared_memory *ism = ics->server->ism;
	struct ipc_device *idev = get_idev(ics, device_id);
	struct xrt_device *xdev = idev->xdev;
	struct ipc_shared_device *isdev = &ism->isdevs[device_id];

	// Update inputs.
	xrt_device_update_inputs(xdev);

	// Copy data into the shared memory.
	struct xrt_input *src = xdev->inputs;
	struct xrt_input *dst = &ism->inputs[isdev->first_input_index];
	size_t size = sizeof(struct xrt_input) * isdev->num_inputs;

	bool io_active = ics->io_active && idev->io_active;
	if (io_active) {
		memcpy(dst, src, size);
	} else {
		memset(dst, 0, size);

		for (uint32_t i = 0; i < isdev->num_inputs; i++) {
			dst[i].name = src[i].name;

			// Special case the rotation of the head.
			if (dst[i].name == XRT_INPUT_GENERIC_HEAD_POSE) {
				dst[i].active = src[i].active;
			}
		}
	}

	// Reply.
	return XRT_SUCCESS;
}

static struct xrt_input *
find_input(volatile struct ipc_client_state *ics, uint32_t device_id, enum xrt_input_name name)
{
	struct ipc_shared_memory *ism = ics->server->ism;
	struct ipc_shared_device *isdev = &ism->isdevs[device_id];
	struct xrt_input *io = &ism->inputs[isdev->first_input_index];

	for (uint32_t i = 0; i < isdev->num_inputs; i++) {
		if (io[i].name == name) {
			return &io[i];
		}
	}

	return NULL;
}

xrt_result_t
ipc_handle_device_get_tracked_pose(volatile struct ipc_client_state *ics,
                                   uint32_t id,
                                   enum xrt_input_name name,
                                   uint64_t at_timestamp,
                                   struct xrt_space_relation *out_relation)
{

	// To make the code a bit more readable.
	uint32_t device_id = id;
	struct ipc_device *isdev = &ics->server->idevs[device_id];
	struct xrt_device *xdev = isdev->xdev;

	// Find the input
	struct xrt_input *input = find_input(ics, device_id, name);
	if (input == NULL) {
		return XRT_ERROR_IPC_FAILURE;
	}

	// Special case the headpose.
	bool disabled = (!isdev->io_active || !ics->io_active) && name != XRT_INPUT_GENERIC_HEAD_POSE;
	bool active_on_client = input->active;

	// We have been disabled but the client hasn't called update.
	if (disabled && active_on_client) {
		U_ZERO(out_relation);
		return XRT_SUCCESS;
	}

	if (disabled || !active_on_client) {
		return XRT_ERROR_POSE_NOT_ACTIVE;
	}

	// Get the pose.
	xrt_device_get_tracked_pose(xdev, name, at_timestamp, out_relation);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_device_get_hand_tracking(volatile struct ipc_client_state *ics,
                                    uint32_t id,
                                    enum xrt_input_name name,
                                    uint64_t at_timestamp,
                                    struct xrt_hand_joint_set *out_value)
{

	// To make the code a bit more readable.
	uint32_t device_id = id;
	struct xrt_device *xdev = get_xdev(ics, device_id);

	// Get the pose.
	xrt_device_get_hand_tracking(xdev, name, at_timestamp, out_value);

	return XRT_SUCCESS;
}
xrt_result_t
ipc_handle_device_get_view_pose(volatile struct ipc_client_state *ics,
                                uint32_t id,
                                struct xrt_vec3 *eye_relation,
                                uint32_t view_index,
                                struct xrt_pose *out_pose)
{
	// To make the code a bit more readable.
	uint32_t device_id = id;
	struct xrt_device *xdev = get_xdev(ics, device_id);

	// Get the pose.
	xrt_device_get_view_pose(xdev, eye_relation, view_index, out_pose);

	return XRT_SUCCESS;
}

xrt_result_t
ipc_handle_device_set_output(volatile struct ipc_client_state *ics,
                             uint32_t id,
                             enum xrt_output_name name,
                             union xrt_output_value *value)
{
	// To make the code a bit more readable.
	uint32_t device_id = id;
	struct xrt_device *xdev = get_xdev(ics, device_id);

	// Set the output.
	xrt_device_set_output(xdev, name, value);

	return XRT_SUCCESS;
}
