precision highp float;

#ifdef VERTEX
	uniform vec4 u_sun_params;
	#define time (u_sun_params.w)
	uniform mat4 u_screen_from_view;
	uniform mat4 u_view_from_world;
	uniform mat4 u_world_from_local;

	in vec4 screen_vertex;

	out vec2 local_uv;
	out vec3 view_dir_vs;
	out vec3 view_dir_ws;
	out mat3 view_from_world;
	out float far_clip;
#endif

#ifdef PIXEL
	uniform vec4 u_sun_params;
	#define sun_direction (u_sun_params.xyz)
	#define time (u_sun_params.w)

	in vec2 local_uv;
	in vec3 view_dir_vs;
	in vec3 view_dir_ws;
	in mat3 view_from_world;
	in float far_clip;

	out vec4 out_color;
#endif

// these 3 functions adapted from https://www.shadertoy.com/view/MdXyzX
vec2 wavedx(vec2 position, vec2 direction, float speed, float frequency, float timeshift) {
	float x = dot(direction, position) * frequency + timeshift * speed;
	float wave = exp(sin(x) - 1.0);
	float dx = wave * cos(x);
	return vec2(wave, -dx);
}

float getwaves(vec2 position) {
	const float drag_mult = 0.048;
	const int iterations = 40;
	// iter, w, ws state
	vec3 accum = vec3(0.0, 0.0, 0.0);
	// phase, speed, weight
	vec3 psw = vec3(6.0, 2.0, 1.0);
	for (int i=0; i < iterations; i++) {
		// note: vec2(sin(), cos()) will always be normalized
		vec2 p = vec2(sin(accum.x), cos(accum.x));
		vec2 res = wavedx(position, p, psw.y, psw.x, time);
		position += p * res.y * psw.z * drag_mult;
		accum += vec3(12.0, res.x * psw.z, psw.z);
		psw *= vec3(1.18, 1.07, 0.8);
	}
	return accum.y / accum.z;
}

vec3 normal(vec2 pos, float e, float depth) {
	vec2 ex = vec2(e * 0.1, 0.0);
	float H = getwaves(pos.xy * 0.1) * depth;
	vec3 a = vec3(pos.x, H, pos.y);
	return normalize(
		cross(
			(a-vec3(pos.x - e, getwaves(pos.xy * 0.1 - ex.xy) * depth, pos.y)),
			(a-vec3(pos.x, getwaves(pos.xy * 0.1 + ex.yx) * depth, pos.y + e))
		)
	);
}

#ifdef VERTEX
struct RayPlane {
	vec3 pos;
	vec3 dir;
};

vec4 ray_plane(RayPlane ray, RayPlane plane) {
	float t = dot(plane.pos - ray.pos, plane.dir) / dot(plane.dir, ray.dir);
	return mix(
		vec4(0.0, 0.0, 0.0, -1.0), // clip
		vec4(ray.pos + ray.dir * t, 1.0),
		step(t, 0.0)
	);
}

float deform(vec2 coord) {
	float size = 10.0;
	vec2 uv = coord / size;
	local_uv = uv;
	float depth = 1.2;
	float offset = depth * getwaves(uv);
	return mix(offset, 0.0, min(1.0, distance(coord / 250.0, vec2(0.0))));
}

vec2 correct_screen_uv(vec2 uv) {
	float overscan = 1.2;
	float squish = u_screen_from_view[0].x;
	float squash = u_screen_from_view[1].y;
	// edge case: square render target
	if (abs(squish - squash) < 0.01) {
		return uv * overscan * 1.1;
	}
	return vec2((uv.x - 0.5) * overscan * squash, (uv.y - 0.5) * overscan * squish);
}

