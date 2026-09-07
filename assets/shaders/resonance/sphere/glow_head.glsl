#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform int postProcessingNumber;
uniform vec2 resolution;
uniform sampler2D audioL;
uniform sampler2D audioR;
uniform float time;
uniform int audioLSize;
uniform int audioRSize;
uniform sampler2D tex;
uniform float u_fade;
out vec4 FragColor;
#ifndef TWOPI
#define TWOPI 6.28318530718
#endif
#ifndef PI
#define PI 3.14159265359
#endif
#define RAD_PI PI / 180.
