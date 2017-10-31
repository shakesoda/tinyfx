#define TFX_IMPLEMENTATION
// #define TFX_USE_GLES3
#define TFX_DEBUG
#include "tinyfx.h"

/**********************\
| implementation stuff |
\**********************/
#ifdef TFX_IMPLEMENTATION
#include <GLES2/gl2.h>
#include <string.h>
#include <stdio.h>
#ifdef TFX_DEBUG
#include <assert.h>
#else
#define assert(op) (void)(op);
#endif

#ifndef TFX_UNIFORM_BUFFER_SIZE
// by default, allow up to 8MB of uniform updates per frame.
#define TFX_UNIFORM_BUFFER_SIZE 1024*1024*8
#endif

// The following code is public domain, from https://github.com/nothings/stb
//////////////////////////////////////////////////////////////////////////////
#ifndef STB_STRETCHY_BUFFER_H_INCLUDED
#define STB_STRETCHY_BUFFER_H_INCLUDED

#ifndef NO_STRETCHY_BUFFER_SHORT_NAMES
#define sb_free   stb_sb_free
#define sb_push   stb_sb_push
#define sb_count  stb_sb_count
#define sb_add    stb_sb_add
#define sb_last   stb_sb_last
#endif

#define stb_sb_free(a)         ((a) ? free(stb__sbraw(a)),0 : 0)
#define stb_sb_push(a,v)       (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a)        ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a,n)        (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb_sb_last(a)         ((a)[stb__sbn(a)-1])

#define stb__sbraw(a) ((int *) (a) - 2)
#define stb__sbm(a)   stb__sbraw(a)[0]
#define stb__sbn(a)   stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))
#define stb__sbmaybegrow(a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)      ((a) = stb__sbgrowf((a), (n), sizeof(*(a))))

#include <stdlib.h>

static void * stb__sbgrowf(void *arr, int increment, int itemsize)
{
	int dbl_cur = arr ? 2*stb__sbm(arr) : 0;
	int min_needed = stb_sb_count(arr) + increment;
	int m = dbl_cur > min_needed ? dbl_cur : min_needed;
	int *p = (int *) realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int)*2);
	if (p) {
		if (!arr)
			p[1] = 0;
		p[0] = m;
		return p+2;
	} else {
		#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
		STRETCHY_BUFFER_OUT_OF_MEMORY ;
		#endif
		return (void *) (2*sizeof(int)); // try to force a NULL pointer exception later
	}
}
#endif // STB_STRETCHY_BUFFER_H_INCLUDED
//////////////////////////////////////////////////////////////////////////////

#define CHECK(fn) fn; { GLenum _status; while ((_status = glGetError())) { if (_status == GL_NO_ERROR) break; printf("%s:%d GL ERROR: %d\n", __FILE__, __LINE__, _status); } }

#define VIEW_MAX 256

// view flags
enum {
	// clear modes
	TFX_VIEW_CLEAR_COLOR   = 1 << 0,
	TFX_VIEW_CLEAR_DEPTH   = 1 << 1,

	// depth modes
	TFX_VIEW_DEPTH_TEST_LT = 1 << 2,
	TFX_VIEW_DEPTH_TEST_GT = 1 << 3,

	// scissor test
	TFX_VIEW_SCISSOR       = 1 << 4
};

typedef struct tfx_view {
	uint32_t flags;

	tfx_canvas *canvas;
	tfx_draw    *draws;
	tfx_blit_op *blits;

	int   clear_color;
	float clear_depth;

	tfx_rect scissor_rect;
} tfx_view;

#define TFX_VIEW_CLEAR_MASK      (TFX_VIEW_CLEAR_COLOR | TFX_VIEW_CLEAR_DEPTH)
#define TFX_VIEW_DEPTH_TEST_MASK (TFX_VIEW_DEPTH_TEST_LT | TFX_VIEW_DEPTH_TEST_GT)

#define TFX_STATE_CULL_MASK      (TFX_STATE_CULL_CW | TFX_STATE_CULL_CCW)
#define TFX_STATE_BLEND_MASK     (TFX_STATE_BLEND_ALPHA)
#define TFX_STATE_DRAW_MASK      (TFX_STATE_DRAW_POINTS \
	| TFX_STATE_DRAW_LINES | TFX_STATE_DRAW_LINE_STRIP | TFX_STATE_DRAW_LINE_LOOP \
	| TFX_STATE_DRAW_TRI_STRIP | TFX_STATE_DRAW_TRI_FAN \
)

