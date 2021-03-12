// Copyright 2019, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Hacky SDL integration
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */

#include "xrt/xrt_instance.h"
#include "xrt/xrt_config_have.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_debug.h"

#include "os/os_threading.h"


struct xrt_instance;

#ifndef XRT_HAVE_SDL2

int
oxr_sdl2_hack_create(void **out_hack)
{
	return 0;
}

void
oxr_sdl2_hack_start(void *hack, struct xrt_instance *xinst, struct xrt_device **xdevs)
{}

void
oxr_sdl2_hack_stop(void **hack)
{}

#else

#include "ogl/ogl_api.h"

#include "gui/gui_common.h"
#include "gui/gui_imgui.h"

#include "qwerty_interface.h"

#include <SDL2/SDL.h>

DEBUG_GET_ONCE_BOOL_OPTION(gui, "OXR_DEBUG_GUI", false)
DEBUG_GET_ONCE_BOOL_OPTION(qwerty_enable, "QWERTY_ENABLE", false)


/*!
 * Common struct holding state for the GUI interface.
 * @extends gui_program
 */
struct sdl2_program
{
	struct gui_program base;

	SDL_GLContext ctx;
	SDL_Window *win;

	struct os_thread_helper oth;

	bool sdl_initialized;
};

struct gui_imgui
{
	bool show_imgui_demo;
	bool show_implot_demo;
	struct xrt_colour_rgb_f32 clear;
};

static void
sdl2_window_init(struct sdl2_program *p)
{
	const char *title = "Monado! ☺";
	int x = SDL_WINDOWPOS_UNDEFINED;
	int y = SDL_WINDOWPOS_UNDEFINED;
	int w = 1920;
	int h = 1080;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

	int window_flags = 0;
	window_flags |= SDL_WINDOW_SHOWN;
	window_flags |= SDL_WINDOW_OPENGL;
	window_flags |= SDL_WINDOW_RESIZABLE;
	window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#if 0
	window_flags |= SDL_WINDOW_MAXIMIZED;
#endif

	p->win = SDL_CreateWindow(title, x, y, w, h, window_flags);
	if (p->win == NULL) {
		U_LOG_E("Failed to create window!");
		return;
	}

	p->ctx = SDL_GL_CreateContext(p->win);
	if (p->ctx == NULL) {
		U_LOG_E("Failed to create GL context!");
		return;
	}

	SDL_GL_MakeCurrent(p->win, p->ctx);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	// Setup OpenGL bindings.
	bool err = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) == 0;
	if (err) {
		U_LOG_E("Failed to load GL functions!");
		return;
	}

	// To manage the scenes.
	gui_scene_manager_init(&p->base);

	// Start the scene.
	gui_scene_debug(&p->base);
}

