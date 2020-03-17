#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum tfx_buffer_flags {
	TFX_BUFFER_NONE = 0,
	// for index buffers: use 32-bit instead of 16-bit indices.
	TFX_BUFFER_INDEX_32 = 1 << 0,
	// updated regularly (once per game tick)
	TFX_BUFFER_MUTABLE = 1 << 1,
	// temporary (updated many times per frame)
	// TFX_BUFFER_STREAM  = 1 << 2
} tfx_buffer_flags;

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
	TFX_STATE_WIREFRAME       = 1 << 13,

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
	TFX_TEXTURE_RESERVE_MIPS = 1 << 6,
	TFX_TEXTURE_CUBE = 1 << 7,
	TFX_TEXTURE_MSAA_SAMPLE = 1 << 8,
	TFX_TEXTURE_MSAA_X2 = 1 << 9,
	TFX_TEXTURE_MSAA_X4 = 1 << 10,
	TFX_TEXTURE_EXTERNAL = 1 << 11
};

typedef enum tfx_reset_flags {
	TFX_RESET_NONE = 0,
	TFX_RESET_MAX_ANISOTROPY = 1 << 0,
	TFX_RESET_REPORT_GPU_TIMINGS = 1 << 1,
	// a basic text/image overlay for debugging.
	// be aware that it's pretty slow, since it just plots pixels on cpu.
	// you probably don't want to leave this on if you're not using it!
	TFX_RESET_DEBUG_OVERLAY = 1 << 2,
	TFX_RESET_DEBUG_OVERLAY_STATS = 1 << 3
	// TFX_RESET_VR
} tfx_reset_flags;

typedef enum tfx_view_flags {
	TFX_VIEW_NONE = 0,
	TFX_VIEW_INVALIDATE = 1 << 0,
	TFX_VIEW_FLUSH = 1 << 1,
	TFX_VIEW_SORT_SEQUENTIAL = 1 << 2,
	TFX_VIEW_DEFAULT = TFX_VIEW_SORT_SEQUENTIAL
} tfx_view_flags;

typedef enum tfx_format {
	// color only
	TFX_FORMAT_RGB565 = 0,
	TFX_FORMAT_RGBA8,

	TFX_FORMAT_SRGB8,
	TFX_FORMAT_SRGB8_A8,

	TFX_FORMAT_RGB10A2,
	TFX_FORMAT_RG11B10F,
	TFX_FORMAT_RGB16F,
	TFX_FORMAT_RGBA16F,

	// color + depth
	TFX_FORMAT_RGB565_D16,
	TFX_FORMAT_RGBA8_D16,
	TFX_FORMAT_RGBA8_D24,

	// TFX_FORMAT_RGB10A2_D16,
	// TFX_FORMAT_RG11B10F_D16,
	// TFX_FORMAT_RGBA16F_D16,

	// single channel
	TFX_FORMAT_R16F,
	TFX_FORMAT_R32UI,
	TFX_FORMAT_R32F,

	// two channels
	TFX_FORMAT_RG16F,
	TFX_FORMAT_RG32F,

	// depth only
	TFX_FORMAT_D16,
	TFX_FORMAT_D24,
	TFX_FORMAT_D32,
	TFX_FORMAT_D32F,
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
	unsigned gl_msaa_id;
	unsigned gl_idx, gl_count;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t mip_count;
	tfx_format format;
	bool is_depth;
	bool is_stencil;
	bool dirty;
	uint16_t flags, _pad0;
	void *internal;
} tfx_texture;

typedef struct tfx_canvas {
	// 0 = normal, 1 = msaa
	unsigned gl_fbo[2];
	tfx_texture attachments[8];
	uint32_t allocated;
	uint16_t width;
	uint16_t height;
	uint16_t current_width;
	uint16_t current_height;
	int current_mip;
	//bool mipmaps;
	bool msaa;
	bool cube;
	bool own_attachments;
	bool reconfigure;
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
	tfx_buffer_flags flags;
	tfx_vertex_format format;
	void *internal;
} tfx_buffer;

typedef struct tfx_transient_buffer {
	bool has_format;
	tfx_vertex_format format;
	void *data;
	uint16_t num;
	uint32_t offset;
} tfx_transient_buffer;

typedef void (*tfx_draw_callback)(void);

typedef struct tfx_timing_info {
	uint64_t time;
	uint8_t id, _pad0[3];
	const char *name;
} tfx_timing_info;

typedef struct tfx_stats {
	uint32_t draws;
	uint32_t blits;
	uint32_t num_timings;
	tfx_timing_info *timings;
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
	bool multibind;
} tfx_caps;

// TODO
// #define TFX_API __attribute__ ((visibility("default")))
#define TFX_API

