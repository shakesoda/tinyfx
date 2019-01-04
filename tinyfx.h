#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
	TFX_DEPTH_TEST_GT,
	TFX_DEPTH_TEST_EQ
} tfx_depth_test;

#define TFX_INVALID_BUFFER tfx_buffer { 0 }
#define TFX_INVALID_TRANSIENT_BUFFER tfx_transient_buffer { 0 }

// draw state
enum {
	// cull modes
	TFX_STATE_CULL_CW     = 1 << 0,
	TFX_STATE_CULL_CCW    = 1 << 1,

	// depth write
	TFX_STATE_DEPTH_WRITE = 1 << 2,
	TFX_STATE_RGB_WRITE   = 1 << 3,
	TFX_STATE_ALPHA_WRITE = 1 << 4,

	// blending
	TFX_STATE_BLEND_ALPHA = 1 << 5,

	// primitive modes
	TFX_STATE_DRAW_POINTS     = 1 << 6,
	TFX_STATE_DRAW_LINES      = 1 << 7,
	TFX_STATE_DRAW_LINE_STRIP = 1 << 8,
	TFX_STATE_DRAW_LINE_LOOP  = 1 << 9,
	TFX_STATE_DRAW_TRI_STRIP  = 1 << 10,
	TFX_STATE_DRAW_TRI_FAN    = 1 << 11,

	// misc state
	TFX_STATE_MSAA            = 1 << 12,

	TFX_STATE_DEFAULT = 0
		| TFX_STATE_CULL_CCW
		| TFX_STATE_MSAA
		| TFX_STATE_DEPTH_WRITE
		| TFX_STATE_RGB_WRITE
		| TFX_STATE_ALPHA_WRITE
		| TFX_STATE_BLEND_ALPHA
};

enum {
	TFX_CUBE_MAP_POSITIVE_X = 0,
	TFX_CUBE_MAP_NEGATIVE_X = 1,
	TFX_CUBE_MAP_POSITIVE_Y = 2,
	TFX_CUBE_MAP_NEGATIVE_Y = 3,
	TFX_CUBE_MAP_POSITIVE_Z = 4,
	TFX_CUBE_MAP_NEGATIVE_Z = 5
};

enum {
	TFX_TEXTURE_FILTER_POINT = 1 << 0,
	TFX_TEXTURE_FILTER_LINEAR  = 1 << 1,
	//TFX_TEXTURE_FILTER_ANISOTROPIC = 1 << 2,
	TFX_TEXTURE_CPU_WRITABLE = 1 << 3,
	// TFX_TEXTURE_GPU_WRITABLE = 1 << 4,
	TFX_TEXTURE_GEN_MIPS = 1 << 5,
	TFX_TEXTURE_CUBE = 1 << 6
};

typedef enum tfx_reset_flags {
	TFX_RESET_NONE = 0,
	TFX_RESET_MAX_ANISOTROPY = 1 << 0,
	// TFX_RESET_DEBUG...
	// TFX_RESET_VR
} tfx_reset_flags;

typedef enum tfx_format {
	// color only
	TFX_FORMAT_RGB565 = 0,
	TFX_FORMAT_RGBA8,

	TFX_FORMAT_RGB10A2,
	TFX_FORMAT_RG11B10F,
	TFX_FORMAT_RGBA16F,

	// color + depth
	TFX_FORMAT_RGB565_D16,
	TFX_FORMAT_RGBA8_D16,
	TFX_FORMAT_RGBA8_D24,

	// TFX_FORMAT_RGB10A2_D16,
	// TFX_FORMAT_RG11B10F_D16,
	// TFX_FORMAT_RGBA16F_D16,

	// depth only
	TFX_FORMAT_D16,
	TFX_FORMAT_D24,
	//TFX_FORMAT_D24_S8
} tfx_format;

typedef unsigned tfx_program;

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

typedef enum tfx_severity {
	TFX_SEVERITY_INFO,
	TFX_SEVERITY_WARNING,
	TFX_SEVERITY_ERROR,
	TFX_SEVERITY_FATAL
} tfx_severity;

typedef struct tfx_platform_data {
	bool use_gles;
	int context_version;
	void* (*gl_get_proc_address)(const char*);
	void(*info_log)(const char* msg, tfx_severity level);
} tfx_platform_data;

typedef struct tfx_uniform {
	union {
		float   *fdata;
		int     *idata;
		uint8_t *data;
	};
	const char *name;
	tfx_uniform_type type;
	int count;
	int last_count;
	size_t size;
} tfx_uniform;

typedef struct tfx_texture {
	unsigned gl_ids[2];
	unsigned gl_idx, gl_count;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	tfx_format format;
	uint16_t flags, _pad0;
	void *internal;
} tfx_texture;

typedef struct tfx_canvas {
	unsigned gl_fbo;
	tfx_texture attachments[8];
	//unsigned gl_ids[8]; // limit: 2x msaa + 2x non-msaa
	uint32_t allocated;
	uint16_t width;
	uint16_t height;
	//tfx_format format;
	//bool mipmaps;
	bool cube;
	//void *user_data;
	bool own_attachments;
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
	// limit to 8, since we only have an 8 bit mask
	tfx_vertex_component components[8];
	uint8_t count, component_mask, _pad0[2];
	size_t stride;
} tfx_vertex_format;

typedef struct tfx_buffer {
	unsigned gl_id;
	bool dirty;
	bool has_format;
	tfx_vertex_format format;
} tfx_buffer;

