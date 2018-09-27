#define TFX_IMPLEMENTATION
// #define TFX_USE_GLES 31
// #define TFX_USE_GL 32
// #define TFX_USE_EPOXY 1
#define TFX_DEBUG
#include "tinyfx.h"

#ifdef TFX_LEAK_CHECK
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"
#endif

/**********************\
| implementation stuff |
\**********************/
#ifdef TFX_IMPLEMENTATION

// TODO: grab function pointers at runtime...
// TODO: restore support for GL < 4.3
#include <GL/glcorearb.h>

#include <string.h>
#include <stdio.h>
#ifdef TFX_DEBUG
#include <assert.h>
#else
#define assert(op) (void)(op);
#endif

// needed on msvc
#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf _snprintf
#endif

#ifndef TFX_UNIFORM_BUFFER_SIZE
// by default, allow up to 8MB of uniform updates per frame.
#define TFX_UNIFORM_BUFFER_SIZE 1024*1024*8
#endif

#ifndef TFX_TRANSIENT_BUFFER_SIZE
// by default, allow up to 8MB of transient buffer data per frame.
#define TFX_TRANSIENT_BUFFER_SIZE 1024*1024*8
#endif

// The following code is public domain, from https://github.com/nothings/stb
//////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
#define STB_STRETCHY_BUFFER_CPP
#endif
#ifndef STB_STRETCHY_BUFFER_H_INCLUDED
#define STB_STRETCHY_BUFFER_H_INCLUDED

#ifndef NO_STRETCHY_BUFFER_SHORT_NAMES
#define sb_free   stb_sb_free
#define sb_push   stb_sb_push
#define sb_count  stb_sb_count
#define sb_add    stb_sb_add
#define sb_last   stb_sb_last
#endif

#ifdef  STB_STRETCHY_BUFFER_CPP
#define stb_sb_push(t,a,v)      (stb__sbmaybegrow(t,a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_add(t,a,n)       (stb__sbmaybegrow(t,a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb__sbmaybegrow(t,a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(t,a,n) : 0)
#define stb__sbgrow(t,a,n)      ((a) = (t*)stb__sbgrowf((void*)(a), (n), sizeof(t)))
#else
#define stb_sb_push(a,v)        (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_add(a,n)         (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb__sbmaybegrow(a,n)   (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)        ((a) = stb__sbgrowf((a), (n), sizeof(*(a))))
#endif

#define stb_sb_free(a)          ((a) ? free(stb__sbraw(a)),0 : 0)
#define stb_sb_count(a)         ((a) ? stb__sbn(a) : 0)
#define stb_sb_last(a)          ((a)[stb__sbn(a)-1])

#define stb__sbraw(a) ((int *) (a) - 2)
#define stb__sbm(a)   stb__sbraw(a)[0]
#define stb__sbn(a)   stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))

#include <stdlib.h>

static void * stb__sbgrowf(void *arr, int increment, int itemsize)
{
	int dbl_cur = arr ? 2 * stb__sbm(arr) : 0;
	int min_needed = stb_sb_count(arr) + increment;
	int m = dbl_cur > min_needed ? dbl_cur : min_needed;
	int *p = (int *)realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int) * 2);
	if (p) {
		if (!arr)
			p[1] = 0;
		p[0] = m;
		return p + 2;
	}
	else {
#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
		STRETCHY_BUFFER_OUT_OF_MEMORY;
#endif
		return (void *)(2 * sizeof(int)); // try to force a NULL pointer exception later
	}
}
#endif // STB_STRETCHY_BUFFER_H_INCLUDED
//////////////////////////////////////////////////////////////////////////////

static char *tfx_strdup(const char *src) {
	size_t len = strlen(src) + 1;
	char *s = malloc(len);
	if (s == NULL)
		return NULL;
	return (char *)memcpy(s, src, len);
}

#ifdef TFX_DEBUG
#define CHECK(fn) fn; { GLenum _status; while ((_status = tfx_glGetError())) { if (_status == GL_NO_ERROR) break; printf("%s:%d GL ERROR: %d\n", __FILE__, __LINE__, _status); } }
#else
#define CHECK(fn) fn;
#endif

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

typedef struct tfx_draw {
	tfx_draw_callback callback;
	uint64_t flags;

	tfx_program program;
	tfx_uniform *uniforms;

	// TODO: remove indirection
	tfx_texture *textures[8];
	tfx_buffer *ssbos[8];
	bool ssbo_write[8];
	tfx_buffer vbo;
	bool use_vbo;

	tfx_buffer ibo;
	bool use_ibo;

	tfx_vertex_format tvb_fmt;
	bool use_tvb;

	tfx_rect scissor_rect;
	bool use_scissor;

	size_t offset;
	uint16_t indices;
	uint32_t depth;

	// for compute jobs
	uint32_t threads_x;
	uint32_t threads_y;
	uint32_t threads_z;
} tfx_draw;

typedef struct tfx_view {
	uint32_t flags;

	const char *name;

	bool has_canvas;
	tfx_canvas  canvas;
	tfx_draw    *draws;
	tfx_draw    *jobs;
	tfx_blit_op *blits;

	int   clear_color;
	float clear_depth;

	tfx_rect scissor_rect;

	float view[16];
	float proj_left[16];
	float proj_right[16];
} tfx_view;

#define TFX_VIEW_CLEAR_MASK      (TFX_VIEW_CLEAR_COLOR | TFX_VIEW_CLEAR_DEPTH)
#define TFX_VIEW_DEPTH_TEST_MASK (TFX_VIEW_DEPTH_TEST_LT | TFX_VIEW_DEPTH_TEST_GT)

#define TFX_STATE_CULL_MASK      (TFX_STATE_CULL_CW | TFX_STATE_CULL_CCW)
#define TFX_STATE_BLEND_MASK     (TFX_STATE_BLEND_ALPHA)
#define TFX_STATE_DRAW_MASK      (TFX_STATE_DRAW_POINTS \
	| TFX_STATE_DRAW_LINES | TFX_STATE_DRAW_LINE_STRIP | TFX_STATE_DRAW_LINE_LOOP \
	| TFX_STATE_DRAW_TRI_STRIP | TFX_STATE_DRAW_TRI_FAN \
)

static tfx_canvas g_backbuffer;

typedef struct tfx_glext {
	const char *ext;
	bool supported;
} tfx_glext;

static tfx_glext available_exts[] = {
	{ "GL_ARB_multisample", false },
	{ "GL_ARB_compute_shader", false },
	{ "GL_ARB_texture_float", false },
	{ "GL_EXT_debug_marker", false },
	{ "GL_ARB_debug_output", false },
	{ "GL_KHR_debug", false },
	{ "GL_NVX_gpu_memory_info", false },
	{ NULL, false }
};