static tfx_canvas backbuffer;

tfx_caps tfx_dump_caps() {
	// I am told by the docs that this can be 0.
	// It's not on the RPi, but since it's only a few lines of code...
	int release_shader_c = 0;
	glGetIntegerv(GL_SHADER_COMPILER, &release_shader_c);
	printf("GL shader compiler control: %d\n", release_shader_c);
	printf("GL vendor: %s\n", glGetString(GL_VENDOR));
	printf("GL version: %s\n", glGetString(GL_VERSION));

	char *exts = (char*)glGetString(GL_EXTENSIONS);
	int len = 0;

	puts("GL extensions:");
	char *pch = strtok(exts, " ");
	while (pch != NULL) {
		printf("\t%s\n", pch);
		pch = strtok(NULL, " ");
		len++;
	}

	puts("TinyFX renderer: "
	#ifdef TFX_USE_GLES3 // NYI
		"GLES3"
	#else
		"GLES2"
	#endif
	);

	tfx_caps caps;
	memset(&caps, 0, sizeof(tfx_caps));
	caps.compute = false;
	caps.float_canvas = false;

	return caps;
}

// uniforms updated this frame
static tfx_uniform *uniforms = NULL;
static uint8_t *uniform_buffer = NULL;
static uint8_t *ub_cursor = NULL;
static tfx_view views[VIEW_MAX];

void tfx_reset(uint16_t width, uint16_t height) {
	memset(&backbuffer, 0, sizeof(tfx_canvas));
	backbuffer.gl_id  = 0;
	backbuffer.width  = width;
	backbuffer.height = height;

	if (!uniform_buffer) {
		uniform_buffer = malloc(TFX_UNIFORM_BUFFER_SIZE);
		memset(uniform_buffer, 0, TFX_UNIFORM_BUFFER_SIZE);
		ub_cursor = uniform_buffer;
	}

	memset(&views, 0, sizeof(tfx_view)*VIEW_MAX);
}

void tfx_shutdown() {
	// TODO: clean up all GL objects, allocs, etc.
	free(uniform_buffer);
	uniform_buffer = NULL;

	// this can happen if you shutdown before calling frame()
	if (uniforms) {
		sb_free(uniforms);
		uniforms = NULL;
	}
}

static bool shaderc_allocated = false;

static GLuint load_shader(GLenum type, const char *shaderSrc) {
	shaderc_allocated = true;

	GLuint shader = CHECK(glCreateShader(type));
	if (!shader) {
		fprintf(stderr, "Something has gone horribly wrong, and we can't make shaders.\n");
		return 0;
	}

	CHECK(glShaderSource(shader, 1, &shaderSrc, NULL));
	CHECK(glCompileShader(shader));

	GLint compiled;
	CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled));
	if (!compiled) {
		GLint infoLen = 0;
		CHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen));
		if (infoLen > 0) {
			char* infoLog = malloc(sizeof(char) * infoLen);
			CHECK(glGetShaderInfoLog(shader, infoLen, NULL, infoLog));
			fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}
		CHECK(glDeleteShader(shader));
		return 0;
	}

	return shader;
}

tfx_program tfx_program_new(const char *vss, const char *fss, const char *attribs[]) {
	GLuint vs = load_shader(GL_VERTEX_SHADER, vss);
	GLuint fs = load_shader(GL_FRAGMENT_SHADER, fss);
	GLuint program = CHECK(glCreateProgram());
	if (!program) {
		return 0;
	}

	CHECK(glAttachShader(program, vs));
	CHECK(glAttachShader(program, fs));

	const char **it = attribs;
	int i = 0;
	while (*it != NULL) {
		CHECK(glBindAttribLocation(program, i, *it));
		i++;
		it++;
	}

	CHECK(glLinkProgram(program));

	GLint linked;
	CHECK(glGetProgramiv(program, GL_LINK_STATUS, &linked));
	if (!linked) {
		GLint infoLen = 0;
		CHECK(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen));
		if (infoLen > 0) {
			char* infoLog = malloc(infoLen);
			CHECK(glGetProgramInfoLog(program, infoLen, NULL, infoLog));
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		CHECK(glDeleteProgram(program));
		return 0;
	}

	CHECK(glDeleteShader(vs));
	CHECK(glDeleteShader(fs));

	return program;
}