typedef struct tfx_transient_buffer {
	bool has_format;
	tfx_vertex_format format;
	void *data;
	uint16_t num;
	uint32_t offset;
} tfx_transient_buffer;

typedef void (*tfx_draw_callback)(void);

typedef struct tfx_stats {
	uint32_t draws;
	uint32_t blits;
} tfx_stats;

typedef struct tfx_caps {
	bool compute;
	bool float_canvas;
	bool multisample;
	bool debug_marker;
	bool debug_output;
	bool memory_info;
	bool instancing;
	bool seamless_cubemap;
	bool anisotropic_filtering;
} tfx_caps;

// TODO
// #define TFX_API __attribute__ ((visibility("default")))
#define TFX_API

TFX_API void tfx_set_platform_data(tfx_platform_data pd);

TFX_API tfx_caps tfx_get_caps();
TFX_API void tfx_dump_caps();
TFX_API void tfx_reset(uint16_t width, uint16_t height, tfx_reset_flags flags);
TFX_API void tfx_shutdown();

TFX_API tfx_vertex_format tfx_vertex_format_start();
TFX_API void tfx_vertex_format_add(tfx_vertex_format *fmt, uint8_t slot, size_t count, bool normalized, tfx_component_type type);
TFX_API void tfx_vertex_format_end(tfx_vertex_format *fmt);
TFX_API size_t tfx_vertex_format_offset(tfx_vertex_format *fmt, uint8_t slot);

TFX_API uint32_t tfx_transient_buffer_get_available(tfx_vertex_format *fmt);
TFX_API tfx_transient_buffer tfx_transient_buffer_new(tfx_vertex_format *fmt, uint16_t num_verts);

TFX_API tfx_buffer tfx_buffer_new(void *data, size_t size, tfx_vertex_format *format, tfx_buffer_usage usage);

TFX_API tfx_texture tfx_texture_new(uint16_t w, uint16_t h, uint16_t layers, void *data, tfx_format format, uint16_t flags);
TFX_API void tfx_texture_update(tfx_texture *tex, void *data);
TFX_API void tfx_texture_free(tfx_texture *tex);
TFX_API tfx_texture tfx_get_texture(tfx_canvas *canvas, uint8_t index);

TFX_API tfx_canvas tfx_canvas_new(uint16_t w, uint16_t h, tfx_format format, uint16_t flags);
TFX_API tfx_canvas tfx_canvas_attachments_new(bool claim_attachments, int count, tfx_texture *attachments);

TFX_API void tfx_view_set_name(uint8_t id, const char *name);
TFX_API void tfx_view_set_canvas(uint8_t id, tfx_canvas *canvas, int layer);
TFX_API void tfx_view_set_clear_color(uint8_t id, unsigned color);
TFX_API void tfx_view_set_clear_depth(uint8_t id, float depth);
TFX_API void tfx_view_set_depth_test(uint8_t id, tfx_depth_test mode);
TFX_API void tfx_view_set_scissor(uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
// order: xywh, in pixels
TFX_API void tfx_view_set_viewports(uint8_t id, int count, uint16_t **viewports);
TFX_API void tfx_view_set_instance_mul(uint8_t id, unsigned factor);
TFX_API uint16_t tfx_view_get_width(uint8_t id);
TFX_API uint16_t tfx_view_get_height(uint8_t id);
TFX_API void tfx_view_get_dimensions(uint8_t id, uint16_t *w, uint16_t *h);
// TFX_API void tfx_view_set_transform(uint8_t id, float *view, float *proj_l, float *proj_r);

TFX_API tfx_program tfx_program_new(const char *vss, const char *fss, const char *attribs[]);
TFX_API tfx_program tfx_program_gs_new(const char *gss, const char *vss, const char *fss, const char *attribs[]);
TFX_API tfx_program tfx_program_cs_new(const char *css);

TFX_API tfx_uniform tfx_uniform_new(const char *name, tfx_uniform_type type, int count);

// TFX_API void tfx_set_transform(float *mtx, uint8_t count);
TFX_API void tfx_set_transient_buffer(tfx_transient_buffer tb);
// pass -1 to update maximum uniform size
TFX_API void tfx_set_uniform(tfx_uniform *uniform, const float *data, const int count);
// pass -1 to update maximum uniform size
TFX_API void tfx_set_uniform_int(tfx_uniform *uniform, const int *data, const int count);
TFX_API void tfx_set_callback(tfx_draw_callback cb);
TFX_API void tfx_set_state(uint64_t flags);
TFX_API void tfx_set_scissor(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
TFX_API void tfx_set_texture(tfx_uniform *uniform, tfx_texture *tex, uint8_t slot);
TFX_API void tfx_set_buffer(tfx_buffer *buf, uint8_t slot, bool write);
// TFX_API void tfx_set_image(tfx_texture *tex, uint8_t slot, bool write);
TFX_API void tfx_set_vertices(tfx_buffer *vbo, int count);
TFX_API void tfx_set_indices(tfx_buffer *ibo, int count);
TFX_API void tfx_dispatch(uint8_t id, tfx_program program, uint32_t x, uint32_t y, uint32_t z);
// TFX_API void tfx_submit_ordered(uint8_t id, tfx_program program, uint32_t depth, bool retain);
TFX_API void tfx_submit(uint8_t id, tfx_program program, bool retain);
TFX_API void tfx_touch(uint8_t id);

TFX_API void tfx_blit(uint8_t src, uint8_t dst, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

TFX_API tfx_stats tfx_frame();

#undef TFX_API

#ifdef __cplusplus
}
#endif
