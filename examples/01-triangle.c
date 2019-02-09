#include "tinyfx.h"
#include "demo_util.h"

struct {
	tfx_buffer vbo;
	tfx_program prog;
} tri_res;

void triangle_init(uint16_t w, uint16_t h) {
	const uint8_t back = 1;
	tfx_view_set_clear_color(back, 0x555555ff);
	tfx_view_set_clear_depth(back, 1.0);
	tfx_view_set_depth_test(back, TFX_DEPTH_TEST_LT);
	tfx_view_set_name(back, "Forward Pass");

	const char *vss = ""
		"in vec3 a_position;\n"
		"in vec4 a_color;\n"
		"out vec4 v_col;\n"
		"void main() {\n"
		"	v_col = a_color;\n"
		"	gl_Position = vec4(a_position.xyz, 1.0);\n"
		"}\n"
	;
	const char *fss = ""
		"precision mediump float;\n"
		"in vec4 v_col;\n"
		"void main() {\n"
		"	gl_FragColor = v_col;\n"
		"}\n"
	;
	const char *attribs[] = {
		"a_position",
		"a_color",
		NULL
	};

	tri_res.prog = tfx_program_new(vss, fss, attribs);

	float verts[] = {
		 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
	};

	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 0, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_add(&fmt, 1, 4, true, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	tri_res.vbo = tfx_buffer_new(verts, sizeof(verts), &fmt, TFX_BUFFER_NONE);
}

void triangle_frame() {
	const uint8_t back = 1;
	tfx_set_vertices(&tri_res.vbo, 3);
	tfx_set_state(0
		| TFX_STATE_RGB_WRITE
		| TFX_STATE_ALPHA_WRITE
		| TFX_STATE_BLEND_ALPHA
	);
	tfx_submit(back, tri_res.prog, false);
	tfx_frame();
}

void triangle_deinit() {
	tfx_buffer_free(&tri_res.vbo);
	// tfx_program_free(tri_res.prog); // NYI
}