PFNGLGETSTRINGPROC tfx_glGetString;
PFNGLGETSTRINGIPROC tfx_glGetStringi;
PFNGLGETERRORPROC tfx_glGetError;
PFNGLBLENDFUNCPROC tfx_glBlendFunc;
PFNGLCOLORMASKPROC tfx_glColorMask;
PFNGLGETINTEGERVPROC tfx_glGetIntegerv;
PFNGLGENBUFFERSPROC tfx_glGenBuffers;
PFNGLBINDBUFFERPROC tfx_glBindBuffer;
PFNGLBUFFERDATAPROC tfx_glBufferData;
PFNGLDELETEBUFFERSPROC tfx_glDeleteBuffers;
PFNGLCREATESHADERPROC tfx_glCreateShader;
PFNGLSHADERSOURCEPROC tfx_glShaderSource;
PFNGLCOMPILESHADERPROC tfx_glCompileShader;
PFNGLGETSHADERIVPROC tfx_glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC tfx_glGetShaderInfoLog;
PFNGLDELETESHADERPROC tfx_glDeleteShader;
PFNGLCREATEPROGRAMPROC tfx_glCreateProgram;
PFNGLATTACHSHADERPROC tfx_glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC tfx_glBindAttribLocation;
PFNGLLINKPROGRAMPROC tfx_glLinkProgram;
PFNGLGETPROGRAMIVPROC tfx_glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC tfx_glGetProgramInfoLog;
PFNGLDELETEPROGRAMPROC tfx_glDeleteProgram;
PFNGLGENTEXTURESPROC tfx_glGenTextures;
PFNGLBINDTEXTUREPROC tfx_glBindTexture;
PFNGLTEXPARAMETERIPROC tfx_glTexParameteri;
PFNGLPIXELSTOREIPROC tfx_glPixelStorei;
PFNGLTEXIMAGE2DPROC tfx_glTexImage2D;
PFNGLGENERATEMIPMAPPROC tfx_glGenerateMipmap;
PFNGLGENFRAMEBUFFERSPROC tfx_glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC tfx_glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC tfx_glFramebufferTexture2D;
PFNGLGENRENDERBUFFERSPROC tfx_glGenRenderbuffers;
PFNGLBINDRENDERBUFFERPROC tfx_glBindRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC tfx_glRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFERPROC tfx_glFramebufferRenderbuffer;
PFNGLCHECKFRAMEBUFFERSTATUSPROC tfx_glCheckFramebufferStatus;
PFNGLGETUNIFORMLOCATIONPROC tfx_glGetUniformLocation;
PFNGLRELEASESHADERCOMPILERPROC tfx_glReleaseShaderCompiler;
PFNGLGENVERTEXARRAYSPROC tfx_glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC tfx_glBindVertexArray;
PFNGLMAPBUFFERRANGEPROC tfx_glMapBufferRange;
PFNGLBUFFERSUBDATAPROC tfx_glBufferSubData;
PFNGLUNMAPBUFFERPROC tfx_glUnmapBuffer;
PFNGLUSEPROGRAMPROC tfx_glUseProgram;
PFNGLMEMORYBARRIERPROC tfx_glMemoryBarrier;
PFNGLBINDBUFFERBASEPROC tfx_glBindBufferBase;
PFNGLDISPATCHCOMPUTEPROC tfx_glDispatchCompute;
PFNGLVIEWPORTPROC tfx_glViewport;
PFNGLSCISSORPROC tfx_glScissor;
PFNGLCLEARCOLORPROC tfx_glClearColor;
PFNGLCLEARDEPTHFPROC tfx_glClearDepthf;
PFNGLCLEARPROC tfx_glClear;
PFNGLENABLEPROC tfx_glEnable;
PFNGLDEPTHFUNCPROC tfx_glDepthFunc;
PFNGLDISABLEPROC tfx_glDisable;
PFNGLDEPTHMASKPROC tfx_glDepthMask;
PFNGLFRONTFACEPROC tfx_glFrontFace;
PFNGLUNIFORM1IPROC tfx_glUniform1i;
PFNGLUNIFORM1FPROC tfx_glUniform1f;
PFNGLUNIFORM2FVPROC tfx_glUniform2fv;
PFNGLUNIFORM3FVPROC tfx_glUniform3fv;
PFNGLUNIFORM4FVPROC tfx_glUniform4fv;
PFNGLUNIFORMMATRIX2FVPROC tfx_glUniformMatrix2fv;
PFNGLUNIFORMMATRIX3FVPROC tfx_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC tfx_glUniformMatrix4fv;
PFNGLENABLEVERTEXATTRIBARRAYPROC tfx_glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC tfx_glVertexAttribPointer;
PFNGLDISABLEVERTEXATTRIBARRAYPROC tfx_glDisableVertexAttribArray;
PFNGLACTIVETEXTUREPROC tfx_glActiveTexture;
PFNGLDRAWELEMENTSINSTANCEDPROC tfx_glDrawElementsInstanced;
PFNGLDRAWARRAYSINSTANCEDPROC tfx_glDrawArraysInstanced;
PFNGLDRAWELEMENTSPROC tfx_glDrawElements;
PFNGLDRAWARRAYSPROC tfx_glDrawArrays;
PFNGLDELETEVERTEXARRAYSPROC tfx_glDeleteVertexArrays;
PFNGLBINDFRAGDATALOCATIONPROC tfx_glBindFragDataLocation;

// debug output/markers
PFNGLPUSHDEBUGGROUPPROC tfx_glPushDebugGroup;
PFNGLPOPDEBUGGROUPPROC tfx_glPopDebugGroup;
PFNGLINSERTEVENTMARKEREXTPROC tfx_glInsertEventMarkerEXT;