tfx_vertex_format tfx_vertex_format_start() {
	tfx_vertex_format fmt;
	memset(&fmt, 0, sizeof(tfx_vertex_format));

	return fmt;
}

void tfx_vertex_format_add(tfx_vertex_format *fmt, size_t count, bool normalized, tfx_component_type type) {
	tfx_vertex_component component;
	memset(&component, 0, sizeof(tfx_vertex_component));

	component.offset = 0;
	component.size = count;
	component.normalized = normalized;
	component.type = type;

	sb_push(fmt->components, component);
}

void tfx_vertex_format_end(tfx_vertex_format *fmt) {
	size_t stride = 0;
	int nc = sb_count(fmt->components);
	for (int i = 0; i < nc; i++) {
		tfx_vertex_component *vc = &fmt->components[i];
		size_t bytes = 0;
		switch (vc->type) {
			case TFX_TYPE_SKIP:
			case TFX_TYPE_UBYTE:
			case TFX_TYPE_BYTE: bytes = 1; break;
			case TFX_TYPE_USHORT:
			case TFX_TYPE_SHORT: bytes = 2; break;
			case TFX_TYPE_FLOAT: bytes = 4; break;
			default: assert(false); break;
		}
		vc->offset = stride;
		stride += vc->size * bytes;
	}
	fmt->stride = stride;
}

tfx_buffer tfx_buffer_new(void *data, size_t size, tfx_vertex_format *format, tfx_buffer_usage usage) {
	GLenum target = format == NULL ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
	GLenum gl_usage = GL_STATIC_DRAW;
	switch (usage) {
		case TFX_USAGE_STATIC:  gl_usage = GL_STATIC_DRAW; break;
		case TFX_USAGE_DYNAMIC: gl_usage = GL_DYNAMIC_DRAW; break;
		case TFX_USAGE_STREAM:  gl_usage = GL_STREAM_DRAW; break;
		default: assert(false); break;
	}

	tfx_buffer buffer;
	memset(&buffer, 0, sizeof(tfx_buffer));
	buffer.gl_id = 0;
	buffer.format = format;

	CHECK(glGenBuffers(1, &buffer.gl_id));
	CHECK(glBindBuffer(target, buffer.gl_id));

	if (size != 0) {
		CHECK(glBufferData(target, size, data, gl_usage));
	}

	return buffer;
}

tfx_texture tfx_texture_new(uint16_t w, uint16_t h, void *data, bool gen_mips, tfx_format format) {
	tfx_texture t;
	memset(&t, 0, sizeof(tfx_texture));

	t.width = w;
	t.height = h;
	t.format = format;

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gen_mips? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLenum gl_fmt = 0;
	GLenum gl_type = 0;
	switch (format) {
		case TFX_FORMAT_RGB565:
			gl_fmt = GL_RGB;
			gl_type = GL_UNSIGNED_SHORT_5_6_5;
			break;
		case TFX_FORMAT_RGBA8:
			gl_fmt = GL_RGBA;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		default:
			assert(false);
			break;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_fmt, w, h, 0, gl_fmt, gl_type, data);
	if (gen_mips && data) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	t.gl_id = id;

	return t;
}

tfx_canvas tfx_canvas_new(uint16_t w, uint16_t h, tfx_format format) {
	tfx_canvas c;
	memset(&c, 0, sizeof(tfx_canvas));

	c.width  = w;
	c.height = h;
	c.format = format;

	GLenum color_format = 0;
	GLenum depth_format = 0;
	GLenum internal = 0;

	switch (format) {
		case TFX_FORMAT_RGBA8: {
			color_format = GL_UNSIGNED_BYTE;
			internal = GL_RGB;
			break;
		}
		case TFX_FORMAT_RGB565: {
			color_format = GL_UNSIGNED_SHORT_5_6_5;
			internal = GL_RGB;
			break;
		}
		case TFX_FORMAT_D16: {
			depth_format = GL_DEPTH_COMPONENT16;
			break;
		}
		case TFX_FORMAT_RGBA8_D16: {
			color_format = GL_UNSIGNED_BYTE;
			depth_format = GL_DEPTH_COMPONENT16;
			internal = GL_RGB;
			break;
		}
		case TFX_FORMAT_RGB565_D16: {
			color_format = GL_UNSIGNED_SHORT_5_6_5;
			depth_format = GL_DEPTH_COMPONENT16;
			internal = GL_RGB;
			break;
		}
		default: assert(false); break;
	}

	// and now the fbo.
	GLuint fbo;
	CHECK(glGenFramebuffers(1, &fbo));
	CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

	// setup color buffer...
	if (color_format) {
		assert(internal != 0);

		GLuint color;
		CHECK(glGenTextures(1, &color));
		CHECK(glBindTexture(GL_TEXTURE_2D, color));
		CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, internal, color_format, NULL));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0));
	}

	// setup depth buffer...
	if (depth_format) {
		GLuint rbo;
		CHECK(glGenRenderbuffers(1, &rbo));
		CHECK(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
		CHECK(glRenderbufferStorage(GL_RENDERBUFFER, depth_format, w, h));
		CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo));
	}

	GLenum status = CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER));
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		// TODO: return something more error-y
		return c;
	}

	c.gl_id = fbo;

	return c;
}

