#pragma once

#include <stdio.h>  // file stuff
#include <stdlib.h> // malloc
#include <math.h>
#include "tinyfx.h"

void *demo_read_file(const char *filename) {
	FILE *f = fopen(filename, "r");
	if (f) {
		fseek(f, 0L, SEEK_END);
		int bytes = ftell(f);
		rewind(f);
		char *buf = (char*)malloc(bytes+1);
		buf[bytes] = '\0';
		fread(buf, 1, bytes, f);
		fclose(f);

		return buf;
	}
	printf("Unable to open file \"%s\"\n", filename);
	return NULL;
}

tfx_transient_buffer demo_screen_triangle(float depth) {
	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 0, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	tfx_transient_buffer tb = tfx_transient_buffer_new(&fmt, 3);
	float *fdata = (float*)tb.data;
	fdata[0] = fdata[3] = fdata[4] = fdata[7] = -1.0f;
	fdata[2] = fdata[5] = fdata[8] = depth;
	fdata[1] = fdata[6] = 3.0f;
	return tb;
}

// various useful math
static float to_rad(const float deg) {
	return deg * (3.14159265358979323846f / 180.0f);
}

void mat4_projection(float out[16], float fovy, float aspect, float near, float far, bool infinite) {
	float t = tanf(to_rad(fovy) / 2.0f);
	float m22 = infinite ? 1.0f : -(far + near) / (far - near);
	float m23 = infinite ? 2.0f * near : -(2.0f * far * near) / (far - near);
	float m32 = infinite ? -1.0f : -1.0f;
	for (int i = 0; i < 16; i++) { out[i] = 0.0f; }
	out[0] = 1.0f / (t * aspect);
	out[5] = 1.0f / t;
	out[10] = m22;
	out[11] = m32;
	out[14] = m23;
}

float vec_dot(const float *a, const float *b, int len) {
	float sum = 0.0;
	for (int i = 0; i < len; i++) {
		sum += a[i] * b[i];
	}
	return sum;
}

float vec_len(const float *in, int len) {
	return sqrtf(vec_dot(in, in, len));
}

void vec_norm(float *out, const float *in, int len) {
	float vlen = vec_len(in, len);
	if (vlen == 0.0f) {
		for (int i = 0; i < len; i++) {
			out[i] = 0.0f;
		}
		return;
	}
	vlen = 1.0f / vlen;
	for (int i = 0; i < len; i++) {
		out[i] = in[i] * vlen;
	}
}

void vec3_cross(float *out, const float *a, const float *b) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

void mat4_lookat(float out[16], const float eye[3], const float at[3], const float up[3]) {
	float forward[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
	vec_norm(forward, forward, 3);

	float side[3];
	vec3_cross(side, forward, up);
	vec_norm(side, side, 3);

	float new_up[3];
	vec3_cross(new_up, side, forward);

	out[3] = out[7] = out[11] = 0.0f;
	out[0] = side[0];
	out[1] = new_up[0];
	out[2] = -forward[0];
	out[4] = side[1];
	out[5] = new_up[1];
	out[6] = -forward[1];
	out[8] = side[2];
	out[9] = new_up[2];
	out[10] = -forward[2];
	out[12] = -vec_dot(side, eye, 3);
	out[13] = -vec_dot(new_up, eye, 3);
	out[14] = vec_dot(forward, eye, 3);
	out[15] = 1.0f;
}

void mat4_mul(float out[16], const float a[16], const float b[16]) {
	out[0] = a[0]  * b[0] + a[1]  * b[4] + a[2]  * b[8]  + a[3]  * b[12];
	out[1] = a[0]  * b[1] + a[1]  * b[5] + a[2]  * b[9]  + a[3]  * b[13];
	out[2] = a[0]  * b[2] + a[1]  * b[6] + a[2]  * b[10] + a[3]  * b[14];
	out[3] = a[0]  * b[3] + a[1]  * b[7] + a[2]  * b[11] + a[3]  * b[15];
	out[4] = a[4]  * b[0] + a[5]  * b[4] + a[6]  * b[8]  + a[7]  * b[12];
	out[5] = a[4]  * b[1] + a[5]  * b[5] + a[6]  * b[9]  + a[7]  * b[13];
	out[6] = a[4]  * b[2] + a[5]  * b[6] + a[6]  * b[10] + a[7]  * b[14];
	out[7] = a[4]  * b[3] + a[5]  * b[7] + a[6]  * b[11] + a[7]  * b[15];
	out[8] = a[8]  * b[0] + a[9]  * b[4] + a[10] * b[8]  + a[11] * b[12];
	out[9] = a[8]  * b[1] + a[9]  * b[5] + a[10] * b[9]  + a[11] * b[13];
	out[10] = a[8]  * b[2] + a[9]  * b[6] + a[10] * b[10] + a[11] * b[14];
	out[11] = a[8]  * b[3] + a[9]  * b[7] + a[10] * b[11] + a[11] * b[15];
	out[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8]  + a[15] * b[12];
	out[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9]  + a[15] * b[13];
	out[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
	out[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
}

void mat4_invert(float out[16], const float a[16]) {
	float tmp[16];
	tmp[0]  =  a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
	tmp[1]  = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
	tmp[2]  =  a[1] * a[6]  * a[15] - a[1] * a[7]  * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7]  - a[13] * a[3] * a[6];
	tmp[3]  = -a[1] * a[6]  * a[11] + a[1] * a[7]  * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9]  * a[2] * a[7]  + a[9]  * a[3] * a[6];
	tmp[4]  = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
	tmp[5]  =  a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
	tmp[6]  = -a[0] * a[6]  * a[15] + a[0] * a[7]  * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7]  + a[12] * a[3] * a[6];
	tmp[7]  =  a[0] * a[6]  * a[11] - a[0] * a[7]  * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8]  * a[2] * a[7]  - a[8]  * a[3] * a[6];
	tmp[8]  =  a[4] * a[9]  * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
	tmp[9]  = -a[0] * a[9]  * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
	tmp[10] =  a[0] * a[5]  * a[15] - a[0] * a[7]  * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7]  - a[12] * a[3] * a[5];
	tmp[11] = -a[0] * a[5]  * a[11] + a[0] * a[7]  * a[9]  + a[4] * a[1] * a[11] - a[4] * a[3] * a[9]  - a[8]  * a[1] * a[7]  + a[8]  * a[3] * a[5];
	tmp[12] = -a[4] * a[9]  * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
	tmp[13] =  a[0] * a[9]  * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
	tmp[14] = -a[0] * a[5]  * a[14] + a[0] * a[6]  * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6]  + a[12] * a[2] * a[5];
	tmp[15] =  a[0] * a[5]  * a[10] - a[0] * a[6]  * a[9]  - a[4] * a[1] * a[10] + a[4] * a[2] * a[9]  + a[8]  * a[1] * a[6]  - a[8]  * a[2] * a[5];

	float det = a[0] * tmp[0] + a[1] * tmp[4] + a[2] * tmp[8] + a[3] * tmp[12];
	if (det == 0.0f) {
		for (int i = 0; i < 16; i++) {
			out[i] = a[i];
		}
	}
	det = 1.0f / det;

	for (int i = 0; i < 16; i++) {
		out[i] = tmp[i] * det;
	}
}

float clamp(float v, float low, float high) {
	return fmaxf(fminf(v, high), low);
}
