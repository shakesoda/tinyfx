#include "tinyfx.h"
#include "demo_util.h"
#include <math.h>

struct {
	tfx_program prog;
	tfx_uniform world_from_screen; // inverse(p*v)
	tfx_uniform sun_params;
	float aspect;
	float proj[16];
	int pitch;
	int yaw;
} sky_res;

void sky_init(uint16_t w, uint16_t h) {
	sky_res.pitch = 0;
	sky_res.yaw = 0;
	sky_res.aspect = (float)w / (float)h;

	const uint8_t back = 1;
	tfx_view_set_clear_color(back, 0x555555ff);
	tfx_view_set_name(back, "Sky Pass");

	const char *src = demo_read_file("examples/02-sky.glsl");
	const char *attribs[] = { "a_position", NULL };

	sky_res.prog = tfx_program_new(src, src, attribs, -1);

	sky_res.world_from_screen = tfx_uniform_new("u_world_from_screen", TFX_UNIFORM_MAT4, 1);
	sky_res.sun_params = tfx_uniform_new("u_sun_params", TFX_UNIFORM_VEC4, 1);

	mat4_projection(sky_res.proj, 140.0f, sky_res.aspect, 0.1f, 1000.0f, false);
}

void sky_frame(int mx, int my) {
	const uint8_t back = 1;

	sky_res.pitch += my;
	sky_res.yaw   -= mx;

	const float sensitivity = 0.1f;
	const float limit = 3.1415962f * 0.5f;
	const float pitch = clamp(to_rad((float)sky_res.pitch * sensitivity), -limit, limit);
	const float yaw   = to_rad((float)sky_res.yaw * sensitivity);

	const float eye[3] = { 0.0f, 0.0f, 0.0f };
	float at[3] = {
		eye[0] + cosf(yaw),
		eye[1] + sinf(yaw),
		eye[2] + sinf(pitch)
	};
	const float up[3] = { 0.0f, 0.0f, 1.0f };
	float view[16];
	mat4_lookat(view, at, eye, up);

	float tmp[16], inv[16];
	mat4_mul(tmp, view, sky_res.proj);
	mat4_invert(inv, tmp);
	tfx_set_uniform(&sky_res.world_from_screen, inv, 1);

	float dir[3] = { 0.707f, 0.0f, 0.707f };
	float sun[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec_norm(sun, dir, 3);
	tfx_set_uniform(&sky_res.sun_params, sun, 1);

	tfx_transient_buffer tb = demo_screen_triangle(1.0f);
	tfx_set_transient_buffer(tb);
	tfx_set_state(TFX_STATE_RGB_WRITE | TFX_STATE_DEPTH_WRITE);
	tfx_submit(back, sky_res.prog, false);
	tfx_frame();
}

void sky_deinit() {}
