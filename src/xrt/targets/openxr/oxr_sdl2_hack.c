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

struct cJSON;
typedef struct cJSON cJSON;
struct xrt_prober_device;

#include "xrt/xrt_device.h"
#include "qwerty_interface.h"

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

#include <SDL2/SDL.h>

DEBUG_GET_ONCE_BOOL_OPTION(gui, "OXR_DEBUG_GUI", false)


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
qwerty_process_inputs(struct xrt_device **xdevs, SDL_Event event)
{
	// XXX: Think about a better way of obtaining qwerty_devices from xdevs than
	// hardcoding xdevs[] indices. Maybe adding a QWERTY xrt_device_name?
	struct xrt_device *qhmd = xdevs[0];
	struct xrt_device *qleft = xdevs[1]; // XXX: Why q prefix? This should be xleft
	struct xrt_device *qright = xdevs[2];

	bool using_qhmd = qwerty_hmd_available(qleft);

	// clang-format off
	// XXX: I'm definitely pushing some limits with so much clang-format off
	// it is mainly for the oneline ifs that I think are more readable

	// XXX: This static vars seem a bad idea, maybe should use
	// SDL_GetKeyboardState but I quickly read that is only for SDL2 and I
	// think monado should support SDL1 as well. Also this `*_pressed = true`
	// logic is repeated in qwerty_device, smells bad.
	static bool alt_pressed = false;
	static bool ctrl_pressed = false;

	bool alt_down = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LALT;
	bool alt_up = event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LALT;
	bool ctrl_down = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LCTRL;
	bool ctrl_up = event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LCTRL;
	if (alt_down) alt_pressed = true;
	if (alt_up) alt_pressed = false;
	if (ctrl_down) ctrl_pressed = true;
	if (ctrl_up) ctrl_pressed = false;

	bool change_focus = alt_down || alt_up || ctrl_down || ctrl_up;
	if (change_focus) {
		if (using_qhmd) qwerty_release_all(qhmd);
		qwerty_release_all(qright);
		qwerty_release_all(qleft);
	}

	struct xrt_device *qdev; // Focused device
	if (ctrl_pressed) qdev = qleft;
	else if (alt_pressed) qdev = qright;
	else if (using_qhmd) qdev = qhmd;
	else /* if (!using_qhmd) */ qdev = qright;

	// Default controller for methods that only make sense for controllers.
	// Right one because some window managers capture alt+click actions before SDL
	struct xrt_device *qctrl = qdev != qhmd ? qdev : qright;

	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a) qwerty_press_left(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_a) qwerty_release_left(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_d) qwerty_press_right(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_d) qwerty_release_right(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_w) qwerty_press_forward(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_w) qwerty_release_forward(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s) qwerty_press_backward(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_s) qwerty_release_backward(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_e) qwerty_press_up(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_e) qwerty_release_up(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q) qwerty_press_down(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_q) qwerty_release_down(qdev);

	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT) qwerty_press_look_left(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LEFT) qwerty_release_look_left(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) qwerty_press_look_right(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RIGHT) qwerty_release_look_right(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP) qwerty_press_look_up(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_UP) qwerty_release_look_up(qdev);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) qwerty_press_look_down(qdev);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_DOWN) qwerty_release_look_down(qdev);

	// Select and menu clicks only for controllers.
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) qwerty_select_click(qctrl);
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_MIDDLE) qwerty_menu_click(qctrl);

	// XXX: The behaviour of the F key is not consistent to that of the R key in that touching F without ctrl or alt does not affect both controllers.
	// XXX: Look for other inconsistencies in the shortcuts usage and improve it
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f) qwerty_toggle_follow_hmd(qctrl);
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE && event.key.repeat == 0) qwerty_toggle_follow_hmd(qctrl);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE) qwerty_toggle_follow_hmd(qctrl);

	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
		if (ctrl_pressed) qwerty_reset_controller_pose(qleft);
		else if (alt_pressed) qwerty_reset_controller_pose(qright);
		else {
			qwerty_reset_controller_pose(qleft);
			qwerty_reset_controller_pose(qright);
		}
	}
	if (event.type == SDL_MOUSEWHEEL) qwerty_change_movement_speed(qdev, event.wheel.y);

	float sprint_steps = 5;
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, sprint_steps);
	if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LSHIFT) qwerty_change_movement_speed(qdev, -sprint_steps);
	// clang-format on

	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
		SDL_SetRelativeMouseMode(false);
	if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON_RMASK) {
		SDL_SetRelativeMouseMode(true);
		float sensitivity = 0.1f; // 1px moves `sensitivity` look_speed units
		float yaw = -event.motion.xrel * sensitivity;
		float pitch = -event.motion.yrel * sensitivity;
		qwerty_add_look_delta(qdev, yaw, pitch);
	}
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

			// XXX: Get using_qwerty condition from somewhere else.
			// Maybe add a new xrt_device_name and scan xdevs? An environment var?
			// This is how I was doing it before enabling non-qwerty hmds to work:
			// 	struct xrt_device *hmd = p->base.xdevs[0];
			// 	bool using_qwerty = hmd != NULL && !strcmp(hmd->serial, "Qwerty HMD");
			bool using_qwerty = true;
			if (using_qwerty)
				qwerty_process_inputs(p->base.xdevs, event);

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

	// XXXANS: for some reason p->base.xdevs is empty on monado-service, understand why
	// ANS: gui_program was probably intended to be used only on monado-gui (hence the _hack prefix)

	// XXXASK: Having more than one owner for these pointers seems like a bad idea
	// XXXASK: Also, these pointers will be touched from the gui thread without a lock
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
