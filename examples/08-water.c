#include "tinyfx.h"
#include "demo_util.h"
#include <math.h>

/*
local function grid(width, height, subdivisions)
	local tiles = math.max(0, math.floor(subdivisions or 0)) + 1
	local verts = {}
	local function vertex(x, y)
		local x_pct = x / tiles
		local y_pct = y / tiles
		return {
			x_pct * width, y_pct * height, 0,
			0, 1, 0,
			x_pct, y_pct
		}
	end
	for y=1,tiles do
		for x=1,tiles do
			local a = vertex(x-1, y-1)
			local b = vertex(x-1, y)
			local c = vertex(x, y-1)
			local d = vertex(x, y)
			table.insert(verts, a)
			table.insert(verts, b)
			table.insert(verts, c)
			table.insert(verts, b)
			table.insert(verts, d)
			table.insert(verts, c)
		end
	end
	local fmt = {
		{ 'lovrPosition', 'float', 3 },
		{ 'lovrNormal',   'float', 3 },
		{ 'lovrTexCoord', 'float', 2 }
	}
	-- lovr is broken
	local indices = {}
	for i, _ in ipairs(verts) do
		table.insert(indices, i)
	end
	local m = lovr.graphics.newMesh(fmt, verts, "triangles", "static", false)
	m:setVertexMap(indices)
	return m
end

local ground = grid(1, 1, canvas:getWidth()/6)


// this function is broken in lovr 0.14.0
local function lookAt(eye, at, up)
	local z_axis=vec3(eye-at):normalize()
	local x_axis=vec3(up):cross(z_axis):normalize()
	local y_axis=vec3(z_axis):cross(x_axis)
	return lovr.math.newMat4(
		x_axis.x,y_axis.x,z_axis.x,0,
		x_axis.y,y_axis.y,z_axis.y,0,
		x_axis.z,y_axis.z,z_axis.z,0,
		-x_axis:dot(eye),-y_axis:dot(eye),-z_axis:dot(eye),1
	)
end

*/
struct {
	tfx_program prog;
	tfx_program sky;
	tfx_uniform world_from_local; // inverse(p*v)
	tfx_uniform screen_from_view;
	tfx_uniform view_from_world;
	tfx_uniform world_from_screen;
	tfx_uniform sun_params;
	tfx_buffer grid;
	int grid_vertices;
	float aspect;
	float proj[16];
	int pitch;
	int yaw;
	double start;
} water_res;

struct vert {
	float p[3];
};

void write_vertex(struct vert *out, float x, float y, float tiles) {
	out->p[0] = x / tiles;
	out->p[1] = y / tiles;
	out->p[2] = 0.0f;
}

void water_init(uint16_t w, uint16_t h) {
	water_res.start = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();

	water_res.pitch = 0;
	water_res.yaw = 0;
	water_res.aspect = (float)w / (float)h;

	const uint8_t back = 1;
	tfx_view_set_clear_color(back, 0x555555ff);
	tfx_view_set_clear_depth(back, 1.0f);
	tfx_view_set_depth_test(back, TFX_DEPTH_TEST_LT);
	tfx_view_set_name(back, "Water Pass");

	const char *attribs[] = { "a_position", NULL };

	const char *src = demo_read_file("examples/08-water.glsl");
	water_res.prog = tfx_program_new(src, src, attribs, -1);
	free(src);

	const char *src2 = demo_read_file("examples/08-water-sky.glsl");
	water_res.sky = tfx_program_new(src2, src2, attribs, -1);
	free(src2);

	water_res.world_from_local = tfx_uniform_new("u_world_from_local", TFX_UNIFORM_MAT4, 1);
	water_res.screen_from_view = tfx_uniform_new("u_screen_from_view", TFX_UNIFORM_MAT4, 1);
	water_res.view_from_world = tfx_uniform_new("u_view_from_world", TFX_UNIFORM_MAT4, 1);
	water_res.world_from_screen = tfx_uniform_new("u_world_from_screen", TFX_UNIFORM_MAT4, 1);

	water_res.sun_params = tfx_uniform_new("u_sun_params", TFX_UNIFORM_VEC4, 1);

	mat4_projection(water_res.proj, 55.0f, water_res.aspect, 0.1f, 1000.0f, false);

	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 0, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	float div = 6.0f;
	float tiles = fmaxf(0.0f, floorf((float)w / div)) + 1.0f;
	water_res.grid_vertices = tiles * tiles * 6;
	struct vert *verts = malloc(fmt.stride * water_res.grid_vertices);

	int i = 0;
	for (int y = 1; y < tiles; y++) {
		for (int x = 1; x < tiles; x++) {
			write_vertex(&verts[i++], x-1, y-1, tiles);
			write_vertex(&verts[i++], x-1, y, tiles);
			write_vertex(&verts[i++], x, y-1, tiles);

			write_vertex(&verts[i++], x-1, y, tiles);
			write_vertex(&verts[i++], x, y, tiles);
			write_vertex(&verts[i++], x, y-1, tiles);
		}
	}

	water_res.grid = tfx_buffer_new(
		verts,
		fmt.stride * water_res.grid_vertices,
		&fmt,
		TFX_BUFFER_NONE
	);

	free(verts);
}

void water_frame(int mx, int my) {
	const uint8_t back = 1;

	water_res.pitch += my;
	water_res.yaw   -= mx;

	const float sensitivity = 0.1f;
	const float limit = 3.1415962f * 0.5f;
	const float pitch = clamp(to_rad((float)water_res.pitch * sensitivity), -limit, limit);
	const float yaw   = to_rad((float)water_res.yaw * sensitivity);

	const float eye[3] = { 0.0f, 0.0f, 5.75f };
	float at[3] = {
		eye[0] + cosf(yaw),
		eye[1] + sinf(yaw),
		eye[2] + sinf(pitch)
	};
	const float up[3] = { 0.0f, 0.0f, 1.0f };
	float view[16];
	mat4_lookat(view, at, eye, up);

	float tmp[16], inv[16];
	mat4_mul(tmp, view, water_res.proj);
	mat4_invert(inv, tmp);

	float model[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	tfx_set_uniform(&water_res.world_from_local, model, 1);
	tfx_set_uniform(&water_res.view_from_world, view, 1);
	tfx_set_uniform(&water_res.screen_from_view, water_res.proj, 1);
	tfx_set_uniform(&water_res.world_from_screen, inv, 1);

	double now = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();

	float dir[3] = { 1.0f, 0.5f, -0.25f };
	float sun[4] = { 0.0f, 0.0f, 0.0f, (float)(now - water_res.start) };
	vec_norm(sun, dir, 3);
	tfx_set_uniform(&water_res.sun_params, sun, 1);

	tfx_set_transient_buffer(demo_screen_triangle(1.0f));
	tfx_set_state(TFX_STATE_RGB_WRITE);
	tfx_submit(back, water_res.sky, false);

	tfx_set_vertices(&water_res.grid, water_res.grid_vertices);
	tfx_set_state(TFX_STATE_RGB_WRITE | TFX_STATE_DEPTH_WRITE | TFX_STATE_CULL_CCW);
	tfx_submit(back, water_res.prog, false);

	tfx_frame();
}

void water_deinit() {
	tfx_buffer_free(&water_res.grid);
}
