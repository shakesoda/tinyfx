#include "tinyfx.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <sys/signal.h>
#include <stdlib.h>

#define INTERNAL_RES 240

void *read_file(const char *filename) {
	FILE *f = fopen(filename, "r");
	if (f) {
		fseek(f, 0L, SEEK_END);
		int bytes = ftell(f);
		rewind(f);
		char *buf = malloc(bytes+1);
		buf[bytes] = '\0';
		fread(buf, 1, bytes, f);
		fclose(f);

		return buf;
	}
	printf("Unable to open file \"%s\"\n", filename);
	return NULL;
}

typedef struct state_t {
	uint16_t width;
	uint16_t height;
	uint16_t internal_width;
	uint16_t internal_height;
	SDL_Window     *window;
	SDL_GLContext  context;
	int dead;
} state_t;

int poll_events(state_t *state);

void run(state_t *state) {
	tfx_reset(state->width, state->height);
	// tfx_dump_caps();

	tfx_canvas fb = tfx_canvas_new(state->internal_width, state->internal_height, TFX_FORMAT_RGB565_D16);

	uint8_t v = 0;
	tfx_view_set_canvas(v, &fb);
	tfx_view_set_clear_color(v, 0xff00ffff);
	tfx_view_set_clear_depth(v, 1.0);

	uint8_t back = 1;
	tfx_view_set_clear_color(back, 0x555555ff);
	tfx_view_set_clear_depth(back, 1.0);
	tfx_view_set_depth_test(back, TFX_DEPTH_TEST_LT);

	char *vss = read_file("shader.vs.glsl");
	char *fss = read_file("shader.fs.glsl");

	if (!vss || !fss) {
		vss = read_file("test/shader.vs.glsl");
		fss = read_file("test/shader.fs.glsl");
	}

	assert(vss);
	assert(fss);

	const char *attribs[] = {
		"a_position",
		"a_color",
		NULL
	};

	tfx_program prog = tfx_program_new(vss, fss, attribs);
	free(vss);
	free(fss);

	float verts[] = {
		 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
	};

	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_add(&fmt, 4, true, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	tfx_buffer vbo = tfx_buffer_new(verts, sizeof(verts), TFX_USAGE_STATIC, &fmt);

	tfx_uniform u = tfx_uniform_new("u_texture", TFX_UNIFORM_INT, 1);
	int uv[] = { 0 };
	tfx_uniform_set_int(&u, uv);

	while (!state->dead) {
		state->dead = poll_events(state);
		// tfx_touch(&v);
		tfx_touch(back);

		// tfx_blit(&v, &back, 0, 0, tfx_view_get_width(&back), tfx_view_get_height(&back));

		tfx_set_vertices(&vbo, 3);
		tfx_set_state(0);
		tfx_submit(back, prog, false);

		// process command buffer
		tfx_frame();
		SDL_GL_SwapWindow(state->window);
	}

	tfx_shutdown();
}

int poll_events(state_t *state) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_WINDOWEVENT
			&& event.window.event == SDL_WINDOWEVENT_CLOSE
		) {
			return 1;
		}
		switch (event.type) {
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					return 1;
				}
			default: break;
		}
	}
	return 0;
}

static state_t *g_state = NULL;

void sigh(int signo) {
	if (signo == SIGINT) {
		g_state->dead = 1;
		puts("");
	}

	// Make *SURE* that SDL gives input back to the OS.
	if (signo == SIGSEGV) {
		SDL_Quit();
	}
}


#include <SDL2/SDL_opengl.h>

typedef void (APIENTRY *DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam);
typedef void (APIENTRY *DEBUGMSG)(DEBUGPROC callback, const void* userParam);

DEBUGMSG glDebugMessageCallback = 0;

static void debug_spew(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei len, const GLchar *msg, void *p) {
	printf("GL DEBUG: %s\n", msg);
}

int main(int argc, char **argv) {
	state_t state;
	memset(&state, 0, sizeof(state_t));

	g_state = &state;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	state.window = SDL_CreateWindow(
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

	state.context = SDL_GL_CreateContext(state.window);
	SDL_GL_GetDrawableSize(
		state.window,
		(int*)&state.width,
		(int*)&state.height
	);
	SDL_GL_MakeCurrent(state.window, state.context);

	glDebugMessageCallback = (DEBUGMSG)SDL_GL_GetProcAddress("glDebugMessageCallback");
	if (glDebugMessageCallback) {
		puts("GL debug enabled");
		glDebugMessageCallback(&debug_spew, NULL);
	}

	float aspect = (float)state.width / state.height;
	state.internal_width  = INTERNAL_RES*aspect;
	state.internal_height = INTERNAL_RES;

	printf("Display resolution: %dx%d\n", state.width, state.height);
	printf("Internal resolution: %dx%d\n", state.internal_width, state.internal_height);

	SDL_SetRelativeMouseMode(1);
	SDL_GL_SetSwapInterval(1);

	assert(signal(SIGINT, sigh) != SIG_ERR);

	run(&state);

	SDL_Quit();

	return 0;
}