static void
sdl2_loop(struct sdl2_program *p)
{
	// Need to call this before any other Imgui call.
	igCreateContext(NULL);

	// Local state
	ImGuiIO *io = igGetIO();

	// Setup Platform/Renderer bindings
	igImGui_ImplSDL2_InitForOpenGL(p->win, p->ctx);
	igImGui_ImplOpenGL3_Init(NULL);

	// Setup Dear ImGui style
	igStyleColorsDark(NULL);

	// Setup the plot context.
	ImPlotContext *plot_ctx = ImPlot_CreateContext();
	ImPlot_SetCurrentContext(plot_ctx);

	// Setup qwerty driver usage
	bool qwerty_enabled = debug_get_bool_option_qwerty_enable();

	// Main loop
	struct gui_imgui gui = {0};
	gui.clear.r = 0.45f;
	gui.clear.g = 0.55f;
	gui.clear.b = 0.60f;
	u_var_add_root(&gui, "GUI Control", false);
	u_var_add_rgb_f32(&gui, &gui.clear, "Clear Colour");
	u_var_add_bool(&gui, &gui.show_imgui_demo, "Imgui Demo Window");
	u_var_add_bool(&gui, &gui.show_implot_demo, "Implot Demo Window");
	u_var_add_bool(&gui, &p->base.stopped, "Exit");

	while (!p->base.stopped) {
		SDL_Event event;

		while (SDL_PollEvent(&event)) {
			igImGui_ImplSDL2_ProcessEvent(&event);

			// Caution here, qwerty driver is being accessed by the main thread as well
			if (qwerty_enabled) {
				qwerty_process_event(p->base.xdevs, NUM_XDEVS, event);
			}

			if (event.type == SDL_QUIT) {
				p->base.stopped = true;
			}

			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
			    event.window.windowID == SDL_GetWindowID(p->win)) {
				p->base.stopped = true;
			}
		}

		// Start the Dear ImGui frame
		igImGui_ImplOpenGL3_NewFrame();
		igImGui_ImplSDL2_NewFrame(p->win);

		// Start new frame.
		igNewFrame();

		// Render the scene into it.
		gui_scene_manager_render(&p->base);

		// Handle this here.
		if (gui.show_imgui_demo) {
			igShowDemoWindow(&gui.show_imgui_demo);
		}

		// Handle this here.
		if (gui.show_implot_demo) {
			ImPlot_ShowDemoWindow(&gui.show_implot_demo);
		}

		// Build the DrawData (EndFrame).
		igRender();

		// Clear the background.
		glViewport(0, 0, (int)io->DisplaySize.x, (int)io->DisplaySize.y);
		glClearColor(gui.clear.r, gui.clear.g, gui.clear.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		igImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());

		SDL_GL_SwapWindow(p->win);

		gui_prober_update(&p->base);
	}

	// Cleanup
	u_var_remove_root(&gui);
	ImPlot_DestroyContext(plot_ctx);
	igImGui_ImplOpenGL3_Shutdown();
	igImGui_ImplSDL2_Shutdown();
	igDestroyContext(NULL);
}

static void
sdl2_close(struct sdl2_program *p)
{
	// All scenes should be destroyed by now.
	gui_scene_manager_destroy(&p->base);

	if (p->ctx != NULL) {
		SDL_GL_DeleteContext(p->ctx);
		p->ctx = NULL;
	}

	if (p->win != NULL) {
		SDL_DestroyWindow(p->win);
		p->win = NULL;
	}

	if (p->sdl_initialized) {
		//! @todo: Properly quit SDL without crashing SDL client apps
		// SDL_Quit();
		p->sdl_initialized = false;
	}
}

static void *
oxr_sdl2_hack_run_thread(void *ptr)
{
	struct sdl2_program *p = (struct sdl2_program *)ptr;

	sdl2_window_init(p);

	sdl2_loop(p);

	sdl2_close(p);

	return NULL;
}

int
oxr_sdl2_hack_create(void **out_hack)
{
	// Enabled?
	if (!debug_get_bool_option_gui()) {
		return 0;
	}

	// Need to do this as early as possible.
	u_var_force_on();

	struct sdl2_program *p = U_TYPED_CALLOC(struct sdl2_program);
	if (p == NULL) {
		return -1;
	}

	*out_hack = p;

	return 0;
}

void
oxr_sdl2_hack_start(void *hack, struct xrt_instance *xinst, struct xrt_device **xdevs)
{
	struct sdl2_program *p = (struct sdl2_program *)hack;
	if (p == NULL) {
		return;
	}

	xrt_instance_get_prober(xinst, &p->base.xp);

	for (size_t i = 0; i < NUM_XDEVS; i++) {
		p->base.xdevs[i] = xdevs[i];
	}

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		U_LOG_E("Failed to init SDL2!");
		return;
	}
	p->sdl_initialized = true;

	(void)os_thread_helper_start(&p->oth, oxr_sdl2_hack_run_thread, p);
}

void
oxr_sdl2_hack_stop(void **hack_ptr)
{
	struct sdl2_program *p = *(struct sdl2_program **)hack_ptr;
	if (p == NULL) {
		return;
	}

	// HACK!
	p->base.stopped = true;

	// Destroy the thread object.
	os_thread_helper_destroy(&p->oth);

	free(p);
	*hack_ptr = NULL;
}
#endif
