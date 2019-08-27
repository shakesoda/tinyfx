#include <sys/signal.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL.h>
#include <assert.h>

#include "demo_util.h"
#include "01-triangle.c"
#include "02-sky.c"

struct demo_def {
	void (*init)(uint16_t, uint16_t);
	void (*frame)(int, int);
	void (*cleanup)();
};

static const int num_demos = 2;
static struct demo_def g_demos[] = {
	{ &triangle_init, &triangle_frame, &triangle_deinit },
	{ &sky_init, &sky_frame, &sky_deinit },
};

static int g_current_demo = 0;
static int g_queue_demo = 0;

static uint16_t g_width = 0;
static uint16_t g_height = 0;
static SDL_Window     *g_window;
static SDL_GLContext  g_context;

static int g_alive = 1;

static int g_mx = 0;
static int g_my = 0;

static int demo_poll_events() {
	g_mx = 0;
	g_my = 0;
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_WINDOWEVENT
			&& event.window.event == SDL_WINDOWEVENT_CLOSE
		) {
			return 0;
		}
		switch (event.type) {
			case SDL_KEYDOWN: {
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					return 0;
				}
				if (event.key.keysym.sym == SDLK_LEFT) {
					g_queue_demo -= 1;
					break;
				}
				if (event.key.keysym.sym == SDLK_RIGHT) {
					g_queue_demo += 1;
					break;
				}
				break;
			}
			case SDL_MOUSEMOTION: {
				g_mx += event.motion.xrel;
				g_my += event.motion.yrel;
				break;
			}
			default: break;
		}
	}
	return 1;
}

void demo_end_frame() {
	SDL_GL_SwapWindow(g_window);
	g_alive = demo_poll_events();
}

void sigh(int signo) {
	if (signo == SIGINT) {
		g_alive = 0;
		puts("");
	}

	// Make *SURE* that SDL gives input back to the OS.
	if (signo == SIGSEGV) {
		SDL_Quit();
	}
}

typedef void (APIENTRY *DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam);
typedef void (APIENTRY *DEBUGMSG)(DEBUGPROC callback, const void* userParam);

DEBUGMSG glDebugMessageCallback = 0;

static void debug_spew(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei len, const GLchar *msg, void *p) {
	if (sev == GL_DEBUG_SEVERITY_NOTIFICATION || sev == GL_DEBUG_SEVERITY_LOW) {
		return;
	}
	printf("GL DEBUG: %s\n", msg);
}

int main(int argc, char **argv) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	g_window = SDL_CreateWindow(
		"",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 480,
		SDL_WINDOW_OPENGL
	);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,    5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,  6);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,  0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

	// So that desktops behave consistently with RPi
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	g_context = SDL_GL_CreateContext(g_window);
	SDL_GL_GetDrawableSize(
		g_window,
		(int*)&g_width,
		(int*)&g_height
	);
	SDL_GL_MakeCurrent(g_window, g_context);

	glDebugMessageCallback = (DEBUGMSG)SDL_GL_GetProcAddress("glDebugMessageCallback");
	if (glDebugMessageCallback) {
		puts("GL debug enabled");
		glDebugMessageCallback(&debug_spew, NULL);
	}

	printf("Display resolution: %dx%d\n", g_width, g_height);

	SDL_SetRelativeMouseMode(1);
	SDL_GL_SetSwapInterval(1);

	assert(signal(SIGINT, sigh) != SIG_ERR);

	tfx_platform_data pd;
	pd.use_gles = true;
	pd.context_version = 20;
	pd.gl_get_proc_address = SDL_GL_GetProcAddress;
	tfx_set_platform_data(pd);
	tfx_reset_flags flags = TFX_RESET_NONE
		| TFX_RESET_DEBUG_OVERLAY | TFX_RESET_DEBUG_OVERLAY_STATS
		| TFX_RESET_REPORT_GPU_TIMINGS
	;
	tfx_reset(g_width, g_height, flags);

	if (g_demos[g_current_demo].init) {
		g_demos[g_current_demo].init(g_width, g_height);
	}

	double then = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
	double last_report = then;
	while (g_alive) {
		double now = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
		double delta = now - then;
		then = now;

#if 0
		if (now - last_report > 1.0) {
			printf("%0.2fms %0.1ffps\n", (float)(delta * 1000.0), (float)(1.0 / delta));
			last_report = now;
		}
#endif

		// on demo change, cleanup old and init new
		if (g_current_demo != g_queue_demo) {
			while (g_queue_demo < 0) {
				g_queue_demo += num_demos;
			}

			if (g_demos[g_current_demo].cleanup) {
				g_demos[g_current_demo].cleanup();
			}

			g_queue_demo %= num_demos;
			g_current_demo = g_queue_demo;

			if (g_demos[g_current_demo].init) {
				tfx_reset(g_width, g_height, flags);
				g_demos[g_current_demo].init(g_width, g_height);
			}
		}

		if (g_demos[g_current_demo].frame) {
			g_demos[g_current_demo].frame(g_mx, g_my);
		}

		demo_end_frame();
	}

	if (g_demos[g_current_demo].cleanup) {
		g_demos[g_current_demo].cleanup();
	}

	tfx_shutdown();

	SDL_Quit();

	return 0;
}
