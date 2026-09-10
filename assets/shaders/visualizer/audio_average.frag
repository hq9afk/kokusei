#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
#ifndef TWOPI
#define TWOPI 6.28318530718
#endif
#define window(t, sz) (0.53836 - (0.46164 * cos(TWOPI * t / sz)))
uniform int avgFrames;
uniform sampler2D audioR0;
uniform sampler2D audioR1;
uniform sampler2D audioR2;
uniform sampler2D audioR3;
uniform sampler2D audioR4;
out vec4 FragColor;

void main() {
    float r = 0.0;
    r += window(float(0), float(avgFrames - 1)) * texelFetch(audioR0, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(1), float(avgFrames - 1)) * texelFetch(audioR1, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(2), float(avgFrames - 1)) * texelFetch(audioR2, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(3), float(avgFrames - 1)) * texelFetch(audioR3, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(4), float(avgFrames - 1)) * texelFetch(audioR4, ivec2(int(gl_FragCoord.x), 0), 0).r;
    FragColor.r = (r / float(avgFrames));
}