void load_em_up(void* (*get_proc_address)(const char*)) {
	tfx_glGetString = get_proc_address("glGetString");
	tfx_glGetStringi = get_proc_address("glGetStringi");
	tfx_glGetError = get_proc_address("glGetError");
	tfx_glBlendFunc = get_proc_address("glBlendFunc");
	tfx_glColorMask = get_proc_address("glColorMask");
	tfx_glGetIntegerv = get_proc_address("glGetIntegerv");
	tfx_glGenBuffers = get_proc_address("glGenBuffers");
	tfx_glBindBuffer = get_proc_address("glBindBuffer");
	tfx_glBufferData = get_proc_address("glBufferData");
	tfx_glDeleteBuffers = get_proc_address("glDeleteBuffers");
	tfx_glCreateShader = get_proc_address("glCreateShader");
	tfx_glShaderSource = get_proc_address("glShaderSource");
	tfx_glCompileShader = get_proc_address("glCompileShader");
	tfx_glGetShaderiv = get_proc_address("glGetShaderiv");
	tfx_glGetShaderInfoLog = get_proc_address("glGetShaderInfoLog");
	tfx_glDeleteShader = get_proc_address("glDeleteShader");
	tfx_glCreateProgram = get_proc_address("glCreateProgram");
	tfx_glAttachShader = get_proc_address("glAttachShader");
	tfx_glBindAttribLocation = get_proc_address("glBindAttribLocation");
	tfx_glLinkProgram = get_proc_address("glLinkProgram");
	tfx_glGetProgramiv = get_proc_address("glGetProgramiv");
	tfx_glGetProgramInfoLog = get_proc_address("glGetProgramInfoLog");
	tfx_glDeleteProgram = get_proc_address("glDeleteProgram");
	tfx_glGenTextures = get_proc_address("glGenTextures");
	tfx_glBindTexture = get_proc_address("glBindTexture");
	tfx_glTexParameteri = get_proc_address("glTexParameteri");
	tfx_glPixelStorei = get_proc_address("glPixelStorei");
	tfx_glTexImage2D = get_proc_address("glTexImage2D");
	tfx_glGenerateMipmap = get_proc_address("glGenerateMipmap");
	tfx_glGenFramebuffers = get_proc_address("glGenFramebuffers");
	tfx_glBindFramebuffer = get_proc_address("glBindFramebuffer");
	tfx_glFramebufferTexture2D = get_proc_address("glFramebufferTexture2D");
	tfx_glGenRenderbuffers = get_proc_address("glGenRenderbuffers");
	tfx_glBindRenderbuffer = get_proc_address("glBindRenderbuffer");
	tfx_glRenderbufferStorage = get_proc_address("glRenderbufferStorage");
	tfx_glFramebufferRenderbuffer = get_proc_address("glFramebufferRenderbuffer");
	tfx_glCheckFramebufferStatus = get_proc_address("glCheckFramebufferStatus");
	tfx_glGetUniformLocation = get_proc_address("glGetUniformLocation");
	tfx_glReleaseShaderCompiler = get_proc_address("glReleaseShaderCompiler");
	tfx_glGenVertexArrays = get_proc_address("glGenVertexArrays");
	tfx_glBindVertexArray = get_proc_address("glBindVertexArray");
	tfx_glMapBufferRange = get_proc_address("glMapBufferRange");
	tfx_glBufferSubData = get_proc_address("glBufferSubData");
	tfx_glUnmapBuffer = get_proc_address("glUnmapBuffer");
	tfx_glUseProgram = get_proc_address("glUseProgram");
	tfx_glMemoryBarrier = get_proc_address("glMemoryBarrier");
	tfx_glBindBufferBase = get_proc_address("glBindBufferBase");
	tfx_glDispatchCompute = get_proc_address("glDispatchCompute");
	tfx_glViewport = get_proc_address("glViewport");
	tfx_glScissor = get_proc_address("glScissor");
	tfx_glClearColor = get_proc_address("glClearColor");
	tfx_glClearDepthf = get_proc_address("glClearDepthf");
	tfx_glClear = get_proc_address("glClear");
	tfx_glEnable = get_proc_address("glEnable");
	tfx_glDepthFunc = get_proc_address("glDepthFunc");
	tfx_glDisable = get_proc_address("glDisable");
	tfx_glDepthMask = get_proc_address("glDepthMask");
	tfx_glFrontFace = get_proc_address("glFrontFace");
	tfx_glUniform1i = get_proc_address("glUniform1i");
	tfx_glUniform1f = get_proc_address("glUniform1f");
	tfx_glUniform2fv = get_proc_address("glUniform2fv");
	tfx_glUniform3fv = get_proc_address("glUniform3fv");
	tfx_glUniform4fv = get_proc_address("glUniform4fv");
	tfx_glUniformMatrix2fv = get_proc_address("glUniformMatrix2fv");
	tfx_glUniformMatrix3fv = get_proc_address("glUniformMatrix3fv");
	tfx_glUniformMatrix4fv = get_proc_address("glUniformMatrix4fv");
	tfx_glEnableVertexAttribArray = get_proc_address("glEnableVertexAttribArray");
	tfx_glVertexAttribPointer = get_proc_address("glVertexAttribPointer");
	tfx_glDisableVertexAttribArray = get_proc_address("glDisableVertexAttribArray");
	tfx_glActiveTexture = get_proc_address("glActiveTexture");
	tfx_glDrawElementsInstanced = get_proc_address("glDrawElementsInstanced");
	tfx_glDrawArraysInstanced = get_proc_address("glDrawArraysInstanced");
	tfx_glDrawElements = get_proc_address("glDrawElements");
	tfx_glDrawArrays = get_proc_address("glDrawArrays");
	tfx_glDeleteVertexArrays = get_proc_address("glDeleteVertexArrays");
	tfx_glBindFragDataLocation = get_proc_address("glBindFragDataLocation");

	tfx_glPushDebugGroup = get_proc_address("glPushDebugGroup");
	tfx_glPopDebugGroup = get_proc_address("glPopDebugGroup");
	tfx_glInsertEventMarkerEXT = get_proc_address("glInsertEventMarkerEXT");
}

tfx_caps tfx_get_caps() {
	tfx_caps caps;
	memset(&caps, 0, sizeof(tfx_caps));

	if (tfx_glGetStringi) {
		GLint ext_count = 0;
		CHECK(tfx_glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count));

		for (int i = 0; i < ext_count; i++) {
			const char *search = (const char*)CHECK(tfx_glGetStringi(GL_EXTENSIONS, i));
#if defined(_MSC_VER) && 0
			OutputDebugString(search);
			OutputDebugString("\n");
#endif
			for (int j = 0;; j++) {
				tfx_glext *tmp = &available_exts[j];
				if (!tmp->ext) {
					break;
				}
				if (strcmp(tmp->ext, search) == 0) {
					tmp->supported = true;
					break;
				}
			}
		}
	}
	else if (tfx_glGetString) {
		const char *real = (const char*)CHECK(tfx_glGetString(GL_EXTENSIONS));
		char *exts = tfx_strdup(real);
		char *pch = strtok(exts, " ");
		int len = 0;
		const char **supported = NULL;
		while (pch != NULL) {
			sb_push(supported, pch);
			pch = strtok(NULL, " ");
			len++;
		}

		int n = sb_count(supported);
		for (int i = 0; i < n; i++) {
			const char *search = supported[i];

			for (int j = 0; ; j++) {
				tfx_glext *tmp = &available_exts[j];
				if (!tmp->ext) {
					break;
				}
				if (strcmp(tmp->ext, search) == 0) {
					tmp->supported = true;
					break;
				}
			}
		}
		sb_free(supported);
	}

	caps.multisample = available_exts[0].supported;
	caps.compute = available_exts[1].supported;
	caps.float_canvas = available_exts[2].supported;
	caps.debug_marker = available_exts[3].supported || available_exts[5].supported;
	caps.debug_output = available_exts[4].supported;
	caps.memory_info = available_exts[6].supported;

	return caps;
}

static void tfx_printb(const char *k, bool v) {
	printf("TinyFX %s: %s\n", k, v? "true" : "false");
}

void tfx_dump_caps() {
	tfx_caps caps = tfx_get_caps();

	// I am told by the docs that this can be 0.
	// It's not on the RPi, but since it's only a few lines of code...
	int release_shader_c = 0;
	tfx_glGetIntegerv(GL_SHADER_COMPILER, &release_shader_c);
	printf("GL shader compiler control: %d\n", release_shader_c);
	printf("GL vendor: %s\n", tfx_glGetString(GL_VENDOR));
	printf("GL version: %s\n", tfx_glGetString(GL_VERSION));

	puts("GL extensions:");
	if (tfx_glGetStringi) {
		GLint ext_count = 0;
		CHECK(tfx_glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count));

		for (int i = 0; i < ext_count; i++) {
			const char *real = (const char*)CHECK(tfx_glGetStringi(GL_EXTENSIONS, i));
			printf("\t%s\n", real);
		}
	}
	else if (tfx_glGetString) {
		const char *real = (const char*)CHECK(tfx_glGetString(GL_EXTENSIONS));
		char *exts = tfx_strdup(real);
		int len = 0;

		char *pch = strtok(exts, " ");
		while (pch != NULL) {
			printf("\t%s\n", pch);
			pch = strtok(NULL, " ");
			len++;
		}
	}

	puts("TinyFX renderer: "
	#ifdef TFX_USE_GLES // NYI
#define FUG(V) "GLES" #V
		FUG(TFX_USE_GLES/10)
	#else
		"GL4"
	#endif
	);

	tfx_printb("compute", caps.compute);
	tfx_printb("fp canvas", caps.float_canvas);
	tfx_printb("multisample", caps.multisample);
}

// uniforms updated this frame
static tfx_uniform *g_uniforms = NULL;
static uint8_t *g_uniform_buffer = NULL;
static uint8_t *g_ub_cursor = NULL;