void main() {
	vec2 screen_uv = screen_vertex.xy * 2.0 - 1.0;
	screen_uv = correct_screen_uv(screen_uv);
	screen_uv.y *= -1.0;

	RayPlane ray;
	mat3 rot = mat3(u_view_from_world);
	ray.pos = -u_view_from_world[3].xyz * rot;
	ray.dir = normalize(vec3(screen_uv, u_screen_from_view[0].x) * rot);

	RayPlane plane;
	plane.pos = (u_world_from_local * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	plane.dir = vec3(0.0, 0.0, 1.0);

	vec4 vertex = ray_plane(ray, plane);
	if (vertex.w < 0.0) {
		gl_Position = vertex;
		return;
	}
	vertex.z = deform(vertex.xy);

	view_dir_vs = -(u_view_from_world * vertex).xyz;

	view_dir_ws = transpose(mat3(u_view_from_world)) * -view_dir_vs;
	view_from_world = mat3(u_view_from_world);

	float near = (2.0 * u_screen_from_view[3][2]) / (2.0 * u_screen_from_view[2][2] - 2.0);
	far_clip = ((u_screen_from_view[2][2] - 1.0) * near) / (u_screen_from_view[2][2] + 1.0);

	gl_Position = u_screen_from_view * u_view_from_world * vertex;
}
#endif

#ifdef PIXEL
float square(float v) { return v*v; }
float cube(float v) { return v*v*v; }

vec3 extra_cheap_atmosphere(vec3 i_ws, vec3 sun_ws, float sdi) {
	sun_ws.z = max(sun_ws.z, -0.07);
	float special_trick = 1.0 / (i_ws.z * 1.0 + 0.125);
	float special_trick2 = 1.0 / (sun_ws.z * 11.0 + 1.0);
	float raysundt = square(abs(sdi));
	float sundt = pow(max(0.0, sdi), 8.0);
	float mymie = sundt * special_trick * 0.025;
	vec3 suncolor = mix(vec3(1.0), max(vec3(0.0), vec3(1.0) - vec3(5.5, 13.0, 22.4) / 22.4), special_trick2);
	vec3 bluesky= vec3(5.5, 13.0, 22.4) / 22.4 * suncolor;
	vec3 bluesky2 = max(vec3(0.0), bluesky - vec3(5.5, 13.0, 22.4) * 0.002 * (special_trick + -6.0 * sun_ws.z * sun_ws.z));
	bluesky2 *= special_trick * (0.4 + raysundt * 0.4);
	return max(vec3(0.0), bluesky2 * (1.0 + 1.0 * cube(1.0 - i_ws.z)) + mymie * suncolor);
}

vec3 sun(vec3 i_ws, vec3 sun_ws, float sdi) {
	vec3 sun_color = vec3(1000.0);
	float sun_angle = sdi;
	// normalize(vec3(6.0, 7.0, 8.0));
	vec3 halo = vec3(0.49153915231142, 0.57346234436333, 0.65538553641523);
	float halo_a = (sun_angle * 0.5 + 0.5, 1.0) * 0.125;
	float sun_size = 0.9999;
	float halo_b = (1.0-pow(smoothstep(sun_size, sun_size - 0.5, sun_angle), 0.025)) * 5.0;
	halo *= (0.00125 /*1.25 * 1.0/length(sun_color)*/) * (halo_a + halo_b);
	return sun_color * halo + sun_color * smoothstep(sun_size, 1.0, sun_angle);
}

vec3 sky_approx(vec3 i_ws, vec3 sun_ws) {
	float sdi = dot(sun_ws, i_ws);
	vec3 final = extra_cheap_atmosphere(i_ws, sun_ws, sdi);
	final += sun(i_ws, sun_ws, sdi);
	final = mix( // earth shadow
		final,
		vec3(3.0 * sun_ws.z) * vec3(0.3, 0.6, 1.0),
		smoothstep(0.25, -0.1, i_ws.z)
	);
	return final * 3.3635856610149; //exp2(1.75);
}

float schlick_ior_fresnel(float ior, float ldh) {
	float f0 = (ior-1.0)/(ior+1.0);
	f0 *= f0;
	float x = clamp(1.0-ldh, 0.0, 1.0);
	float x2 = x*x;
	return f0 + (1.0 - f0) * (x2*x2*x);
}

vec3 tonemap_aces(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

#ifdef GL_ES
vec4 linear_to_gamma(vec4 c) {
	c.rgb = pow(c.rgb, vec3(2.2));
	return c;
}
#else
vec4 linear_to_gamma(vec4 c) {
	bvec3 leq = lessThanEqual(c.rgb, vec3(0.04045));
	c.r = leq.r ? c.r / 12.92 : pow((c.r + 0.055) / 1.055, 2.4);
	c.g = leq.g ? c.g / 12.92 : pow((c.g + 0.055) / 1.055, 2.4);
	c.b = leq.b ? c.b / 12.92 : pow((c.b + 0.055) / 1.055, 2.4);
	return c;
}
#endif

void main() {
	vec3 n_ws = normal(local_uv, 0.001, 1.2).xzy;
	vec3 n_vs = (view_from_world*n_ws);
	vec3 i_vs = normalize(view_dir_vs);
	vec3 i_ws = normalize(view_dir_ws);
	float ldh = sqrt(max(0.05, dot(n_vs, i_vs)));
	float ndi = schlick_ior_fresnel(1.53, ldh);
	vec3 l_ws = normalize(sun_direction);
	vec3 final = mix(vec3(0.35, 0.50, 0.85), sky_approx(reflect(i_ws, n_ws), -l_ws).rgb, ndi);
	float fog = sqrt(min(1.0, length(view_dir_vs) / far_clip));
	final = mix(final, sky_approx(i_ws, -l_ws), fog);
	vec3 screen = final;
	//screen.rgb *= exp2(-1.75);
	screen.rgb *= 0.29730177875068;
	// normalize(vec3(5.0, 5.5, 5.25));
	screen.rgb /= vec3(0.54944225579476, 0.60438648137423, 0.5769143685845);
	screen.rgb = tonemap_aces(screen.rgb);// / tonemap_aces(vec3(1000.0));
	out_color = linear_to_gamma(vec4(screen, 1.0));
}
#endif
