#pragma once

#include <tinyfx.h>
#include <string>

namespace tfx {
	struct VertexFormat {
		tfx_vertex_format fmt;
		VertexFormat() {
			this->fmt = tfx_vertex_format_start();
		}
		inline void add(size_t count, bool normalized = false, tfx_component_type type = TFX_TYPE_FLOAT) {
			tfx_vertex_format_add(&this->fmt, count, normalized, type);
		}
		inline void end() {
			tfx_vertex_format_end(&this->fmt);
		}
	};

	struct Buffer {
		tfx_buffer buffer;
		Buffer(void *data, size_t size, VertexFormat fmt, tfx_buffer_usage usage = TFX_USAGE_STATIC) {
			this->buffer = tfx_buffer_new(data, size, usage, &fmt.fmt);
		}
	};

	struct Uniform {
		tfx_uniform uniform;
		Uniform(std::string name, tfx_uniform_type type, int count = 1) {
			this->uniform = tfx_uniform_new(name.c_str(), type, count);
		}
		inline void set_float(float *data) {
			tfx_uniform_set_float(&this->uniform, data);
		}
		inline void set_int(int *data) {
			tfx_uniform_set_int(&this->uniform, data);
		}
	};

	struct Canvas {
		tfx_canvas canvas;
		Canvas(uint16_t w, uint16_t h, tfx_format format = TFX_FORMAT_RGBA8_D16) {
			this->canvas = tfx_canvas_new(w, h, format);
		}
	};

	struct View {
		uint8_t id;
		View(uint8_t _id) {
			this->id = _id;
		}
		inline void set_canvas(Canvas *canvas) {
			tfx_view_set_canvas(this->id, &canvas->canvas);
		}
		inline void set_clear_color(int color = 0x000000ff) {
			tfx_view_set_clear_color(this->id, color);
		}
		inline void set_clear_depth(float depth = 1.0f) {
			tfx_view_set_clear_depth(this->id, depth);
		}
		inline void set_depth_test(tfx_depth_test mode = TFX_DEPTH_TEST_NONE) {
			tfx_view_set_depth_test(this->id, mode);
		}
		inline void set_scissor(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
			tfx_view_set_scissor(this->id, x, y, w, h);
		}
		inline uint16_t get_width() {
			return tfx_view_get_width(this->id);
		}
		inline uint16_t get_height() {
			return tfx_view_get_width(this->id);
		}
		inline void get_dimensions(uint16_t *w, uint16_t *h) {
			tfx_view_get_dimensions(this->id, w, h);
		}
		inline void touch() {
			tfx_touch(this->id);
		}
	};

	struct Program {
		tfx_program program;
		Program(std::string vss, std::string fss, const char *attribs[]) {
			this->program = tfx_program_new(vss.c_str(), fss.c_str(), attribs);
		}
	};

	inline tfx_caps dump_caps() {
		return tfx_dump_caps();
	}
	inline void reset(uint16_t width, uint16_t height) {
		tfx_reset(width, height);
	}
	inline void shutdown() {
		tfx_shutdown();
	}
	inline tfx_stats frame() {
		return tfx_frame();
	}
	inline void set_callback(tfx_draw_callback cb) {
		tfx_set_callback(cb);
	}
	inline void set_state(uint64_t flags) {
		tfx_set_state(flags);
	}
	inline void set_vertices(Buffer *vbo, int count) {
		tfx_set_vertices(&vbo->buffer, count);
	}
	inline void set_indices(Buffer *ibo, int count) {
		tfx_set_indices(&ibo->buffer, count);
	}
	inline void submit(View *view, Program *program, bool retain = false) {
		tfx_submit(view->id, program->program, retain);
	}
	// inline void blit(tfx_view *src, tfx_view *dst, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

} // tfx