static tfx_view g_views[VIEW_MAX];

static struct {
	uint8_t *data;
	uint32_t offset;
	tfx_buffer buf;
} g_transient_buffer;

static tfx_caps g_caps;

static tfx_platform_data g_platform_data;

static void tvb_reset() {
	g_transient_buffer.offset = 0;

	if (!g_transient_buffer.buf.gl_id) {
		GLuint id;
		CHECK(tfx_glGenBuffers(1, &id));
		CHECK(tfx_glBindBuffer(GL_ARRAY_BUFFER, id));
		CHECK(tfx_glBufferData(GL_ARRAY_BUFFER, TFX_TRANSIENT_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW));
		g_transient_buffer.buf.gl_id = id;
	}
}

void tfx_set_platform_data(tfx_platform_data pd) {
	// supported: GL > 3, 2.1, ES 2.0, ES 3.0+
	assert(0
		|| (pd.context_version >= 30)
		|| (pd.use_gles && pd.context_version == 20)
		|| (!pd.use_gles && pd.context_version == 21)
	);
	memcpy(&g_platform_data, &pd, sizeof(tfx_platform_data));
}

// null format = index buffer
tfx_transient_buffer tfx_transient_buffer_new(tfx_vertex_format *fmt, uint16_t num_verts) {
	// transient index buffers aren't supported yet
	assert(fmt != NULL);
	assert(fmt->stride > 0);

	tfx_transient_buffer buf;
	memset(&buf, 0, sizeof(tfx_transient_buffer));
	buf.data = g_transient_buffer.data + g_transient_buffer.offset;
	buf.num = num_verts;
	buf.offset = g_transient_buffer.offset;
	uint32_t stride = sizeof(uint16_t);
	if (fmt) {
		buf.has_format = true;
		buf.format = *fmt;
		stride = fmt->stride;
	}
	g_transient_buffer.offset += (uint32_t)(num_verts * stride);
	g_transient_buffer.offset += g_transient_buffer.offset % 4; // align, in case the stride is weird
	return buf;
}

// null format = available indices (uint16)
uint32_t tfx_transient_buffer_get_available(tfx_vertex_format *fmt) {
	assert(fmt->stride > 0);
	uint32_t avail = TFX_TRANSIENT_BUFFER_SIZE;
	avail -= g_transient_buffer.offset;
	uint32_t stride = sizeof(uint16_t);
	if (fmt) {
		stride = fmt->stride;
	}
	avail /= (uint32_t)stride;
	return avail;
}

void tfx_reset(uint16_t width, uint16_t height) {
	if (g_platform_data.gl_get_proc_address != NULL) {
		load_em_up(g_platform_data.gl_get_proc_address);
	}

	g_caps = tfx_get_caps();

	memset(&g_backbuffer, 0, sizeof(tfx_canvas));
	g_backbuffer.allocated = 1;
	g_backbuffer.gl_id[0] = 0;
	g_backbuffer.width = width;
	g_backbuffer.height = height;

	if (!g_uniform_buffer) {
		g_uniform_buffer = (uint8_t*)malloc(TFX_UNIFORM_BUFFER_SIZE);
		memset(g_uniform_buffer, 0, TFX_UNIFORM_BUFFER_SIZE);
		g_ub_cursor = g_uniform_buffer;
	}

	if (!g_transient_buffer.data) {
		g_transient_buffer.data = (uint8_t*)malloc(TFX_TRANSIENT_BUFFER_SIZE);
		memset(g_transient_buffer.data, 0xfc, TFX_TRANSIENT_BUFFER_SIZE);
		tvb_reset();
	}

#ifdef _MSC_VER
	if (g_caps.memory_info) {
		GLint memory;
		// GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    
		CHECK(tfx_glGetIntegerv(0x9048, &memory));
		char buf[64];
		snprintf(buf, 64, "VRAM: %dMiB\n", memory / 1024);
		OutputDebugString(buf);
	}
#endif

	memset(&g_views, 0, sizeof(tfx_view)*VIEW_MAX);
}

static tfx_program *g_programs = NULL;

void tfx_shutdown() {
	tfx_frame();

	// TODO: clean up all GL objects, allocs, etc.
	free(g_uniform_buffer);
	g_uniform_buffer = NULL;

	free(g_transient_buffer.data);
	g_transient_buffer.data = NULL;

	if (g_transient_buffer.buf.gl_id) {
		tfx_glDeleteBuffers(1, &g_transient_buffer.buf.gl_id);
	}

	// this can happen if you shutdown before calling frame()
	if (g_uniforms) {
		sb_free(g_uniforms);
		g_uniforms = NULL;
	}

	tfx_glUseProgram(0);
	int np = sb_count(g_programs);
	for (int i = 0; i < np; i++) {
		tfx_glDeleteProgram(g_programs[i]);
	}
	g_programs = NULL;

#ifdef TFX_LEAK_CHECK
	stb_leakcheck_dumpmem();
#endif
}

static bool g_shaderc_allocated = false;

static void push_group(unsigned id, const char *label) {
	if (tfx_glPushDebugGroup) {
		CHECK(tfx_glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, id, strlen(label), label));
	}
}

static void pop_group() {
	if (tfx_glPopDebugGroup) {
		CHECK(tfx_glPopDebugGroup());
	}
}

static GLuint load_shader(GLenum type, const char *shaderSrc) {
	g_shaderc_allocated = true;

	GLuint shader = CHECK(tfx_glCreateShader(type));
	if (!shader) {
		fprintf(stderr, "Something has gone horribly wrong, and we can't make shaders.\n");
		return 0;
	}

	CHECK(tfx_glShaderSource(shader, 1, &shaderSrc, NULL));
	CHECK(tfx_glCompileShader(shader));

	GLint compiled;
	CHECK(tfx_glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled));
	if (!compiled) {
		GLint infoLen = 0;
		CHECK(tfx_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen));
		if (infoLen > 0) {
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			CHECK(tfx_glGetShaderInfoLog(shader, infoLen, NULL, infoLog));
			fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
#ifdef _MSC_VER
			OutputDebugString(infoLog);
#endif
			free(infoLog);
		}
		CHECK(tfx_glDeleteShader(shader));
		assert(compiled);
		return 0;
	}

	return shader;
}

const char *legacy_vs_prepend = ""
	"#ifndef GL_ES\n"
	"#define lowp\n"
	"#define mediump\n"
	"#define highp\n"
	"#else\n"
	"precision highp float;\n"
	"#define in attribute\n"
	"#define out varying\n"
	"#endif\n"
	"#pragma optionNV(strict on)\n"
	"#define main _pain\n"
	"#define tfx_viewport_count 1\n"
	"#line 1\n"
;
const char *legacy_fs_prepend = ""
	"#ifndef GL_ES\n"
	"#define lowp\n"
	"#define mediump\n"
	"#define highp\n"
	"#else\n"
	"precision mediump float;\n"
	"#define in varying\n"
	"#endif\n"
	"#pragma optionNV(strict on)\n"
	"#line 1\n"
;
const char *vs_prepend = ""
	"#define main _pain\n"
	"#define tfx_viewport_count 1\n"
	"#line 1\n"
;
const char *fs_prepend = ""
	"#line 1\n"
;

const char *vs_append = ""
	"#undef main\n"
	"void main() {\n"
	"	_pain();\n"
	// TODO: enable extensions if GL < 4.1, add gl_Layer support for
	// single pass cube/shadow cascade rendering
	"#if 0\n"
	"	gl_ViewportIndex = gl_InstanceID % tfx_viewport_count;\n"
	"#endif\n"
	"}\n"
