#version 320 es
precision highp float;
precision highp sampler2D;
in vec2 vUv;
uniform sampler2D tex;
out vec4 FragColor;
void main() { FragColor = texture(tex, vUv); }
