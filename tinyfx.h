#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum tfx_buffer_usage {
	// only occasionally changed, if never
	TFX_USAGE_STATIC = 0,
	// updated regularly (once per game tick)
	TFX_USAGE_DYNAMIC,
	// temporary (updated many times per frame)
	TFX_USAGE_STREAM
} tfx_buffer_usage;

typedef enum tfx_depth_test {
	TFX_DEPTH_TEST_NONE = 0,
	TFX_DEPTH_TEST_LT,
	TFX_DEPTH_TEST_GT
} tfx_depth_test;

// draw state
enum {
	// cull modes
	TFX_STATE_CULL_CW     = 1 << 0,
	TFX_STATE_CULL_CCW    = 1 << 1,

	// depth write
	TFX_STATE_DEPTH_WRITE = 1 << 2,

	// blending
	TFX_STATE_BLEND_ALPHA = 1 << 3,

	// primitive modes
	TFX_STATE_DRAW_POINTS     = 1 << 4,
	TFX_STATE_DRAW_LINES      = 1 << 5,
	TFX_STATE_DRAW_LINE_STRIP = 1 << 6,
	TFX_STATE_DRAW_LINE_LOOP  = 1 << 7,
	TFX_STATE_DRAW_TRI_STRIP  = 1 << 8,
	TFX_STATE_DRAW_TRI_FAN    = 1 << 9
};

typedef enum tfx_format {
	// color only
	TFX_FORMAT_RGB565 = 0,
	TFX_FORMAT_RGBA8,

	// color + depth
	TFX_FORMAT_RGB565_D16,
	TFX_FORMAT_RGBA8_D16,

	// depth only
	TFX_FORMAT_D16,
} tfx_format;

typedef GLuint tfx_program;

typedef enum tfx_uniform_type {
	TFX_UNIFORM_INT = 0,
	TFX_UNIFORM_FLOAT,
	TFX_UNIFORM_VEC2,
	TFX_UNIFORM_VEC3,
	TFX_UNIFORM_VEC4,
	TFX_UNIFORM_MAT2,
	TFX_UNIFORM_MAT3,
	TFX_UNIFORM_MAT4
} tfx_uniform_type;

typedef struct tfx_uniform {
	union {
		float   *fdata;
		int     *idata;
		uint8_t *data;
	};
	const char *name;
	tfx_uniform_type type;
	int count;
	size_t size;
} tfx_uniform;

typedef struct tfx_canvas {
	GLuint gl_id;
	uint16_t width;
	uint16_t height;
	tfx_format format;
} tfx_canvas;

typedef enum tfx_component_type {
	TFX_TYPE_FLOAT = 0,
	TFX_TYPE_BYTE,
	TFX_TYPE_UBYTE,
	TFX_TYPE_SHORT,
	TFX_TYPE_USHORT,
	TFX_TYPE_SKIP,
} tfx_component_type;

typedef struct tfx_vertex_component {
	size_t offset;
	size_t size;
	bool normalized;
	tfx_component_type type;
} tfx_vertex_component;

typedef struct tfx_vertex_format {
	tfx_vertex_component *components;
	size_t stride;
} tfx_vertex_format;

typedef struct tfx_buffer {
	GLuint gl_id;
	tfx_vertex_format *format;
} tfx_buffer;

typedef void (*tfx_draw_callback)(void);

typedef struct tfx_draw {
	tfx_draw_callback callback;
	uint32_t flags;

	tfx_program program;
	tfx_uniform *uniforms;

	tfx_buffer *vbo;
	tfx_buffer *ibo;

	size_t offset;
	uint16_t indices;
} tfx_draw;

typedef struct tfx_rect {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} tfx_rect;

typedef struct tfx_blit_op {
	tfx_canvas *source;
	tfx_rect rect;
} tfx_blit_op;

typedef struct tfx_stats {
	uint32_t draws;
	uint32_t blits;
} tfx_stats;

typedef struct tfx_caps {
	bool compute;
	bool float_canvas;
} tfx_caps;

// TODO
// #define TFX_API __attribute__ ((visibility("default")))
#define TFX_API

TFX_API tfx_caps tfx_dump_caps();
TFX_API void tfx_reset(uint16_t width, uint16_t height);
TFX_API void tfx_shutdown();

TFX_API tfx_vertex_format tfx_vertex_format_start();
TFX_API void tfx_vertex_format_add(tfx_vertex_format *fmt, size_t count, bool normalized, tfx_component_type type);
TFX_API void tfx_vertex_format_end(tfx_vertex_format *fmt);

TFX_API tfx_buffer tfx_buffer_new(void *data, size_t size, tfx_buffer_usage usage, tfx_vertex_format *format);

TFX_API tfx_canvas tfx_canvas_new(uint16_t w, uint16_t h, tfx_format format);

// TFX_API tfx_view tfx_view_new();
TFX_API void tfx_view_set_canvas(uint8_t id, tfx_canvas *canvas);
TFX_API void tfx_view_set_clear_color(uint8_t id, int color);
TFX_API void tfx_view_set_clear_depth(uint8_t id, float depth);
TFX_API void tfx_view_set_depth_test(uint8_t id, tfx_depth_test mode);
TFX_API void tfx_view_set_scissor(uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
TFX_API uint16_t tfx_view_get_width(uint8_t id);
TFX_API uint16_t tfx_view_get_height(uint8_t id);
TFX_API void tfx_view_get_dimensions(uint8_t id, uint16_t *w, uint16_t *h);

TFX_API tfx_program tfx_program_new(const char *vss, const char *fss, const char *attribs[]);

TFX_API tfx_uniform tfx_uniform_new(const char *name, tfx_uniform_type type, int count);
TFX_API void tfx_uniform_set_float(tfx_uniform *uniform, float *data);
TFX_API void tfx_uniform_set_int(tfx_uniform *uniform, int *data);

TFX_API void tfx_set_callback(tfx_draw_callback cb);
TFX_API void tfx_set_state(uint64_t flags);
TFX_API void tfx_set_vertices(tfx_buffer *vbo, int count);
TFX_API void tfx_set_indices(tfx_buffer *ibo, int count);
TFX_API void tfx_submit(uint8_t id, tfx_program program, bool retain);
TFX_API void tfx_touch(uint8_t id);

TFX_API void tfx_blit(uint8_t src, uint8_t dst, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

TFX_API tfx_stats tfx_frame();

#undef TFX_API

#ifdef __cplusplus
}
#endif