;

static char *sappend(const char *left, const char *right) {
	size_t ls = strlen(left);
	size_t rs = strlen(right);
	char *ss = (char*)malloc(ls+rs+1);
	memcpy(ss, left, ls);
	memcpy(ss+ls, right, rs);
	ss[ls+rs] = '\0';
	return ss;
}

tfx_program tfx_program_new(const char *_vss, const char *_fss, const char *attribs[]) {
	char *vss1, *fss1;
	if (g_platform_data.context_version < 30) {
		vss1 = sappend(legacy_vs_prepend, _vss);
		fss1 = sappend(legacy_fs_prepend, _fss);
	}
	else {
		vss1 = sappend(vs_prepend, _vss);
		fss1 = sappend(fs_prepend, _fss);
	}

	char version[64];
	int gl_major = g_platform_data.context_version / 10;
	int gl_minor = g_platform_data.context_version % 10;
	const char *suffix = " core";
	// post GL3/GLES3, versions are sane, just fix suffix.
	if (g_platform_data.use_gles && g_platform_data.context_version >= 30) {
		suffix = " es";
	}
	// GL and GLES2 use GLSL 1xx versions
	else if (g_platform_data.context_version < 30) {
		suffix = "";
		gl_major = 1;
		// GL 2.1 -> GLSL 120
		if (!g_platform_data.use_gles) {
			gl_minor = 2;
		}
	}
	snprintf(version, 64, "#version %d%d0%s\n", gl_major, gl_minor, suffix);

	char *vss2 = sappend(vss1, vs_append);
	free(vss1);

	char *vss = sappend(version, vss2);
	char *fss = sappend(version, fss1);
	free(vss2);
	free(fss1);

	GLuint vs = load_shader(GL_VERTEX_SHADER, vss);
	GLuint fs = load_shader(GL_FRAGMENT_SHADER, fss);
	GLuint program = CHECK(tfx_glCreateProgram());
	if (!program) {
		return 0;
	}

	free(vss);
	free(fss);

	CHECK(tfx_glAttachShader(program, vs));
	CHECK(tfx_glAttachShader(program, fs));

	const char **it = attribs;
	int i = 0;
	while (*it != NULL) {
		CHECK(tfx_glBindAttribLocation(program, i, *it));
		i++;
		it++;
	}

	// TODO: accept frag data binding array
	if (tfx_glBindFragDataLocation) {
		//CHECK(tfx_glBindFragDataLocation(program, 0, "out_color"));
	}

	CHECK(tfx_glLinkProgram(program));

	GLint linked;
	CHECK(tfx_glGetProgramiv(program, GL_LINK_STATUS, &linked));
	if (!linked) {
		GLint infoLen = 0;
		CHECK(tfx_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen));
		if (infoLen > 0) {
			char* infoLog = (char*)malloc(infoLen);
			CHECK(tfx_glGetProgramInfoLog(program, infoLen, NULL, infoLog));
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		CHECK(tfx_glDeleteProgram(program));
		return 0;
	}

	CHECK(tfx_glDeleteShader(vs));
	CHECK(tfx_glDeleteShader(fs));

	sb_push(g_programs, program);

	return program;
}

tfx_program tfx_program_cs_new(const char *css) {
	if (!g_caps.compute) {
		return 0;
	}

	GLuint cs = load_shader(GL_COMPUTE_SHADER, css);
	GLuint program = CHECK(tfx_glCreateProgram());
	if (!program) {
		return 0;
	}
	CHECK(tfx_glAttachShader(program, cs));
	CHECK(tfx_glLinkProgram(program));

	GLint linked;
	CHECK(tfx_glGetProgramiv(program, GL_LINK_STATUS, &linked));
	if (!linked) {
		GLint infoLen = 0;
		CHECK(tfx_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen));
		if (infoLen > 0) {
			char* infoLog = (char*)malloc(infoLen);
			CHECK(tfx_glGetProgramInfoLog(program, infoLen, NULL, infoLog));
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		CHECK(tfx_glDeleteProgram(program));
		return 0;
	}
	CHECK(tfx_glDeleteShader(cs));

	sb_push(g_programs, program);

	return program;
}

tfx_vertex_format tfx_vertex_format_start() {
	tfx_vertex_format fmt;
	memset(&fmt, 0, sizeof(tfx_vertex_format));

	return fmt;
}

void tfx_vertex_format_add(tfx_vertex_format *fmt, uint8_t slot, size_t count, bool normalized, tfx_component_type type) {
	assert(type >= 0 && type <= TFX_TYPE_SKIP);

	if (slot >= fmt->count) {
		fmt->count = slot + 1;
	}
	tfx_vertex_component *component = &fmt->components[slot];
	memset(component, 0, sizeof(tfx_vertex_component));
	component->offset = 0;
	component->size = count;
	component->normalized = normalized;
	component->type = type;

	fmt->component_mask |= 1 << slot;
}

size_t tfx_vertex_format_offset(tfx_vertex_format *fmt, uint8_t slot) {
	assert(slot < 8);
	return fmt->components[slot].offset;
}

void tfx_vertex_format_end(tfx_vertex_format *fmt) {
	size_t stride = 0;
	int nc = fmt->count;
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
	if (format) {
		assert(format->stride > 0);

		buffer.has_format = true;
		buffer.format = *format;
	}

	CHECK(tfx_glGenBuffers(1, &buffer.gl_id));
	CHECK(tfx_glBindBuffer(GL_ARRAY_BUFFER, buffer.gl_id));

	if (size != 0) {
		CHECK(tfx_glBufferData(GL_ARRAY_BUFFER, size, data, gl_usage));
	}

	return buffer;
}

tfx_texture tfx_texture_new(uint16_t w, uint16_t h, void *data, bool gen_mips, tfx_format format, uint16_t flags) {
	tfx_texture t;
	memset(&t, 0, sizeof(tfx_texture));

	t.width = w;
	t.height = h;
	t.format = format;

	GLuint id = 0;
	CHECK(tfx_glGenTextures(1, &id));
	CHECK(tfx_glBindTexture(GL_TEXTURE_2D, id));
	if ((flags & TFX_TEXTURE_FILTER_POINT) == TFX_TEXTURE_FILTER_POINT) {
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gen_mips ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST));
	}
	else if ((flags & TFX_TEXTURE_FILTER_LINEAR) == TFX_TEXTURE_FILTER_LINEAR || !flags) {
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gen_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	}
	CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

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

	CHECK(tfx_glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
	CHECK(tfx_glTexImage2D(GL_TEXTURE_2D, 0, gl_fmt, w, h, 0, gl_fmt, gl_type, data));
	if (gen_mips && data) {
		CHECK(tfx_glGenerateMipmap(GL_TEXTURE_2D));
	}

	t.gl_id = id;

	assert(id > 0);

	return t;
}

