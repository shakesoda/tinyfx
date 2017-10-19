#version 100

#ifdef GL_ES
#define out varying
#define in attribute
#endif

#define PSX_STYLE 0

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

in vec4 a_position;
// in vec2 a_texcoord0;
// in vec3 a_normal;
in vec4 a_color;

// out vec2 v_texcoord0;
// out vec3 v_normal;
out vec4 v_color;

vec4 transform(mat4 mvp, vec4 vertex) {
#if PSX_STYLE > 0
	vec4 v = mvp * vec4(vertex, 1.0);
	vec4 vv = v;
	vv.xyz = v.xyz / v.w;
	vv.x = floor(96.0 * vv.x) / 96.0;
	vv.y = floor(60.0 * vv.y) / 60.0;
	vv.xyz *= v.w;
	return vv;
#else
	return mvp * vertex;
#endif
}

void main() {
	mat4 xform = u_model; // * skeleton;

	// NB: A proper calculation for non-uniform scaling would be
	// transpose(inverse(m)), but we can't do that on GLES, and it'd be too
	// slow. Using the upper 3x3 is good enough.
	// v_normal = mat3(transform) * a_normal;
	v_color = a_color;
	// v_texcoord0 = a_texcoord0;

	mat4 mvp = mat4(1.0);
	// mat4 mvp = u_projection * u_view * xform;
	gl_Position = transform(mvp, a_position);
}
