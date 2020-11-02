#ifdef VERTEX
uniform mat4 u_screen_from_view;
uniform mat4 u_view_from_world;

in vec3 position;

out vec3 view_dir_ws;

void main() {
	vec4 vertex = vec4(position, 1.0);
	vertex *= 2.0;
	mat4 inv_vp = inverse(u_screen_from_view * mat4(mat3(u_view_from_world)));
	view_dir_ws = -(inv_vp * vec4(vertex.x, vertex.y, 1.0, 1.0)).xyz;
	vertex.z = 1.0; // force to the far plane
	gl_Position = vec4(vertex.xyz, 1.0);
}
#endif

#ifdef PIXEL
uniform vec4 u_sun_params;
#define sun_direction (u_sun_params.xyz)

in vec3 view_dir_ws;

out vec4 out_color;

// extra_cheap_atmosphere adapted from https://www.shadertoy.com/view/MdXyzX
vec3 extra_cheap_atmosphere(vec3 i_ws, vec3 sun_ws) {
	sun_ws.z = max(sun_ws.z, -0.07);
	float special_trick = 1.0 / (i_ws.z * 1.0 + 0.125);
	float special_trick2 = 1.0 / (sun_ws.z * 11.0 + 1.0);
	float raysundt = pow(abs(dot(sun_ws, i_ws)), 2.0);
	float sundt = pow(max(0.0, dot(sun_ws, i_ws)), 8.0);
	float mymie = sundt * special_trick * 0.025;
	vec3 suncolor = mix(vec3(1.0), max(vec3(0.0), vec3(1.0) - vec3(5.5, 13.0, 22.4) / 22.4), special_trick2);
	vec3 bluesky= vec3(5.5, 13.0, 22.4) / 22.4 * suncolor;
	vec3 bluesky2 = max(vec3(0.0), bluesky - vec3(5.5, 13.0, 22.4) * 0.002 * (special_trick + -6.0 * sun_ws.z * sun_ws.z));
	bluesky2 *= special_trick * (0.4 + raysundt * 0.4);
	return max(vec3(0.0), bluesky2 * (1.0 + 1.0 * pow(1.0 - i_ws.z, 3.0)) + mymie * suncolor);
}

vec3 sun(vec3 i_ws, vec3 sun_ws) {
	vec3 sun_color = vec3(1000.0);
	float sun_angle = dot(sun_ws, i_ws);
	vec3 halo = normalize(vec3(6.0, 7.0, 8.0));
	float halo_a = pow(sun_angle * 0.5 + 0.5, 1.0) * 0.125;
	float sun_size = 0.9999;
	float halo_b = (1.0-pow(smoothstep(sun_size, sun_size - 0.5, sun_angle), 0.025)) * 5.0;
	halo *= 1.25 * (1.0/length(sun_color)) * (halo_a + halo_b);
	return sun_color * halo + sun_color * smoothstep(sun_size, 1.0, sun_angle);
}

vec3 sky_approx(vec3 i_ws, vec3 sun_ws) {
	vec3 final = extra_cheap_atmosphere(i_ws, sun_ws);
	final += sun(i_ws, sun_ws);
	vec3 up = vec3(0.0, 0.0, 1.0);
	final = mix( // earth shadow
		final,
		vec3(2.0 * dot(sun_ws, up)),
		smoothstep(0.25, -0.1, dot(i_ws, up))
	);
	return final * exp2(1.75);
}

vec3 tonemap_aces(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

vec4 linear_to_gamma(vec4 c) {
	bvec3 leq = lessThanEqual(c.rgb, vec3(0.04045));
	c.r = leq.r ? c.r / 12.92 : pow((c.r + 0.055) / 1.055, 2.4);
	c.g = leq.g ? c.g / 12.92 : pow((c.g + 0.055) / 1.055, 2.4);
	c.b = leq.b ? c.b / 12.92 : pow((c.b + 0.055) / 1.055, 2.4);
	return c;
}

void main() {
	vec3 sun_ws = normalize(-sun_direction);
	vec3 i_ws = normalize(-view_dir_ws);
	vec3 final = sky_approx(i_ws, sun_ws);
	vec3 screen = final;
	screen.rgb *= exp2(-1.75);
	screen.rgb /= normalize(vec3(5.0, 5.5, 5.25));
	screen.rgb = tonemap_aces(screen.rgb) / tonemap_aces(vec3(1000.0));
	out_color = linear_to_gamma(vec4(screen, 1.0));
}
#endif