tfx_canvas tfx_canvas_new(uint16_t w, uint16_t h, tfx_format format) {
	tfx_canvas c;
	memset(&c, 0, sizeof(tfx_canvas));

	c.allocated = 0;
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
	CHECK(tfx_glGenFramebuffers(1, &fbo));
	CHECK(tfx_glBindFramebuffer(GL_FRAMEBUFFER, fbo));

	// setup color buffer...
	if (color_format) {
		assert(internal != 0);

		GLuint color;
		CHECK(tfx_glGenTextures(1, &color));
		CHECK(tfx_glBindTexture(GL_TEXTURE_2D, color));
		CHECK(tfx_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, internal, color_format, NULL));
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		CHECK(tfx_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		CHECK(tfx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0));
	}

	// setup depth buffer...
	if (depth_format) {
		GLuint rbo;
		CHECK(tfx_glGenRenderbuffers(1, &rbo));
		CHECK(tfx_glBindRenderbuffer(GL_RENDERBUFFER, rbo));
		CHECK(tfx_glRenderbufferStorage(GL_RENDERBUFFER, depth_format, w, h));
		CHECK(tfx_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo));
	}

	GLenum status = CHECK(tfx_glCheckFramebufferStatus(GL_FRAMEBUFFER));
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		assert(false);
		// TODO: return something more error-y
		return c;
	}

	c.gl_id[0] = fbo;
	c.allocated += 1;

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

void tfx_set_uniform(tfx_uniform *uniform, const float *data) {
	uniform->data = g_ub_cursor;
	memcpy(uniform->fdata, data, uniform->size);
	g_ub_cursor += uniform->size;

	sb_push(g_uniforms, *uniform);
}

void tfx_view_set_transform(uint8_t id, float *_view, float *proj_l, float *proj_r) {
	// TODO: reserve tfx_world_to_view, tfx_view_to_screen uniforms
	tfx_view *view = &g_views[id];
	assert(view != NULL);
	memcpy(view->view, _view, sizeof(float)*16);
	memcpy(view->proj_left, proj_l, sizeof(float)*16);
	memcpy(view->proj_right, proj_r, sizeof(float)*16);
}

void tfx_view_set_name(uint8_t id, const char *name) {
	tfx_view *view = &g_views[id];
	view->name = name;
}

void tfx_view_set_canvas(uint8_t id, tfx_canvas *canvas) {
	tfx_view *view = &g_views[id];
	assert(view != NULL);
	view->has_canvas = true;
	view->canvas = *canvas;
}

void tfx_view_set_clear_color(uint8_t id, int color) {
	tfx_view *view = &g_views[id];
	assert(view != NULL);
	view->flags |= TFX_VIEW_CLEAR_COLOR;
	view->clear_color = color;
}

void tfx_view_set_clear_depth(uint8_t id, float depth) {
	tfx_view *view = &g_views[id];
	assert(view != NULL);
	view->flags |= TFX_VIEW_CLEAR_DEPTH;
	view->clear_depth = depth;
}

void tfx_view_set_depth_test(uint8_t id, tfx_depth_test mode) {
	tfx_view *view = &g_views[id];
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
	tfx_view *view = &g_views[id];
	assert(view != NULL);

	if (view->has_canvas) {
		return view->canvas.width;
	}

	return g_backbuffer.width;
}

uint16_t tfx_view_get_height(uint8_t id) {
	tfx_view *view = &g_views[id];
	assert(view != NULL);

	if (view->has_canvas) {
		return view->canvas.height;
	}

	return g_backbuffer.height;
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
	tfx_view *view = &g_views[id];
	view->flags |= TFX_VIEW_SCISSOR;

	tfx_rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	view->scissor_rect = rect;
}

static tfx_draw g_tmp_draw;

static void reset() {
	memset(&g_tmp_draw, 0, sizeof(tfx_draw));
}

void tfx_set_callback(tfx_draw_callback cb) {
	g_tmp_draw.callback = cb;
}

void tfx_set_scissor(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	g_tmp_draw.use_scissor = true;
	g_tmp_draw.scissor_rect.x = x;
	g_tmp_draw.scissor_rect.y = y;
	g_tmp_draw.scissor_rect.w = w;
	g_tmp_draw.scissor_rect.h = h;
}

void tfx_set_texture(tfx_uniform *uniform, tfx_texture *tex, uint8_t slot) {
	assert(slot <= 8);
	assert(uniform != NULL);
	assert(uniform->count == 1);

	uniform->data = g_ub_cursor;
	uniform->idata[0] = slot;
	g_ub_cursor += uniform->size;

	sb_push(g_uniforms, *uniform);

	assert(tex->gl_id > 0);
	g_tmp_draw.textures[slot] = tex;
}

tfx_texture tfx_get_texture(tfx_canvas *canvas, uint8_t index) {
	tfx_texture tex;
	memset(&tex, 0, sizeof(tfx_texture));

	assert(index < canvas->allocated);

	tex.format = canvas->format;
	tex.gl_id = canvas->gl_id[index];
	tex.width = canvas->width;
	tex.height = canvas->height;

	return tex;
}

void tfx_set_state(uint64_t flags) {
	g_tmp_draw.flags = flags;
}

void tfx_set_buffer(tfx_buffer *buf, uint8_t slot, bool write) {
	assert(slot <= 8);
	assert(buf != NULL);
	g_tmp_draw.ssbos[slot] = buf;
	g_tmp_draw.ssbo_write[slot] = write;
}

// TODO: make this work for index buffers
void tfx_set_transient_buffer(tfx_transient_buffer tb) {
	assert(tb.has_format);
	g_tmp_draw.vbo = g_transient_buffer.buf;
	g_tmp_draw.use_vbo = true;
	g_tmp_draw.use_tvb = true;
	g_tmp_draw.tvb_fmt = tb.format;
	g_tmp_draw.offset = tb.offset;
	g_tmp_draw.indices = tb.num;
}

void tfx_set_vertices(tfx_buffer *vbo, int count) {
	assert(vbo != NULL);
	assert(vbo->has_format);

	g_tmp_draw.vbo = *vbo;
	g_tmp_draw.use_vbo = true;
	if (!g_tmp_draw.use_ibo) {
		g_tmp_draw.indices = count;
	}
}

void tfx_set_indices(tfx_buffer *ibo, int count) {
	g_tmp_draw.ibo = *ibo;
	g_tmp_draw.use_ibo = true;
	g_tmp_draw.indices = count;
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
	assert(s);
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
	np->name = tfx_strdup(name);
	np->next = hashtab[hashval];
	hashtab[hashval] = np;
}

nlist **ts_new() {
	nlist **hashtab = calloc(TS_HASHSIZE, sizeof(nlist*));
	return hashtab;
}

void ts_delete(nlist **hashtab) {
	for (int i = 0; i < TS_HASHSIZE; i++) {
		if (hashtab[i] != NULL) {
			free(hashtab[i]);
		}
	}
	free(hashtab);
}

static void push_uniforms(tfx_program program, tfx_draw *add_state) {
	nlist **found = ts_new();

	// TODO: this entire thing could probably be much faster.
	// TODO: look into caching uniform locations. never trust GL.
	int n = sb_count(g_uniforms);
	for (int i = n-1; i >= 0; i--) {
		tfx_uniform uniform = g_uniforms[i];
		GLint loc = CHECK(tfx_glGetUniformLocation(program, uniform.name));
		if (loc >= 0) {
			// only record the last update for a given uniform
			if (!ts_lookup(found, uniform.name)) {
				tfx_uniform found_uniform;
				memcpy(&found_uniform, &uniform, sizeof(tfx_uniform));

				found_uniform.data = g_ub_cursor;
				memcpy(found_uniform.data, uniform.data, uniform.size);
				g_ub_cursor += uniform.size;

				ts_set(found, uniform.name);
				sb_push(add_state->uniforms, found_uniform);
			}
		}
	}

	ts_delete(found);
}

