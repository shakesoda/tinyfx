#pragma once

#include <tinyfx.h>
#include <string>
#include <vector>

namespace tfx {
	struct VertexFormat {
		tfx_vertex_format fmt;
		VertexFormat() {
			this->fmt = tfx_vertex_format_start();
		}
		inline void add(size_t count, uint8_t slot, bool normalized = false, tfx_component_type type = TFX_TYPE_FLOAT) {
			tfx_vertex_format_add(&this->fmt, slot, count, normalized, type);
		}
		inline void end() {
			tfx_vertex_format_end(&this->fmt);
		}
		inline size_t offset(uint8_t slot) {
			return tfx_vertex_format_offset(&this->fmt, slot);
		}
	};

	struct Buffer {
		tfx_buffer buffer;
		// Buffer(std::vector &data, VertexFormat fmt, tfx_buffer_usage usage = TFX_USAGE_STATIC) {
		// 	this->buffer = tfx_buffer_new(&data[0], data.size()*sizeof(T), &fmt.fmt, usage);
		// }
		Buffer(tfx_buffer &buf) {
			this->buffer = buf;
		}
	};

	struct TransientBuffer {
		tfx_transient_buffer tvb;
		TransientBuffer(VertexFormat &fmt, uint16_t num) {
			tvb = tfx_transient_buffer_new(&fmt.fmt, num);
		}
	};

	struct Uniform {
		tfx_uniform uniform;
		Uniform(const char *name, tfx_uniform_type type, int count = 1) {
			this->uniform = tfx_uniform_new(name, type, count);
		}
	};

	struct Canvas {
		tfx_canvas canvas;
		Canvas(uint16_t w, uint16_t h, tfx_format format = TFX_FORMAT_RGBA8_D16, uint16_t flags = TFX_TEXTURE_FILTER_POINT) {
			this->canvas = tfx_canvas_new(w, h, format, flags);
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
		// inline void set_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
		// 	tfx_view_set_rect(this->id, x, y, w, h);
		// }
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

	struct Texture {
		tfx_texture texture;
		Texture(uint16_t w, uint16_t h, void *data = NULL, tfx_format format = TFX_FORMAT_RGBA8, uint16_t flags = TFX_TEXTURE_FILTER_LINEAR) {
			this->texture = tfx_texture_new(w, h, data, format, flags);
		}
	};

	struct Program {
		tfx_program program;
		Program(std::string vss, std::string fss, const char *attribs[]) {
			this->program = tfx_program_new(vss.c_str(), fss.c_str(), attribs);
		}
	};

	inline void dump_caps() {
		tfx_dump_caps();
	}
	inline tfx_caps get_caps() {
		return tfx_get_caps();
	}
	inline uint32_t transient_buffer_get_available(const VertexFormat &fmt) {
		return tfx_transient_buffer_get_available((tfx_vertex_format*)&fmt.fmt);
	}
	inline void touch(uint8_t id) {
		tfx_touch(id);
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
	inline void set_uniform(Uniform &uniform, float data) {
		float tmp = data;
		tfx_set_uniform(&uniform.uniform, &tmp);
	}
	inline void set_uniform(Uniform &uniform, float *data) {
		tfx_set_uniform(&uniform.uniform, data);
	}
	inline void set_texture(Uniform &uniform, Texture &texture, uint8_t slot) {
		tfx_set_texture(&uniform.uniform, &texture.texture, slot);
	}
	inline void set_callback(tfx_draw_callback cb) {
		tfx_set_callback(cb);
	}
	inline void set_state(uint64_t flags) {
		tfx_set_state(flags);
	}
	inline void set_buffer(Buffer &buf, uint8_t slot, bool write = false) {
		tfx_set_buffer(&buf.buffer, slot, write);
	}
	inline void set_transient_buffer(TransientBuffer &tvb) {
		tfx_set_transient_buffer(tvb.tvb);
	}
	inline void set_vertices(Buffer &vbo, int count = 0) {
		tfx_set_vertices(&vbo.buffer, count);
	}
	inline void set_indices(Buffer &ibo, int count) {
		tfx_set_indices(&ibo.buffer, count);
	}
	inline void dispatch(uint8_t id, Program &program, uint32_t x, uint32_t y, uint32_t z) {
		tfx_dispatch(id, program.program, x, y, z);
	}
	inline void dispatch(View &view, Program &program, uint32_t x, uint32_t y, uint32_t z) {
		tfx_dispatch(view.id, program.program, x, y, z);
	}
	// inline void submit(uint8_t id, Program &program, uint32_t depth = 0, bool retain = false) {
	// 	tfx_submit_ordered(id, program.program, depth, retain);
	// }
	inline void submit(uint8_t id, Program &program, bool retain = false) {
		tfx_submit(id, program.program, retain);
	}
	inline void submit(View &view, Program &program, bool retain = false) {
		tfx_submit(view.id, program.program, retain);
	}
	// inline void blit(tfx_view *src, tfx_view *dst, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

} // tfx