static size_t uniform_size_for(tfx_uniform_type type) {
	switch (type) {
		case TFX_UNIFORM_FLOAT: return sizeof(float);
		case TFX_UNIFORM_INT: return sizeof(uint32_t);
		case TFX_UNIFORM_VEC2: return sizeof(float)*2;
		case TFX_UNIFORM_VEC3: return sizeof(float)*3;
		case TFX_UNIFORM_VEC4: return sizeof(float)*4;
		case TFX_UNIFORM_MAT2: return sizeof(float)*4;
		case TFX_UNIFORM_MAT3: return sizeof(float)*9;
		case TFX_UNIFORM_MAT4: return sizeof(float)*16;
		default: return 0;
	}
	return 0;
}

tfx_uniform tfx_uniform_new(const char *name, tfx_uniform_type type, int count) {
	tfx_uniform u;
	memset(&u, 0, sizeof(tfx_uniform));

	u.name  = name;
	u.type  = type;
	u.count = count;
	u.size  = count * uniform_size_for(type);

	return u;
}

void tfx_set_uniform(tfx_uniform *uniform, float *data) {
	uniform->data = ub_cursor;
	memcpy(uniform->fdata, data, uniform->size);
	ub_cursor += uniform->size;

	sb_push(uniforms, *uniform);
}

void tfx_view_set_canvas(uint8_t id, tfx_canvas *canvas) {
	tfx_view *view = &views[id];
	assert(view != NULL);
	view->canvas = canvas;
}

void tfx_view_set_clear_color(uint8_t id, int color) {
	tfx_view *view = &views[id];
	assert(view != NULL);
	view->flags |= TFX_VIEW_CLEAR_COLOR;
	view->clear_color = color;
}

void tfx_view_set_clear_depth(uint8_t id, float depth) {
	tfx_view *view = &views[id];
	assert(view != NULL);
	view->flags |= TFX_VIEW_CLEAR_DEPTH;
	view->clear_depth = depth;
}

void tfx_view_set_depth_test(uint8_t id, tfx_depth_test mode) {
	tfx_view *view = &views[id];
	assert(view != NULL);

	view->flags &= ~TFX_VIEW_DEPTH_TEST_MASK;
	switch (mode) {
		case TFX_DEPTH_TEST_NONE: break; /* already cleared */
		case TFX_DEPTH_TEST_LT: {
			view->flags |= TFX_VIEW_DEPTH_TEST_LT;
			break;
		}
		case TFX_DEPTH_TEST_GT: {
			view->flags |= TFX_VIEW_DEPTH_TEST_GT;
			break;
		}
		default: assert(false); break;
	}
}

uint16_t tfx_view_get_width(uint8_t id) {
	tfx_view *view = &views[id];
	assert(view != NULL);

	if (view->canvas != NULL) {
		return view->canvas->width;
	}

	return backbuffer.width;
}

uint16_t tfx_view_get_height(uint8_t id) {
	tfx_view *view = &views[id];
	assert(view != NULL);

	if (view->canvas != NULL) {
		return view->canvas->height;
	}

	return backbuffer.height;
}

void tfx_view_get_dimensions(uint8_t id, uint16_t *w, uint16_t *h) {
	if (w) {
		*w = tfx_view_get_width(id);
	}
	if (h) {
		*h = tfx_view_get_height(id);
	}
}