void tfx_dispatch(uint8_t id, tfx_program program, uint32_t x, uint32_t y, uint32_t z) {
	tfx_view *view = &g_views[id];
	g_tmp_draw.program = program;
	assert(program != 0);
	assert(view != NULL);
	assert((x + y + z) > 0);

	tfx_draw add_state;
	memcpy(&add_state, &g_tmp_draw, sizeof(tfx_draw));
	add_state.threads_x = x;
	add_state.threads_y = y;
	add_state.threads_z = z;

	push_uniforms(program, &add_state);
	sb_push(view->jobs, add_state);

	reset();
}

void tfx_submit(uint8_t id, tfx_program program, bool retain) {
	tfx_view *view = &g_views[id];
	g_tmp_draw.program = program;
	assert(program != 0);
	assert(view != NULL);

	tfx_draw add_state;
	memcpy(&add_state, &g_tmp_draw, sizeof(tfx_draw));
	push_uniforms(program, &add_state);
	sb_push(view->draws, add_state);

	if (!retain) {
		reset();
	}
}

void tfx_submit_ordered(uint8_t id, tfx_program program, uint32_t depth, bool retain) {
	g_tmp_draw.depth = depth;
	tfx_submit(id, program, retain);
}

void tfx_touch(uint8_t id) {
	tfx_view *view = &g_views[id];
	assert(view != NULL);

	reset();
	sb_push(view->draws, g_tmp_draw);
}

static tfx_canvas *get_canvas(tfx_view *view) {
	assert(view != NULL);
	if (view->has_canvas) {
		return &view->canvas;
	}
	return &g_backbuffer;
}

void tfx_blit(uint8_t dst, uint8_t src, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	tfx_rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	tfx_blit_op blit;
	blit.source = get_canvas(&g_views[src]);
	blit.rect = rect;

	// this would cause a GL error and doesn't make sense.
	assert(blit.source != get_canvas(&g_views[dst]));

	tfx_view *view = &g_views[dst];
	sb_push(view->blits, blit);
}

static void release_compiler() {
	if (!g_shaderc_allocated) {
		return;
	}

	int release_shader_c = 0;
	CHECK(tfx_glGetIntegerv(GL_SHADER_COMPILER, &release_shader_c));

	if (release_shader_c) {
		CHECK(tfx_glReleaseShaderCompiler());
	}

	g_shaderc_allocated = false;
}