// bg_fg: 2x 8 bit palette colors. standard palette matches vga.
// bg_fg = (bg << 8) | fg, assuming little endian
TFX_API void tfx_debug_print(const int baserow, const int basecol, const uint16_t bg_fg, const int auto_wrap, const char *str);
TFX_API void tfx_debug_blit_rgba(const int x, const int y, const int w, const int h, const uint32_t *pixels);
// each pixel is an 8-bit palette index (standard palette matches vga, shared with text)
TFX_API void tfx_debug_blit_pal(const int x, const int y, const int w, const int h, const uint8_t *pixels);
// note: doesn't copy! make sure to keep this palette in memory yourself.
// pass NULL to reset to default palette. expects ABGR input.
TFX_API void tfx_debug_set_palette(const uint32_t *palette);

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

TFX_API tfx_buffer tfx_buffer_new(const void *data, size_t size, tfx_vertex_format *format, tfx_buffer_flags flags);
TFX_API void tfx_buffer_update(tfx_buffer *buf, const void *data, uint32_t offset, uint32_t size);
TFX_API void tfx_buffer_free(tfx_buffer *buf);

TFX_API tfx_texture tfx_texture_new(uint16_t w, uint16_t h, uint16_t layers, const void *data, tfx_format format, uint16_t flags);
TFX_API void tfx_texture_update(tfx_texture *tex, const void *data);
TFX_API void tfx_texture_free(tfx_texture *tex);
TFX_API tfx_texture tfx_get_texture(tfx_canvas *canvas, uint8_t index);

TFX_API tfx_canvas tfx_canvas_new(uint16_t w, uint16_t h, tfx_format format, uint16_t flags);
TFX_API void tfx_canvas_free(tfx_canvas *c);
TFX_API tfx_canvas tfx_canvas_attachments_new(bool claim_attachments, int count, tfx_texture *attachments);

TFX_API void tfx_view_set_name(uint8_t id, const char *name);
TFX_API void tfx_view_set_canvas(uint8_t id, tfx_canvas *canvas, int layer);
TFX_API void tfx_view_set_flags(uint8_t id, tfx_view_flags flags);
TFX_API void tfx_view_set_clear_color(uint8_t id, unsigned color);
TFX_API void tfx_view_set_clear_depth(uint8_t id, float depth);
TFX_API void tfx_view_set_depth_test(uint8_t id, tfx_depth_test mode);
TFX_API void tfx_view_set_scissor(uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
// order: xywh, in pixels
TFX_API void tfx_view_set_viewports(uint8_t id, int count, uint16_t **viewports);
TFX_API void tfx_view_set_instance_mul(uint8_t id, unsigned factor);
TFX_API tfx_canvas *tfx_view_get_canvas(uint8_t id);
TFX_API uint16_t tfx_view_get_width(uint8_t id);
TFX_API uint16_t tfx_view_get_height(uint8_t id);
TFX_API void tfx_view_get_dimensions(uint8_t id, uint16_t *w, uint16_t *h);
// TFX_API void tfx_view_set_transform(uint8_t id, float *view, float *proj_l, float *proj_r);

// you may pass -1 for attrib_count to use a null-terminated list for attribs
TFX_API tfx_program tfx_program_new(const char *vss, const char *fss, const char *attribs[], const int attrib_count);
// you may pass -1 for attrib_count to use a null-terminated list for attribs
TFX_API tfx_program tfx_program_len_new(const char *vss, const int _vs_len, const char *fss, const int _fs_len, const char *attribs[], const int attrib_count);
// you may pass -1 for attrib_count to use a null-terminated list for attribs
TFX_API tfx_program tfx_program_gs_len_new(const char *_gss, const int _gs_len, const char *_vss, const int _vs_len, const char *_fss, const int _fs_len, const char *attribs[], const int attrib_count);
// you may pass -1 for attrib_count to use a null-terminated list for attribs
TFX_API tfx_program tfx_program_gs_new(const char *gss, const char *vss, const char *fss, const char *attribs[], const int attrib_count);
TFX_API tfx_program tfx_program_cs_len_new(const char *css, const int _cs_len);
TFX_API tfx_program tfx_program_cs_new(const char *css);
// TODO: add tfx_program_free(tfx_program). they are currently cleaned up with tfx_shutdown.

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
TFX_API void tfx_set_image(tfx_uniform *uniform, tfx_texture *tex, uint8_t slot, uint8_t mip, bool write);
TFX_API void tfx_set_vertices(tfx_buffer *vbo, int count);
TFX_API void tfx_set_indices(tfx_buffer *ibo, int count, int offset);
TFX_API void tfx_dispatch(uint8_t id, tfx_program program, uint32_t x, uint32_t y, uint32_t z);
// TFX_API void tfx_submit_ordered(uint8_t id, tfx_program program, uint32_t depth, bool retain);
TFX_API void tfx_submit(uint8_t id, tfx_program program, bool retain);
// submit an empty draw. useful for using draw callbacks and ensuring views are processed.
TFX_API void tfx_touch(uint8_t id);

TFX_API void tfx_blit(uint8_t src, uint8_t dst, uint16_t x, uint16_t y, uint16_t w, uint16_t h, int mip);

TFX_API tfx_stats tfx_frame();

#undef TFX_API

#ifdef __cplusplus
}
#endif