void tfx_view_set_scissor(uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	tfx_view *view = &views[id];
	view->flags |= TFX_VIEW_SCISSOR;

	tfx_rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	view->scissor_rect = rect;
}

static tfx_draw tmp_draw;

static void reset() {
	memset(&tmp_draw, 0, sizeof(tfx_draw));
}

void tfx_set_callback(tfx_draw_callback cb) {
	tmp_draw.callback = cb;
}

void tfx_set_texture(tfx_uniform *uniform, tfx_texture *tex, uint8_t slot) {
	assert(slot <= 8);
	assert(uniform != NULL);
	assert(uniform->count == 1);

	uniform->data = ub_cursor;
	uniform->idata[0] = slot;
	ub_cursor += uniform->size;

	sb_push(uniforms, *uniform);

	tmp_draw.textures[slot] = tex;
}

void tfx_set_state(uint64_t flags) {
	tmp_draw.flags = flags;
}

void tfx_set_vertices(tfx_buffer *vbo, int count) {
	assert(vbo != NULL);
	assert(vbo->format);

	tmp_draw.vbo = vbo;
	if (!tmp_draw.ibo) {
		tmp_draw.indices = count;
	}
}

void tfx_set_indices(tfx_buffer *ibo, int count) {
	tmp_draw.ibo = ibo;
	tmp_draw.indices = count;
}

// TODO: check that this works in real use...
#define TS_HASHSIZE 101

typedef struct nlist {
	struct nlist *next;
	char *name;
} nlist;

unsigned ts_hash(const char *s) {
	unsigned hashval;
	for (hashval = 0; *s != '\0'; s++)
		hashval = *s + 31 * hashval;
	return hashval % TS_HASHSIZE;
}

bool ts_lookup(nlist **hashtab, const char *s) {
	struct nlist *np;
	for (np = hashtab[ts_hash(s)]; np != NULL; np = np->next) {
		if (strcmp(s, np->name) == 0) {
			return true;
		}
	}
	return false;
}

void ts_set(nlist **hashtab, const char *name) {
	if (ts_lookup(hashtab, name)) {
		return;
	}

	unsigned hashval = ts_hash(name);
	nlist *np = malloc(sizeof(nlist));
	np->name = strdup(name);
	np->next = hashtab[hashval];
	hashtab[hashval] = np;
}

nlist **ts_new() {
	nlist **hashtab = malloc(sizeof(nlist*)*TS_HASHSIZE);
	memset(hashtab, 0, sizeof(nlist*)*TS_HASHSIZE);
	return hashtab;
}

void ts_delete(nlist **hashtab) {
	for (int i = 0; i < TS_HASHSIZE; i++) {
		if (hashtab[i] != NULL) {
			free(hashtab[i]);
		}
	}
}

void tfx_submit(uint8_t id, tfx_program program, bool retain) {
	tfx_view *view = &views[id];
	tmp_draw.program = program;
	assert(program != 0);

	if (view != NULL) {
		tfx_draw add_state;
		memcpy(&add_state, &tmp_draw, sizeof(tfx_draw));

		nlist **found = ts_new();

		// TODO: this entire thing could probably be much faster.
		// TODO: look into caching uniform locations. never trust GL.
		int n = sb_count(uniforms);
		for (int i = n-1; i >= 0; i--) {
			tfx_uniform uniform = uniforms[i];
			GLuint loc = CHECK(glGetUniformLocation(program, uniform.name));
			if (loc >= 0) {
				// only record the last update for a given uniform
				if (!ts_lookup(found, uniform.name)) {
					tfx_uniform found_uniform;
					memcpy(&found_uniform, &uniform, sizeof(tfx_uniform));

					found_uniform.data = ub_cursor;
					memcpy(found_uniform.data, uniform.data, uniform.size);
					ub_cursor += uniform.size;

					ts_set(found, uniform.name);
					sb_push(add_state.uniforms, found_uniform);
				}
			}
		}

		ts_delete(found);

		sb_push(view->draws, add_state);
	}
	if (!retain) {
		reset();
	}
}

void tfx_touch(uint8_t id) {
	tfx_view *view = &views[id];
	assert(view != NULL);

	reset();
	sb_push(view->draws, tmp_draw);
}

