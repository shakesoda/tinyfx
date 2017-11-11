#pragma once

#include <SDL2/SDL.h>
#include <sys/signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
	SDL_Window     *window;
	SDL_GLContext  context;
	int dead;
} state_t;

static state_t *g_state = NULL;

void demo_swap_buffers() {
	SDL_GL_SwapWindow(g_state->window);
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

int demo_run(void (*runcb)(state_t*)) {
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

	printf("Display resolution: %dx%d\n", state.width, state.height);

	SDL_SetRelativeMouseMode(1);
	SDL_GL_SetSwapInterval(1);

	assert(signal(SIGINT, sigh) != SIG_ERR);

	runcb(&state);

	SDL_Quit();

	return 0;
}
