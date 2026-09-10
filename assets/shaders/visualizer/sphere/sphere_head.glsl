#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform vec2 resolution;
uniform sampler2D audioL;
uniform sampler2D audioR;
uniform float time;
uniform int audioLSize;
uniform int audioRSize;
layout(r32ui, binding = 0) uniform highp uimage2D atomicImageTexture0;
layout(r32ui, binding = 1) uniform highp uimage2D atomicImageTexture1;
layout(r32ui, binding = 2) uniform highp uimage2D atomicImageTexture2;
layout(r32ui, binding = 3) uniform highp uimage2D atomicImageTexture3;
layout(r32ui, binding = 4) uniform highp uimage2D atomicImageTexture4;
out vec4 FragColor;
#define AUDIO1D(t, x) texture(t, vec2((x), 0.5))
