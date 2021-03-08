// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Common file for the Monado GUI program.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup gui
 */

#pragma once

#include "xrt/xrt_compiler.h"


/*!
 * @defgroup gui GUI Config Interface
 * @ingroup xrt
 *
 * @brief Small GUI interface to configure Monado based on SDL2.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_XDEVS 8

struct xrt_device;
struct xrt_prober;
struct xrt_fs;
struct xrt_frame_sink;
struct xrt_frame_context;
struct xrt_settings_tracking;
struct time_state;
struct gui_scene_manager;


/*!
 * A gui program.
 *
 * @ingroup gui
 */
struct gui_program
{
	bool stopped;

	struct gui_scene_manager *gsm;

	struct xrt_device *xdevs[NUM_XDEVS];
	struct xrt_instance *instance;
	struct xrt_prober *xp;

	struct gui_ogl_texture *texs[256];
	size_t num_texs;
};

/*!
 * @interface gui_scene
 * A single currently running scene.
 *
 * @ingroup gui
 */
struct gui_scene
{
	void (*render)(struct gui_scene *, struct gui_program *);
	void (*destroy)(struct gui_scene *, struct gui_program *);
};

/*!
 * A OpenGL texture.
 *
 * @ingroup gui
 */
struct gui_ogl_texture
{
	uint64_t seq;
	uint64_t dropped;
	const char *name;
	uint32_t w, h;
	uint32_t id;
	bool half;

	void *ptr;
};

/*!
 * Initialize the prober and open all devices found.
 *
 * @ingroup gui
 */
int
gui_prober_init(struct gui_program *p);

/*!
 * Create devices.
 *
 * @ingroup gui
 */
int
gui_prober_select(struct gui_program *p);

/*!
 * Update all devices.
 *
 * @ingroup gui
 */
void
gui_prober_update(struct gui_program *p);

/*!
 * Destroy all opened devices and destroy the prober.
 *
 * @ingroup gui
 */
void
gui_prober_teardown(struct gui_program *p);

/*!
 * Create a sink that will turn frames into OpenGL textures, since the frame
 * can come from another thread @ref gui_ogl_sink_update needs to be called.
 *
 * Destruction is handled by the frame context.
 *
 * @ingroup gui
 */
struct gui_ogl_texture *
gui_ogl_sink_create(const char *name, struct xrt_frame_context *xfctx, struct xrt_frame_sink **out_sink);

/*!
 * Update the texture to the latest received frame.
 *
 * @ingroup gui
 */
void
gui_ogl_sink_update(struct gui_ogl_texture *);

/*!
 * Push the scene to the top of the lists.
 *
 * @ingroup gui
 */
void
gui_scene_push_front(struct gui_program *p, struct gui_scene *me);

/*!
 * Put a scene on the delete list, also removes it from any other list.
 *
 * @ingroup gui
 */
void
gui_scene_delete_me(struct gui_program *p, struct gui_scene *me);

/*!
 * Render the scenes.
 *
 * @ingroup gui
 */
void
gui_scene_manager_render(struct gui_program *p);

/*!
 * Initialize the scene manager.
 *
 * @ingroup gui
 */
void
gui_scene_manager_init(struct gui_program *p);

/*!
 * Destroy the scene manager.
 *
 * @ingroup gui
 */
void
gui_scene_manager_destroy(struct gui_program *p);


/*
 *
 * Scene creation functions.
 *
 */

/*!
 * Shows the main menu.
 *
 * @ingroup gui
 */
void
gui_scene_main_menu(struct gui_program *p);

/*!
 * Shows a UI that lets you select a video device and mode for calibration.
 *
 * @ingroup gui
 */
void
gui_scene_select_video_calibrate(struct gui_program *p);

/*!
 * Shows a UI that lets you set up tracking overrides.
 *
 * @ingroup gui
 */
void
gui_scene_tracking_overrides(struct gui_program *p);

/*!
 * Regular debug UI.
 *
 * @ingroup gui
 */
void
gui_scene_debug(struct gui_program *p);

/*!
 * Remote control debugging UI.
 *
 * @ingroup gui
 */
void
gui_scene_remote(struct gui_program *p);

/*!
 * Given the frameserver runs the calibration code on it.
 * Claims ownership of @p s.
 *
 * @ingroup gui
 */
void
gui_scene_calibrate(struct gui_program *p,
                    struct xrt_frame_context *xfctx,
                    struct xrt_fs *xfs,
                    struct xrt_settings_tracking *s);


#ifdef __cplusplus
}
#endif
