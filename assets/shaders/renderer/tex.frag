#version 320 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec4 u_color;
out vec4 fragColor;
void main() { fragColor = texture(u_tex, v_uv) * u_color; }