tfx_stats tfx_frame() {
	/* This isn't used on RPi, but should free memory on some devices. When
	 * you call tfx_frame, you should be done with your shader compiles for
	 * a good while, since that should only be done during init/loading. */
	release_compiler();

	if (g_caps.debug_output) {
		CHECK(tfx_glEnable(GL_DEBUG_OUTPUT));
	}

	tfx_stats stats;
	memset(&stats, 0, sizeof(tfx_stats));

	int last_count = 0;

	//CHECK(tfx_glEnable(GL_FRAMEBUFFER_SRGB));

	GLuint vao;
	if (tfx_glGenVertexArrays && tfx_glBindVertexArray) {
		CHECK(tfx_glGenVertexArrays(1, &vao));
		CHECK(tfx_glBindVertexArray(vao));
	}

	unsigned debug_id = 0;

	push_group(debug_id++, "Update Transient Buffers");

	if (g_transient_buffer.offset > 0) {
		CHECK(tfx_glBindBuffer(GL_ARRAY_BUFFER, g_transient_buffer.buf.gl_id));
		if (tfx_glMapBufferRange && tfx_glUnmapBuffer) {
			void *ptr = tfx_glMapBufferRange(GL_ARRAY_BUFFER, 0, g_transient_buffer.offset, GL_MAP_WRITE_BIT);
			if (ptr) {
				memcpy(ptr, g_transient_buffer.data, g_transient_buffer.offset);
				CHECK(tfx_glUnmapBuffer(GL_ARRAY_BUFFER));
			}
		}
		else {
			CHECK(tfx_glBufferSubData(GL_ARRAY_BUFFER, 0, g_transient_buffer.offset, g_transient_buffer.data));
		}
	}

	pop_group();

	char debug_label[256];

	for (int id = 0; id < VIEW_MAX; id++) {
		tfx_view *view = &g_views[id];

		int nd = sb_count(view->draws);
		int cd = sb_count(view->jobs);
		if (nd == 0 && cd == 0) {
			continue;
		}

		if (view->name) {
			snprintf(debug_label, 256, "%s (%d)", view->name, id);
		}
		else {
			snprintf(debug_label, 256, "View %d", id);
		}
		push_group(debug_id++, debug_label);

		GLuint program = 0;
		if (g_caps.compute) {
			if (cd > 0) {
				// split compute into its own section because it is infrequently used.
				// helpful in renderdoc captures.
				push_group(debug_id++, "Compute");
			}
			for (int i = 0; i < cd; i++) {
				tfx_draw job = view->draws[i];
				if (job.program != program) {
					CHECK(tfx_glUseProgram(job.program));
					program = job.program;
				}
				// TODO: bind image textures
				for (int i = 0; i < 8; i++) {
					if (job.textures[i] != NULL) {
						tfx_buffer *ssbo = job.ssbos[i];
						if (ssbo->dirty) {
							CHECK(tfx_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT));
							ssbo->dirty = false;
						}
						if (job.ssbo_write[i]) {
							ssbo->dirty = true;
						}
						CHECK(tfx_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, job.ssbos[i]->gl_id));
					}
					else {
						CHECK(tfx_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, 0));
					}
				}
				CHECK(tfx_glDispatchCompute(job.threads_x, job.threads_y, job.threads_z));
			}
			if (cd > 0) {
				pop_group();
			}
		}

		if (nd == 0) {
			pop_group();
			continue;
		}

		stats.draws += nd;

		tfx_canvas *canvas = get_canvas(view);

		int nb = sb_count(view->blits);
		stats.blits += nb;

		// TODO: blit support

		// TODO: defer framebuffer creation

		// this can currently only happen on error.
		if (canvas->allocated == 0) {
			pop_group();
			continue;
		}
		CHECK(tfx_glBindFramebuffer(GL_FRAMEBUFFER, canvas->gl_id[0]));
		CHECK(tfx_glViewport(0, 0, canvas->width, canvas->height));

		if (view->flags & TFX_VIEW_SCISSOR) {
			tfx_rect rect = view->scissor_rect;
			CHECK(tfx_glEnable(GL_SCISSOR_TEST));
			CHECK(tfx_glScissor(rect.x, canvas->height - rect.y - rect.h, rect.w, rect.h));
		}
		else {
			CHECK(tfx_glDisable(GL_SCISSOR_TEST));
		}

		CHECK(tfx_glColorMask(true, true, true, true));

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
			CHECK(tfx_glClearColor(c[0], c[1], c[2], c[3]));
		}
		if (view->flags & TFX_VIEW_CLEAR_DEPTH) {
			mask |= GL_DEPTH_BUFFER_BIT;
			CHECK(tfx_glClearDepthf(view->clear_depth));
		}

		if (mask != 0) {
			CHECK(tfx_glClear(mask));
		}

		// fallback blit path for ES2 (no glBlitFramebuffer available)
		/*
		for (int i = 0; i < nb; i++) {
			tfx_blit_op blit = view->blits[i];
			// TODO
		}
		*/

		if (view->flags & TFX_VIEW_DEPTH_TEST_MASK) {
			CHECK(tfx_glEnable(GL_DEPTH_TEST));
			if (view->flags & TFX_VIEW_DEPTH_TEST_LT) {
				CHECK(tfx_glDepthFunc(GL_LEQUAL));
			}
			else if (view->flags & TFX_VIEW_DEPTH_TEST_GT) {
				CHECK(tfx_glDepthFunc(GL_GEQUAL));
			}
		}
		else {
			CHECK(tfx_glDisable(GL_DEPTH_TEST));
		}

		// TODO: reduce redundant state setting
		for (int i = 0; i < nd; i++) {
			tfx_draw draw = view->draws[i];
			if (draw.program != program) {
				CHECK(tfx_glUseProgram(draw.program));
				program = draw.program;
			}
			CHECK(tfx_glDepthMask((draw.flags & TFX_STATE_DEPTH_WRITE) > 0));
			if (g_caps.multisample) {
				if (draw.flags & TFX_STATE_MSAA) {
					CHECK(tfx_glEnable(GL_MULTISAMPLE));
				}
				else {
					CHECK(tfx_glDisable(GL_MULTISAMPLE));
				}
			}
			if (draw.flags & TFX_STATE_CULL_CW) {
				CHECK(tfx_glEnable(GL_CULL_FACE));
				CHECK(tfx_glFrontFace(GL_CW));
			}
			else if (draw.flags & TFX_STATE_CULL_CCW) {
				CHECK(tfx_glEnable(GL_CULL_FACE));
				CHECK(tfx_glFrontFace(GL_CCW));
			}
			else {
				CHECK(tfx_glDisable(GL_CULL_FACE));
			}

			if (draw.flags & TFX_STATE_BLEND_MASK) {
				CHECK(tfx_glEnable(GL_BLEND));
				if (draw.flags & TFX_STATE_BLEND_ALPHA) {
					CHECK(tfx_glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
				}
			}
			else {
				CHECK(tfx_glDisable(GL_BLEND));
			}

			bool write_rgb = draw.flags & TFX_STATE_RGB_WRITE;
			bool write_alpha = draw.flags & TFX_STATE_ALPHA_WRITE;
			CHECK(tfx_glColorMask(write_rgb, write_rgb, write_rgb, write_alpha));

			if ((view->flags & TFX_VIEW_SCISSOR) || draw.use_scissor) {
				CHECK(tfx_glEnable(GL_SCISSOR_TEST));
				tfx_rect rect = view->scissor_rect;
				if (draw.use_scissor) {
					rect = draw.scissor_rect;
				}
				CHECK(tfx_glScissor(rect.x, canvas->height - rect.y - rect.h, rect.w, rect.h));
			}
			else {
				CHECK(tfx_glDisable(GL_SCISSOR_TEST));
			}

			int nu = sb_count(draw.uniforms);
			for (int j = 0; j < nu; j++) {
				tfx_uniform uniform = draw.uniforms[j];
				GLint loc = CHECK(tfx_glGetUniformLocation(program, uniform.name));
				if (loc < 0) {
					continue;
				}
				switch (uniform.type) {
					case TFX_UNIFORM_INT:   CHECK(tfx_glUniform1i(loc, *uniform.idata)); break;
					case TFX_UNIFORM_FLOAT: CHECK(tfx_glUniform1f(loc, *uniform.fdata)); break;
					case TFX_UNIFORM_VEC2:  CHECK(tfx_glUniform2fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_VEC3:  CHECK(tfx_glUniform3fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_VEC4:  CHECK(tfx_glUniform4fv(loc, uniform.count, uniform.fdata)); break;
					case TFX_UNIFORM_MAT2:  CHECK(tfx_glUniformMatrix2fv(loc, uniform.count, 0, uniform.fdata)); break;
					case TFX_UNIFORM_MAT3:  CHECK(tfx_glUniformMatrix3fv(loc, uniform.count, 0, uniform.fdata)); break;
					case TFX_UNIFORM_MAT4:  CHECK(tfx_glUniformMatrix4fv(loc, uniform.count, 0, uniform.fdata)); break;
					default: assert(false); break;
				}
			}

			if (draw.callback != NULL) {
				draw.callback();
			}

			if (!draw.use_vbo) {
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

			GLuint vbo = draw.vbo.gl_id;
			assert(vbo != 0);

			if (draw.vbo.dirty && tfx_glMemoryBarrier) {
				CHECK(tfx_glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT));
				draw.vbo.dirty = false;
			}

			uint32_t va_offset = 0;
			if (draw.use_tvb) {
				draw.vbo.format = draw.tvb_fmt;
				va_offset = draw.offset;
			}
			tfx_vertex_format *fmt = &draw.vbo.format;
			assert(fmt != NULL);
			assert(fmt->stride > 0);

			CHECK(tfx_glBindBuffer(GL_ARRAY_BUFFER, vbo));

			int nc = fmt->count;
			assert(nc < 8); // the mask is only 8 bits

			int real = 0;
			for (int i = 0; i < nc; i++) {
				if ((fmt->component_mask & (1 << i)) == 0) {
					continue;
				}
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
				CHECK(tfx_glEnableVertexAttribArray(real));
				CHECK(tfx_glVertexAttribPointer(real, (GLint)vc.size, gl_type, vc.normalized, (GLsizei)fmt->stride, (GLvoid*)(vc.offset + va_offset)));
				real += 1;
			}
			nc = last_count - nc;
			for (int i = 0; i <= nc; i++) {
				CHECK(tfx_glDisableVertexAttribArray(last_count - i));
			}
			last_count = real;

			for (int i = 0; i < 8; i++) {
				tfx_texture *tex = draw.textures[i];
				if (tex != NULL) {
					CHECK(tfx_glActiveTexture(GL_TEXTURE0 + i));
					CHECK(tfx_glBindTexture(GL_TEXTURE_2D, tex->gl_id));
					assert(tex->gl_id > 0);
				}
			}

			if (draw.use_ibo) {
				if (draw.ibo.dirty && tfx_glMemoryBarrier) {
					CHECK(tfx_glMemoryBarrier(GL_ELEMENT_ARRAY_BARRIER_BIT));
					draw.ibo.dirty = false;
				}
				CHECK(tfx_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw.ibo.gl_id));
				if (tfx_glDrawElementsInstanced) {
					CHECK(tfx_glDrawElementsInstanced(mode, draw.indices, GL_UNSIGNED_SHORT, (GLvoid*)draw.offset, 1));
				}
				else {
					CHECK(tfx_glDrawElements(mode, draw.indices, GL_UNSIGNED_SHORT, (GLvoid*)draw.offset));
				}
			}
			else {
				if (tfx_glDrawArraysInstanced) {
					CHECK(tfx_glDrawArraysInstanced(mode, 0, (GLsizei)draw.indices, 1));
				}
				else {
					CHECK(tfx_glDrawArrays(mode, 0, (GLsizei)draw.indices));
				}
			}

			sb_free(draw.uniforms);
		}

		sb_free(view->jobs);
		view->jobs = NULL;

		sb_free(view->draws);
		view->draws = NULL;

		sb_free(view->blits);
		view->blits = NULL;

		pop_group();
	}

	reset();

	tvb_reset();

	sb_free(g_uniforms);
	g_uniforms = NULL;

	g_ub_cursor = g_uniform_buffer;

	CHECK(tfx_glDisable(GL_SCISSOR_TEST));
	CHECK(tfx_glColorMask(true, true, true, true));

	if (tfx_glDeleteVertexArrays) {
		CHECK(tfx_glDeleteVertexArrays(1, &vao));
	}

	return stats;
}
#undef MAX_VIEW
#undef CHECK

#endif // TFX_IMPLEMENTATION
