#version 100

precision mediump float;

#ifdef GL_ES
#define in varying
#endif

in vec4 v_color;

void main() {
	gl_FragColor = v_color;
}