static inline tfx_canvas *get_canvas(tfx_view *view) {
	assert(view != NULL);
	if (view->canvas != NULL) {
		return view->canvas;
	}
	return &backbuffer;
}

void tfx_blit(uint8_t dst, uint8_t src, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	tfx_rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	tfx_blit_op blit;
	blit.source = get_canvas(&views[src]);
	blit.rect = rect;

	// this would cause a GL error and doesn't make sense.
	assert(blit.source != get_canvas(&views[dst]));

	tfx_view *view = &views[dst];
	sb_push(view->blits, blit);
}

static void release_compiler() {
	if (!shaderc_allocated) {
		return;
	}

	int release_shader_c = 0;
	CHECK(glGetIntegerv(GL_SHADER_COMPILER, &release_shader_c));

	if (release_shader_c) {
		CHECK(glReleaseShaderCompiler());
	}

	shaderc_allocated = false;
}

tfx_stats tfx_frame() {
	/* This isn't used on RPi, but should free memory on some devices. When
	 * you call tfx_frame, you should be done with your shader compiles for
	 * a good while, since that should only be done during init/loading. */
	release_compiler();

	tfx_stats stats;
	memset(&stats, 0, sizeof(tfx_stats));

	int last_count = 0;

	for (int id = 0; id < VIEW_MAX; id++) {
		tfx_view *view = &views[id];

		int nd = sb_count(view->draws);
		if (nd == 0) {
			continue;
		}

		stats.draws += nd;

		tfx_canvas *canvas = get_canvas(view);

		int nb = sb_count(view->blits);
		stats.blits += nb;

		// TODO: fast blit path for ES3+

		CHECK(glBindFramebuffer(GL_FRAMEBUFFER, canvas->gl_id));
		CHECK(glViewport(0, 0, canvas->width, canvas->height));
		if (view->flags & TFX_VIEW_SCISSOR) {
			CHECK(glScissor(
				view->scissor_rect.x,
				view->scissor_rect.y,
				view->scissor_rect.w,
				view->scissor_rect.h
			));
		}

		GLuint mask = 0;
		if (view->flags & TFX_VIEW_CLEAR_COLOR) {
			mask |= GL_COLOR_BUFFER_BIT;
			int color = view->clear_color;
			float c[] = {
				((color >> 24) & 0xff) / 255.0f,
				((color >> 16) & 0xff) / 255.0f,
				((color >>  8) & 0xff) / 255.0f,
				((color >>  0) & 0xff) / 255.0f
			};
			CHECK(glClearColor(c[0], c[1], c[2], c[3]));
		}
		if (view->flags & TFX_VIEW_CLEAR_DEPTH) {
			mask |= GL_DEPTH_BUFFER_BIT;
			CHECK(glClearDepthf(view->clear_depth));
		}

		if (mask != 0) {
			CHECK(glClear(mask));
		}

		// fallback blit path for ES2 (no glBlitFramebuffer available)
		/*
		for (int i = 0; i < nb; i++) {
			tfx_blit_op blit = view->blits[i];
			// TODO
		}
		*/

		GLuint program = 0;

		if (view->flags & TFX_VIEW_DEPTH_TEST_MASK) {
			CHECK(glEnable(GL_DEPTH_TEST));
			if (view->flags & TFX_VIEW_DEPTH_TEST_LT) {
				CHECK(glDepthFunc(GL_LEQUAL));
			}
			else if (view->flags & TFX_VIEW_DEPTH_TEST_GT) {
				CHECK(glDepthFunc(GL_GEQUAL));
			}
		}
		else {
			CHECK(glDisable(GL_DEPTH_TEST));
		}

		// TODO: reduce redundant state setting
		for (int i = 0; i < nd; i++) {
			tfx_draw draw = view->draws[i];
			if (draw.program != program) {
				CHECK(glUseProgram(draw.program));
				program = draw.program;
			}
			CHECK(glDepthMask((draw.flags & TFX_STATE_DEPTH_WRITE) > 0));
			if (draw.flags & TFX_STATE_CULL_CW) {
				CHECK(glEnable(GL_CULL_FACE));
				CHECK(glFrontFace(GL_CW));
			}
			else if (draw.flags & TFX_STATE_CULL_CCW) {
				CHECK(glEnable(GL_CULL_FACE));
				CHECK(glFrontFace(GL_CCW));
			}
			else {
				CHECK(glDisable(GL_CULL_FACE));
			}

			int nu = sb_count(draw.uniforms);
			for (int j = 0; j < nu; j++) {
				tfx_uniform uniform = draw.uniforms[j];
				GLint loc = CHECK(glGetUniformLocation(program, uniform.name));
				switch (uniform.type) {
					case TFX_UNIFORM_INT:   CHECK(glUniform1i(loc, *uniform.idata)); break;
					case TFX_UNIFORM_FLOAT: CHECK(glUniform1f(loc, *uniform.fdata)); break;
					case TFX_UNIFORM_VEC2:  CHECK(glUniform2fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_VEC3:  CHECK(glUniform3fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_VEC4:  CHECK(glUniform4fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_MAT2:  CHECK(glUniformMatrix2fv(loc, uniform.count, 0, uniform.fdata)); break;
					case TFX_UNIFORM_MAT3:  CHECK(glUniformMatrix3fv(loc, uniform.count, 0, uniform.fdata)); break;
					case TFX_UNIFORM_MAT4:  CHECK(glUniformMatrix4fv(loc, uniform.count, 0, uniform.fdata)); break;
					default: assert(false); break;
				}
			}

			if (draw.callback != NULL) {
				draw.callback();
			}

			if (!draw.vbo) {
				continue;
			}

			GLenum mode = GL_TRIANGLES;
			switch (draw.flags & TFX_STATE_DRAW_MASK) {
				case TFX_STATE_DRAW_POINTS:     mode = GL_POINTS; break;
				case TFX_STATE_DRAW_LINES:      mode = GL_LINES; break;
				case TFX_STATE_DRAW_LINE_STRIP: mode = GL_LINE_STRIP; break;
				case TFX_STATE_DRAW_LINE_LOOP:  mode = GL_LINE_LOOP; break;
				case TFX_STATE_DRAW_TRI_STRIP:  mode = GL_TRIANGLE_STRIP; break;
				case TFX_STATE_DRAW_TRI_FAN:    mode = GL_TRIANGLE_FAN; break;
				default: break; // unspecified = triangles.
			}

			GLuint vbo = draw.vbo->gl_id;

			CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));

			tfx_vertex_format *fmt = draw.vbo->format;
			int nc = sb_count(fmt->components);
			int real = 0;
			for (int i = 0; i < nc; i++) {
				tfx_vertex_component vc = fmt->components[i];
				GLenum gl_type = GL_FLOAT;
				switch (vc.type) {
					case TFX_TYPE_SKIP: continue;
					case TFX_TYPE_UBYTE:  gl_type = GL_UNSIGNED_BYTE; break;
					case TFX_TYPE_BYTE:   gl_type = GL_BYTE; break;
					case TFX_TYPE_USHORT: gl_type = GL_UNSIGNED_SHORT; break;
					case TFX_TYPE_SHORT:  gl_type = GL_SHORT; break;
					case TFX_TYPE_FLOAT: break;
					default: assert(false); break;
				}
				CHECK(glEnableVertexAttribArray(real));
				CHECK(glVertexAttribPointer(real, vc.size, gl_type, vc.normalized, fmt->stride, (GLvoid*)vc.offset));
				real += 1;
			}
			nc = last_count - nc;
			for (int i = 0; i <= nc; i++) {
				CHECK(glDisableVertexAttribArray(last_count-i));
			}
			last_count = real;

			for (int i = 0; i < 8; i++) {
				CHECK(glActiveTexture(GL_TEXTURE0+i));
				if (draw.textures[i] != NULL) {
					CHECK(glBindTexture(GL_TEXTURE_2D, draw.textures[i]->gl_id));
				}
				else {
					CHECK(glBindTexture(GL_TEXTURE_2D, 0));
				}
			}

			if (draw.ibo) {
				CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw.ibo->gl_id));
				CHECK(glDrawElements(mode, draw.indices, GL_UNSIGNED_SHORT, (GLvoid*)draw.offset));
			}
			else {
				CHECK(glDrawArrays(mode, draw.offset, draw.indices));
			}
		}

		sb_free(view->draws);
		view->draws = NULL;

		sb_free(view->blits);
		view->blits = NULL;
	}

	reset();

	sb_free(uniforms);
	uniforms = NULL;

	ub_cursor = uniform_buffer;

	return stats;
}
#undef MAX_VIEW

#endif // TFX_IMPLEMENTATION
