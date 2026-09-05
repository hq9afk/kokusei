#pragma once

#include <string>

namespace resonance_shaders {

inline constexpr const char *kFsHeader = R"GLSL(#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
)GLSL";

inline constexpr const char *kFullscreenVs = R"GLSL(#version 320 es
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); }
)GLSL";

inline constexpr const char *kLygiaPnoise = R"GLSL(
#ifndef FNC_MOD289
#define FNC_MOD289
float mod289(const in float x) { return x - floor(x * (1. / 289.)) * 289.; }
vec2 mod289(const in vec2 x) { return x - floor(x * (1. / 289.)) * 289.; }
vec3 mod289(const in vec3 x) { return x - floor(x * (1. / 289.)) * 289.; }
vec4 mod289(const in vec4 x) { return x - floor(x * (1. / 289.)) * 289.; }
#endif
#ifndef FNC_PERMUTE
#define FNC_PERMUTE
float permute(const in float v) { return mod289(((v * 34.0) + 1.0) * v); }
vec2 permute(const in vec2 v) { return mod289(((v * 34.0) + 1.0) * v); }
vec3 permute(const in vec3 v) { return mod289(((v * 34.0) + 1.0) * v); }
vec4 permute(const in vec4 v) { return mod289(((v * 34.0) + 1.0) * v); }
#endif
#ifndef FNC_QUINTIC
#define FNC_QUINTIC
float quintic(const in float v) { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec2  quintic(const in vec2 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec3  quintic(const in vec3 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec4  quintic(const in vec4 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }
#endif
#ifndef FNC_TAYLORINVSQRT
#define FNC_TAYLORINVSQRT
float taylorInvSqrt(in float r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec2 taylorInvSqrt(in vec2 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec3 taylorInvSqrt(in vec3 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec4 taylorInvSqrt(in vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
#endif
#ifndef FNC_PNOISE
#define FNC_PNOISE
float pnoise(in vec2 P, in vec2 rep)
{
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, rep.xyxy);
    Pi = mod289(Pi);
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;
    vec4 i = permute(permute(ix) + iy);
    vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;
    vec2 g00 = vec2(gx.x, gy.x);
    vec2 g10 = vec2(gx.y, gy.y);
    vec2 g01 = vec2(gx.z, gy.z);
    vec2 g11 = vec2(gx.w, gy.w);
    vec4 norm = taylorInvSqrt(vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11)));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;
    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));
    vec2 fade_xy = quintic(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
    return 2.3 * n_xy;
}
float pnoise(in vec3 P, in vec3 rep)
{
    vec3 Pi0 = mod(floor(P), rep);
    vec3 Pi1 = mod(Pi0 + vec3(1.0), rep);
    Pi0 = mod289(Pi0);
    Pi1 = mod289(Pi1);
    vec3 Pf0 = fract(P);
    vec3 Pf1 = Pf0 - vec3(1.0);
    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.yy, Pi1.yy);
    vec4 iz0 = Pi0.zzzz;
    vec4 iz1 = Pi1.zzzz;
    vec4 ixy = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);
    vec4 gx0 = ixy0 * (1.0 / 7.0);
    vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
    gx0 = fract(gx0);
    vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
    vec4 sz0 = step(gz0, vec4(0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);
    vec4 gx1 = ixy1 * (1.0 / 7.0);
    vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
    gx1 = fract(gx1);
    vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);
    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);
    vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    g000 *= norm0.x;
    g010 *= norm0.y;
    g100 *= norm0.z;
    g110 *= norm0.w;
    vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
    g001 *= norm1.x;
    g011 *= norm1.y;
    g101 *= norm1.z;
    g111 *= norm1.w;
    float n000 = dot(g000, Pf0);
    float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
    float n111 = dot(g111, Pf1);
    vec3 fade_xyz = quintic(Pf0);
    vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
    vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
    float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
    return 2.2 * n_xyz;
}
float pnoise(in vec4 P, in vec4 rep)
{
    vec4 Pi0 = mod(floor(P), rep);
    vec4 Pi1 = mod(Pi0 + 1.0, rep);
    Pi0 = mod289(Pi0);
    Pi1 = mod289(Pi1);
    vec4 Pf0 = fract(P);
    vec4 Pf1 = Pf0 - 1.0;
    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.yy, Pi1.yy);
    vec4 iz0 = vec4(Pi0.zzzz);
    vec4 iz1 = vec4(Pi1.zzzz);
    vec4 iw0 = vec4(Pi0.wwww);
    vec4 iw1 = vec4(Pi1.wwww);
    vec4 ixy = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);
    vec4 ixy00 = permute(ixy0 + iw0);
    vec4 ixy01 = permute(ixy0 + iw1);
    vec4 ixy10 = permute(ixy1 + iw0);
    vec4 ixy11 = permute(ixy1 + iw1);
    vec4 gx00 = ixy00 * (1.0 / 7.0);
    vec4 gy00 = floor(gx00) * (1.0 / 7.0);
    vec4 gz00 = floor(gy00) * (1.0 / 6.0);
    gx00 = fract(gx00) - 0.5;
    gy00 = fract(gy00) - 0.5;
    gz00 = fract(gz00) - 0.5;
    vec4 gw00 = vec4(0.75) - abs(gx00) - abs(gy00) - abs(gz00);
    vec4 sw00 = step(gw00, vec4(0.0));
    gx00 -= sw00 * (step(0.0, gx00) - 0.5);
    gy00 -= sw00 * (step(0.0, gy00) - 0.5);
    vec4 gx01 = ixy01 * (1.0 / 7.0);
    vec4 gy01 = floor(gx01) * (1.0 / 7.0);
    vec4 gz01 = floor(gy01) * (1.0 / 6.0);
    gx01 = fract(gx01) - 0.5;
    gy01 = fract(gy01) - 0.5;
    gz01 = fract(gz01) - 0.5;
    vec4 gw01 = vec4(0.75) - abs(gx01) - abs(gy01) - abs(gz01);
    vec4 sw01 = step(gw01, vec4(0.0));
    gx01 -= sw01 * (step(0.0, gx01) - 0.5);
    gy01 -= sw01 * (step(0.0, gy01) - 0.5);
    vec4 gx10 = ixy10 * (1.0 / 7.0);
    vec4 gy10 = floor(gx10) * (1.0 / 7.0);
    vec4 gz10 = floor(gy10) * (1.0 / 6.0);
    gx10 = fract(gx10) - 0.5;
    gy10 = fract(gy10) - 0.5;
    gz10 = fract(gz10) - 0.5;
    vec4 gw10 = vec4(0.75) - abs(gx10) - abs(gy10) - abs(gz10);
    vec4 sw10 = step(gw10, vec4(0.0));
    gx10 -= sw10 * (step(0.0, gx10) - 0.5);
    gy10 -= sw10 * (step(0.0, gy10) - 0.5);
    vec4 gx11 = ixy11 * (1.0 / 7.0);
    vec4 gy11 = floor(gx11) * (1.0 / 7.0);
    vec4 gz11 = floor(gy11) * (1.0 / 6.0);
    gx11 = fract(gx11) - 0.5;
    gy11 = fract(gy11) - 0.5;
    gz11 = fract(gz11) - 0.5;
    vec4 gw11 = vec4(0.75) - abs(gx11) - abs(gy11) - abs(gz11);
    vec4 sw11 = step(gw11, vec4(0.0));
    gx11 -= sw11 * (step(0.0, gx11) - 0.5);
    gy11 -= sw11 * (step(0.0, gy11) - 0.5);
    vec4 g0000 = vec4(gx00.x, gy00.x, gz00.x, gw00.x);
    vec4 g1000 = vec4(gx00.y, gy00.y, gz00.y, gw00.y);
    vec4 g0100 = vec4(gx00.z, gy00.z, gz00.z, gw00.z);
    vec4 g1100 = vec4(gx00.w, gy00.w, gz00.w, gw00.w);
    vec4 g0010 = vec4(gx10.x, gy10.x, gz10.x, gw10.x);
    vec4 g1010 = vec4(gx10.y, gy10.y, gz10.y, gw10.y);
    vec4 g0110 = vec4(gx10.z, gy10.z, gz10.z, gw10.z);
    vec4 g1110 = vec4(gx10.w, gy10.w, gz10.w, gw10.w);
    vec4 g0001 = vec4(gx01.x, gy01.x, gz01.x, gw01.x);
    vec4 g1001 = vec4(gx01.y, gy01.y, gz01.y, gw01.y);
    vec4 g0101 = vec4(gx01.z, gy01.z, gz01.z, gw01.z);
    vec4 g1101 = vec4(gx01.w, gy01.w, gz01.w, gw01.w);
    vec4 g0011 = vec4(gx11.x, gy11.x, gz11.x, gw11.x);
    vec4 g1011 = vec4(gx11.y, gy11.y, gz11.y, gw11.y);
    vec4 g0111 = vec4(gx11.z, gy11.z, gz11.z, gw11.z);
    vec4 g1111 = vec4(gx11.w, gy11.w, gz11.w, gw11.w);
    vec4 norm00 = taylorInvSqrt(vec4(dot(g0000, g0000), dot(g0100, g0100), dot(g1000, g1000), dot(g1100, g1100)));
    g0000 *= norm00.x;
    g0100 *= norm00.y;
    g1000 *= norm00.z;
    g1100 *= norm00.w;
    vec4 norm01 = taylorInvSqrt(vec4(dot(g0001, g0001), dot(g0101, g0101), dot(g1001, g1001), dot(g1101, g1101)));
    g0001 *= norm01.x;
    g0101 *= norm01.y;
    g1001 *= norm01.z;
    g1101 *= norm01.w;
    vec4 norm10 = taylorInvSqrt(vec4(dot(g0010, g0010), dot(g0110, g0110), dot(g1010, g1010), dot(g1110, g1110)));
    g0010 *= norm10.x;
    g0110 *= norm10.y;
    g1010 *= norm10.z;
    g1110 *= norm10.w;
    vec4 norm11 = taylorInvSqrt(vec4(dot(g0011, g0011), dot(g0111, g0111), dot(g1011, g1011), dot(g1111, g1111)));
    g0011 *= norm11.x;
    g0111 *= norm11.y;
    g1011 *= norm11.z;
    g1111 *= norm11.w;
    float n0000 = dot(g0000, Pf0);
    float n1000 = dot(g1000, vec4(Pf1.x, Pf0.yzw));
    float n0100 = dot(g0100, vec4(Pf0.x, Pf1.y, Pf0.zw));
    float n1100 = dot(g1100, vec4(Pf1.xy, Pf0.zw));
    float n0010 = dot(g0010, vec4(Pf0.xy, Pf1.z, Pf0.w));
    float n1010 = dot(g1010, vec4(Pf1.x, Pf0.y, Pf1.z, Pf0.w));
    float n0110 = dot(g0110, vec4(Pf0.x, Pf1.yz, Pf0.w));
    float n1110 = dot(g1110, vec4(Pf1.xyz, Pf0.w));
    float n0001 = dot(g0001, vec4(Pf0.xyz, Pf1.w));
    float n1001 = dot(g1001, vec4(Pf1.x, Pf0.yz, Pf1.w));
    float n0101 = dot(g0101, vec4(Pf0.x, Pf1.y, Pf0.z, Pf1.w));
    float n1101 = dot(g1101, vec4(Pf1.xy, Pf0.z, Pf1.w));
    float n0011 = dot(g0011, vec4(Pf0.xy, Pf1.zw));
    float n1011 = dot(g1011, vec4(Pf1.x, Pf0.y, Pf1.zw));
    float n0111 = dot(g0111, vec4(Pf0.x, Pf1.yzw));
    float n1111 = dot(g1111, Pf1);
    vec4 fade_xyzw = quintic(Pf0);
    vec4 n_0w = mix(vec4(n0000, n1000, n0100, n1100), vec4(n0001, n1001, n0101, n1101), fade_xyzw.w);
    vec4 n_1w = mix(vec4(n0010, n1010, n0110, n1110), vec4(n0011, n1011, n0111, n1111), fade_xyzw.w);
    vec4 n_zw = mix(n_0w, n_1w, fade_xyzw.z);
    vec2 n_yzw = mix(n_zw.xy, n_zw.zw, fade_xyzw.y);
    float n_xyzw = mix(n_yzw.x, n_yzw.y, fade_xyzw.x);
    return 2.2 * n_xyzw;
}
#endif
)GLSL";

inline constexpr const char *kStructs = R"GLSL(
struct BaseForm {
    int type;
    vec3 scale;
    vec3 numParticles;
    float zSize;
    mat3 rotations;
    vec3 rotationCenter;
} baseForm;
struct Audio {
    float multiplier;
    float bassMultiplier;
    float mixing;
    float bass;
    float exponentiationFactor;
    float samplePoints[9];
    float samplePointsDifferences[9];
    float intermediateAudios[8];
    float value;
} audio;
struct Particle {
    vec4 color;
    float opacityMultiplier;
    int size;
    float feather;
    float colorIntensityAddStrength;
    float antiAlias;
    vec3 position;
} particle;
struct FractalField {
    float octaveMultiplier;
    float octaveScale;
    int complexity;
    float fScale;
    vec4 dimensions;
    float gamma;
    float minVal;
    float maxVal;
    float offset;
    float noiseMultiplier;
    float constantNoiseMultiplier;
    float affectOpacity;
    float affectSize;
    int loop;
    int loopFrames;
    int displacementType;
    vec3 displacements;
    vec4 flows;
    vec3 noise;
} fractalField;
struct Sphere {
    float radius;
    float feather;
    float strength;
    vec3 center;
    vec3 scale;
} sphere;
struct Glow {
    float blendMode;
    float mixAlpha;
    float offsetAngle;
    float maxAngle;
    vec2 size;
    float intensity;
    float directions;
    vec2 coords;
    float quality;
    vec4 color;
    float brightnessOffset;
    float lightStrength;
    float onTop;
};
#ifndef TWOPI
#define TWOPI (6.2831853071794)
#endif
#ifndef PI
#define PI (3.1415926535897)
#endif
#ifndef RAD_PI
#define RAD_PI (PI / 180.)
#endif
mat3 rotateX(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(1, 0, 0),
        vec3(0, cosAngle, sinAngle),
        vec3(0, -sinAngle, cosAngle));
    return rotationMatrix;
}
mat3 rotateY(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(cosAngle, 0, -sinAngle),
        vec3(0, 1, 0),
        vec3(sinAngle, 0, cosAngle));
    return rotationMatrix;
}
mat3 rotateZ(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(cosAngle, sinAngle, 0),
        vec3(-sinAngle, cosAngle, 0),
        vec3(0, 0, 1));
    return rotationMatrix;
}
#define IDENTITY_MATRIX mat3(vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1))
)GLSL";

inline constexpr const char *kConfig = R"GLSL(
#define colorTracking 0
uniform vec3 u_accent;
uniform highp float particleThin;
uniform int u_particleSize;
uniform int u_complexity;
uniform highp float u_glowDirections;
uniform highp float u_glowQuality;
void init()
{
    audio.multiplier = 6.4;
    audio.bassMultiplier = .5263 * resolution.x;
}
void setProps()
{
    particle.color = vec4(u_accent, 0.3);
    particle.size = u_particleSize;
    particle.feather = 1.0;
    particle.colorIntensityAddStrength = 0.38;
    particle.antiAlias = 8.5;
    fractalField.octaveMultiplier = 0.25;
    fractalField.octaveScale = 1.0;
    fractalField.complexity = u_complexity;
    fractalField.fScale = 9.473;
    fractalField.gamma = 1.0;
    fractalField.minVal = -5.0;
    fractalField.maxVal = 5.0;
    fractalField.flows = vec4(0, 3.8, 0, 1.3);
    fractalField.displacements = vec3(.3884 * resolution.x, .3884 * resolution.x - 20.0, .3884 * resolution.x - 5.0);
    sphere.radius = .7236 * resolution.x;
    sphere.feather = 0.45;
}
void modifyNoiseCoordinates(inout vec4 coords) {}
void setPropsWithNoise() {}
void modifySphericalDisplacement() {}
void setGlow0(inout Glow glow)
{
    glow.blendMode = 1.0;
    glow.mixAlpha = 1.0;
    glow.intensity = 1.0;
    glow.size = vec2(18);
    glow.directions = u_glowDirections;
    glow.quality = u_glowQuality;
    glow.color = vec4(u_accent, 1.0);
    glow.brightnessOffset = .0;
    glow.lightStrength = .5;
}
)GLSL";

inline constexpr const char *kNcsAudioBlock = R"GLSL(
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
)GLSL";

inline constexpr const char *kNcsDefaults = R"GLSL(
void defaultAudioValues()
{
    audio.value = 0.0;
    audio.bass = 0.0;
    audio.mixing = 0.5;
    audio.multiplier = 7.0;
    audio.bassMultiplier = 5.0;
    audio.exponentiationFactor = 1.02;
    audio.samplePoints[0] = 0.1;
    audio.samplePoints[1] = 0.2;
    audio.samplePoints[2] = 0.3;
    audio.samplePoints[3] = 0.4;
    audio.samplePoints[4] = 0.5;
    audio.samplePoints[5] = 0.6;
    audio.samplePoints[6] = 0.7;
    audio.samplePoints[7] = 0.8;
    audio.samplePoints[8] = 0.9;
    audio.samplePointsDifferences[0] = 0.05;
    audio.samplePointsDifferences[1] = 0.05;
    audio.samplePointsDifferences[2] = 0.05;
    audio.samplePointsDifferences[3] = 0.05;
    audio.samplePointsDifferences[4] = 0.05;
    audio.samplePointsDifferences[5] = 0.05;
    audio.samplePointsDifferences[6] = 0.05;
    audio.samplePointsDifferences[7] = 0.05;
    audio.samplePointsDifferences[8] = 0.05;
    audio.intermediateAudios[0] = 0.0;
    audio.intermediateAudios[1] = 0.0;
    audio.intermediateAudios[2] = 0.0;
    audio.intermediateAudios[3] = 0.0;
    audio.intermediateAudios[4] = 0.0;
    audio.intermediateAudios[5] = 0.0;
    audio.intermediateAudios[6] = 0.0;
    audio.intermediateAudios[7] = 0.0;
}
void defaultBaseFormValues()
{
    baseForm.type = 0;
    baseForm.scale = vec3(2.0);
    baseForm.numParticles = vec3(resolution.xy, 1);
    baseForm.zSize = 100.0;
    baseForm.rotations = IDENTITY_MATRIX;
    baseForm.rotationCenter = vec3(resolution.xy / 2.0, 0);
}
void defaultParticleValues()
{
    particle.color = vec4(0, 0, 1, 1);
    particle.size = 3;
    particle.feather = 0.5;
    particle.position = vec3(gl_FragCoord.xy, 0);
    particle.opacityMultiplier = 1.0;
    particle.colorIntensityAddStrength = 0.1;
    particle.antiAlias = 4.5;
}
void defaultFractalFieldValues()
{
    fractalField.octaveMultiplier = 0.5;
    fractalField.octaveScale = 1.5;
    fractalField.complexity = 3;
    fractalField.fScale = 10.0;
    fractalField.gamma = 1.0;
    fractalField.minVal = -1.0;
    fractalField.maxVal = 1.0;
    fractalField.noise = vec3(0);
    fractalField.affectOpacity = 0.0;
    fractalField.affectSize = 0.0;
    fractalField.loop = 0;
    fractalField.loopFrames = 200;
    fractalField.dimensions = vec4(1000);
    fractalField.displacementType = 0;
    fractalField.offset = 0.0;
    fractalField.noiseMultiplier = 1.0;
    fractalField.constantNoiseMultiplier = 0.0;
    fractalField.displacements = vec3(100);
    fractalField.flows = vec4(0, 0, 0, 2.);
}
void defaultSphereValues()
{
    sphere.radius = 0.0;
    sphere.feather = 0.0;
    sphere.scale = vec3(1);
    sphere.strength = 1.0;
    sphere.center = vec3(resolution.xy / 2.0, 0);
}
void setAudio()
{
    float audioRadius = (max(AUDIO1D(audioR,audio.samplePoints[0]).x, AUDIO1D(audioR,audio.samplePoints[0] + audio.samplePointsDifferences[0]).x) + max(AUDIO1D(audioL,audio.samplePoints[0]).x, AUDIO1D(audioL,audio.samplePoints[0] + audio.samplePointsDifferences[0]).x)) / 2.0;
    float audioFractal1 = (max(AUDIO1D(audioR,audio.samplePoints[1]).x, AUDIO1D(audioR,audio.samplePoints[1] + audio.samplePointsDifferences[1]).x) + max(AUDIO1D(audioL,audio.samplePoints[1]).x, AUDIO1D(audioL,audio.samplePoints[1] + audio.samplePointsDifferences[1]).x)) / 2.0;
    float audioFractal2 = (max(AUDIO1D(audioR,audio.samplePoints[2]).x, AUDIO1D(audioR,audio.samplePoints[2] + audio.samplePointsDifferences[2]).x) + max(AUDIO1D(audioL,audio.samplePoints[2]).x, AUDIO1D(audioL,audio.samplePoints[2] + audio.samplePointsDifferences[2]).x)) / 2.0;
    float audioFractal3 = (max(AUDIO1D(audioR,audio.samplePoints[3]).x, AUDIO1D(audioR,audio.samplePoints[3] + audio.samplePointsDifferences[3]).x) + max(AUDIO1D(audioL,audio.samplePoints[3]).x, AUDIO1D(audioL,audio.samplePoints[3] + audio.samplePointsDifferences[3]).x)) / 2.0;
    float audioFractal4 = (max(AUDIO1D(audioR,audio.samplePoints[4]).x, AUDIO1D(audioR,audio.samplePoints[4] + audio.samplePointsDifferences[4]).x) + max(AUDIO1D(audioL,audio.samplePoints[4]).x, AUDIO1D(audioL,audio.samplePoints[4] + audio.samplePointsDifferences[4]).x)) / 2.0;
    float audioFractal5 = (max(AUDIO1D(audioR,audio.samplePoints[5]).x, AUDIO1D(audioR,audio.samplePoints[5] + audio.samplePointsDifferences[5]).x) + max(AUDIO1D(audioL,audio.samplePoints[5]).x, AUDIO1D(audioL,audio.samplePoints[5] + audio.samplePointsDifferences[5]).x)) / 2.0;
    float audioFractal6 = (max(AUDIO1D(audioR,audio.samplePoints[6]).x, AUDIO1D(audioR,audio.samplePoints[6] + audio.samplePointsDifferences[6]).x) + max(AUDIO1D(audioL,audio.samplePoints[6]).x, AUDIO1D(audioL,audio.samplePoints[6] + audio.samplePointsDifferences[6]).x)) / 2.0;
    float audioFractal7 = (max(AUDIO1D(audioR,audio.samplePoints[7]).x, AUDIO1D(audioR,audio.samplePoints[7] + audio.samplePointsDifferences[7]).x) + max(AUDIO1D(audioL,audio.samplePoints[7]).x, AUDIO1D(audioL,audio.samplePoints[7] + audio.samplePointsDifferences[7]).x)) / 2.0;
    float audioFractal8 = (max(AUDIO1D(audioR,audio.samplePoints[8]).x, AUDIO1D(audioR,audio.samplePoints[8] + audio.samplePointsDifferences[8]).x) + max(AUDIO1D(audioL,audio.samplePoints[8]).x, AUDIO1D(audioL,audio.samplePoints[8] + audio.samplePointsDifferences[8]).x)) / 2.0;
    float audios[8] = float[8](audioFractal1, audioFractal2, audioFractal3, audioFractal4, audioFractal5, audioFractal6, audioFractal7, audioFractal8);
    float temp;
    temp = max(audios[0], audios[2]); audios[0] = min(audios[0], audios[2]); audios[2] = temp;
    temp = max(audios[1], audios[3]); audios[1] = min(audios[1], audios[3]); audios[3] = temp;
    temp = max(audios[4], audios[6]); audios[4] = min(audios[4], audios[6]); audios[6] = temp;
    temp = max(audios[5], audios[7]); audios[5] = min(audios[5], audios[7]); audios[7] = temp;
    temp = max(audios[0], audios[4]); audios[0] = min(audios[0], audios[4]); audios[4] = temp;
    temp = max(audios[1], audios[5]); audios[1] = min(audios[1], audios[5]); audios[5] = temp;
    temp = max(audios[2], audios[6]); audios[2] = min(audios[2], audios[6]); audios[6] = temp;
    temp = max(audios[3], audios[7]); audios[3] = min(audios[3], audios[7]); audios[7] = temp;
    temp = max(audios[0], audios[1]); audios[0] = min(audios[0], audios[1]); audios[1] = temp;
    temp = max(audios[2], audios[3]); audios[2] = min(audios[2], audios[3]); audios[3] = temp;
    temp = max(audios[4], audios[5]); audios[4] = min(audios[4], audios[5]); audios[5] = temp;
    temp = max(audios[6], audios[7]); audios[6] = min(audios[6], audios[7]); audios[7] = temp;
    temp = max(audios[2], audios[4]); audios[2] = min(audios[2], audios[4]); audios[4] = temp;
    temp = max(audios[3], audios[5]); audios[3] = min(audios[3], audios[5]); audios[5] = temp;
    temp = max(audios[1], audios[4]); audios[1] = min(audios[1], audios[4]); audios[4] = temp;
    temp = max(audios[3], audios[6]); audios[3] = min(audios[3], audios[6]); audios[6] = temp;
    temp = max(audios[1], audios[2]); audios[1] = min(audios[1], audios[2]); audios[2] = temp;
    temp = max(audios[3], audios[4]); audios[3] = min(audios[3], audios[4]); audios[4] = temp;
    temp = max(audios[5], audios[6]); audios[5] = min(audios[5], audios[6]); audios[6] = temp;
    audio.value = audio.multiplier * mix(mix(audios[7] * audios[6] - audios[1] * audios[0], audios[7] * audios[6], audios[5]), mix(audios[6] * mix(audios[7] - audios[0], audios[6] - audios[3], audios[7] * audios[6]) - pow(audios[1] * audios[0], audio.exponentiationFactor), audios[7] * audios[6], audios[5] * audios[4]), audio.mixing);
    audio.bass = audio.bassMultiplier * abs(audioRadius);
    audio.intermediateAudios[0] = audioFractal1;
    audio.intermediateAudios[1] = audioFractal2;
    audio.intermediateAudios[2] = audioFractal3;
    audio.intermediateAudios[3] = audioFractal4;
    audio.intermediateAudios[4] = audioFractal5;
    audio.intermediateAudios[5] = audioFractal6;
    audio.intermediateAudios[6] = audioFractal7;
    audio.intermediateAudios[7] = audioFractal8;
}
)GLSL";

inline constexpr const char *kNcs1Main = R"GLSL(
float octaveNoise(vec4 p, vec4 flow, vec4 rep)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    float value = 0.0;
    for (int i = 0; i < fractalField.complexity; i += 1) {
        vec4 fractalFieldInput = p;
        modifyNoiseCoordinates(fractalFieldInput);
        fractalFieldInput += flow * time;
        fractalFieldInput *= frequency;
        value += (pnoise(vec4((fractalFieldInput)), rep)) * amplitude;
        total += amplitude;
        amplitude *= fractalField.octaveMultiplier;
        frequency *= fractalField.octaveScale;
    }
    return value / total;
}
float fbm3(vec4 p, vec4 flow)
{
    vec4 flowXLoopFrames = flow * float(fractalField.loopFrames);
    vec4 rep = vec4(fractalField.loop * ivec4(fractalField.fScale * flowXLoopFrames / fractalField.dimensions));
    flowXLoopFrames = mix(vec4(1), flowXLoopFrames, 1. - step(abs(flowXLoopFrames), vec4(0)));
    vec4 newFScale = mix(fractalField.dimensions * rep / (flowXLoopFrames), vec4(fractalField.fScale), vec4(1) - abs(float(fractalField.loop) * sign(flow)));
    p = newFScale * p / fractalField.dimensions;
    flow *= newFScale / fractalField.dimensions;
    vec3 originalSphereCenter = sphere.center;
    sphere.center *= newFScale.xyz / fractalField.dimensions.xyz;
    float oN = (fractalField.constantNoiseMultiplier + audio.value) * (octaveNoise(p, flow, rep));
    oN = sign(oN) * pow(abs(oN), fractalField.gamma);
    sphere.center = originalSphereCenter;
    oN = fractalField.offset + fractalField.noiseMultiplier * oN;
    oN = clamp(oN, fractalField.minVal, fractalField.maxVal);
    return oN;
}
vec3 sphereCoords(vec3 particleCoords, float zLayer, float zLayerDistance)
{
    vec3 newPos;
    float u = (TWOPI * (((particleCoords.x) / (resolution.x))));
    float v = PI * (particleCoords.y / resolution.y);
    newPos.x = resolution.x * sin(u) * sin(v);
    newPos.z = baseForm.zSize * cos(u) * sin(v);
    newPos.y = resolution.y * cos(v);
    newPos.xy += resolution.xy / 2.;
    newPos -= zLayer * (vec3(resolution.xy / 2., baseForm.zSize / 2.) / baseForm.numParticles.z) * normalize(newPos - vec3(resolution.xy / 2.0, 0));
    return newPos;
}
vec3 transformedCoords(vec3 particleCoords)
{
    particleCoords = (baseForm.rotations) * (particleCoords - baseForm.rotationCenter);
    return (particleCoords + vec3(resolution.xy / 2.0, 0));
}
void processZLayer(int zLayer, float zLayerDistance)
{
    vec3 particleCoords = particle.position;
    particleCoords = mix(particleCoords, sphereCoords(particleCoords, float(zLayer), zLayerDistance), float(baseForm.type));
    vec4 old = vec4(particleCoords, 0);
    vec3 displacementValues = vec3(0);
    vec4 flows = fractalField.flows;
    float xFBM3 = fbm3(old.xyzw, flows);
    float yFBM3 = fbm3(old.yzxw, flows.yzxw);
    float zFBM3 = fbm3(old.zxyw, flows.zxyw);
    fractalField.noise = vec3(xFBM3, yFBM3, zFBM3);
    setPropsWithNoise();
    displacementValues.xyz += mix(vec3((fractalField.displacements.x) * xFBM3, (fractalField.displacements.y) * yFBM3, (fractalField.displacements.z) * zFBM3), (fractalField.displacements.x) * xFBM3 * normalize(particleCoords.xyz - vec3(resolution.xy / 2.0, 0)), float(fractalField.displacementType));
    particleCoords.xyz += displacementValues;
    float radius = sphere.radius, blurSize = particle.antiAlias / resolution.y;
    radius += audio.bass;
    vec3 sphereCenterCoords = sphere.center;
    vec3 vectorFromSphereCenter = (particleCoords - sphereCenterCoords);
    vec3 normalizedVector = normalize(vectorFromSphereCenter);
    vec3 newPos = (sphereCenterCoords + radius * normalizedVector);
    float diff = length(newPos - particleCoords);
    diff *= (clamp((smoothstep(0.0, sphere.feather * (radius), diff)) + blurSize, blurSize, 1.0 + blurSize));
    particleCoords += step(length(vectorFromSphereCenter), radius) * sphere.strength * diff * normalizedVector * sphere.scale;
    modifySphericalDisplacement();
    particle.size = int(max(0., float(particle.size) + fractalField.affectSize * (xFBM3 + yFBM3 + zFBM3)));
    particle.opacityMultiplier = max(particle.opacityMultiplier + fractalField.affectOpacity * (xFBM3 + yFBM3 + zFBM3), 0.);
    for (int i = -particle.size; i <= particle.size; i++)
        for (int j = -particle.size; j <= particle.size; j++) {
            float distanceFromParticleCenter = length(vec2(i, j));
            distanceFromParticleCenter = mix(step(distanceFromParticleCenter, float(particle.size)), (1. - smoothstep(float(particle.size) - particle.feather * float(particle.size), float(particle.size), distanceFromParticleCenter)), particle.feather);
            distanceFromParticleCenter *= 100000. * particle.opacityMultiplier;
            vec3 finalCoords = vec3(transformedCoords(particleCoords.xyz) + vec3(i, j, 0)) / baseForm.scale;
            finalCoords += vec3(resolution.xy / 2., 0) * (1. - 1. / (baseForm.scale));
            uint depth = imageAtomicAdd(atomicImageTexture0, ivec2(finalCoords.xy), uint((distanceFromParticleCenter)));
            if ((colorTracking) == 1) {
                uint colorValue = imageAtomicAdd(atomicImageTexture1, ivec2(finalCoords.xy), uint(clamp(particle.color.x * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture2, ivec2(finalCoords.xy), uint(clamp(particle.color.y * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture3, ivec2(finalCoords.xy), uint(clamp(particle.color.z * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture4, ivec2(finalCoords.xy), uint(clamp(particle.color.w * distanceFromParticleCenter, 0., 100000.)));
            }
        }
}
void main()
{
    defaultAudioValues();
    defaultBaseFormValues();
    defaultParticleValues();
    defaultFractalFieldValues();
    defaultSphereValues();
    init();
    setAudio();
    vec3 denom = vec3(baseForm.numParticles - step(float(baseForm.type), 0.0) * vec3(1, 1, 1));
    denom = max(denom, vec3(0.0001));
    ivec3 spaces = ivec3(round((vec3(resolution.xy, baseForm.zSize)) / denom));
    spaces -= int(step(float(baseForm.type), 0.0)) * ivec3(1, 1, 1);
    spaces.x = max(spaces.x, 1);
    spaces.y = max(spaces.y, 1);
    spaces.z = (baseForm.numParticles.z <= 1.0) ? int(baseForm.zSize) + 1 : max(spaces.z, 1);
    particle.position.z -= (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.;
    particle.position.xy += step(baseForm.numParticles.xy, vec2(1)) * vec2(resolution.xy / 2.);
    int currentZIndex = 0;
    for (float i = 0.0; i < baseForm.zSize; i += float(spaces.z)) {
        if (length(vec3(ivec3(mod(vec3(gl_FragCoord.xy, 0.0), vec3(spaces))))) > 0.0)
            discard;
        highp vec2 _pcell = floor(gl_FragCoord.xy);
        if (fract(sin(mod(dot(_pcell, vec2(127.1, 311.7)), 6.2831853)) * 43758.5453123) < particleThin)
            discard;
        setProps();
        processZLayer(currentZIndex, (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.);
        currentZIndex += 1;
        baseForm.rotations = IDENTITY_MATRIX;
        sphere.center = vec3(resolution.xy / 2., 0);
        particle.position.xy = gl_FragCoord.xy;
        particle.position.z = i + float(spaces.z) - (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.;
        particle.position.xy += step(baseForm.numParticles.xy, vec2(1)) * vec2(resolution.xy / 2.);
    }
}
)GLSL";

inline constexpr const char *kNcs2Main = R"GLSL(
uniform sampler2D tex;
vec4 getTrackedColors()
{
    uint colorsV4 = 0u;
    vec4 fetchedColors = vec4(0);
    colorsV4 = imageAtomicExchange(atomicImageTexture1, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.x = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture2, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.y = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture3, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.z = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture4, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.w = float(colorsV4);
    return fetchedColors;
}
void main()
{
    defaultBaseFormValues();
    defaultParticleValues();
    defaultFractalFieldValues();
    defaultSphereValues();
    init();
    setAudio();
    setProps();
    uint depth = 0u;
    depth = imageAtomicExchange(atomicImageTexture0, ivec2(gl_FragCoord.xy), depth);
    vec4 noiseCoords = vec4(1, 1, 1, 0);
    modifyNoiseCoordinates(noiseCoords);
    fractalField.noise = vec3(1);
    setPropsWithNoise();
    modifySphericalDisplacement();
    particle.color = ((colorTracking) == 0 ? particle.color : (getTrackedColors() / float(depth)));
    float actualDepth = float(depth) / (100000.);
    FragColor = step(0.0, float(depth)) * vec4(particle.color.xyz * particle.color.w, particle.color.w);
    FragColor *= (pow(actualDepth, particle.colorIntensityAddStrength)) * (1.0 - pow(1.0 - particle.color.w, actualDepth));
}
)GLSL";

inline constexpr const char *kGlowDecls = R"GLSL(
uniform int postProcessingNumber;
uniform vec2 resolution;
uniform sampler2D audioL;
uniform sampler2D audioR;
uniform float time;
uniform int audioLSize;
uniform int audioRSize;
uniform sampler2D tex;
uniform float u_fade;
uniform vec4 u_backdrop;
out vec4 FragColor;
#ifndef TWOPI
#define TWOPI 6.28318530718
#endif
#ifndef PI
#define PI 3.14159265359
#endif
#define RAD_PI PI / 180.
)GLSL";

inline constexpr const char *kGlowMain = R"GLSL(
float glowLightVal(float glowValue, float glowLightStrengthValue, float glowLightDistanceValue)
{
    return glowLightDistanceValue + glowLightDistanceValue / pow(glowValue, glowLightStrengthValue);
}
vec4 addColors(float blendMode, vec4 above, vec4 below)
{
    return above + (1. - blendMode * above.w) * below;
}
Glow glow0;
void defaultGlowValues(inout Glow glow)
{
    glow.blendMode = 1.0;
    glow.mixAlpha = 1.0;
    glow.offsetAngle = 0.0;
    glow.size = vec2(10);
    glow.intensity = .5;
    glow.directions = 8.0;
    glow.onTop = 0.0;
    glow.coords = gl_FragCoord.xy;
    glow.maxAngle = 360.0;
    glow.quality = 4.0;
    vec4 color = vec4(0.5, 0.5, 0.5, 1.0);
    glow.brightnessOffset = .0;
    glow.lightStrength = .5;
}
void main()
{
    defaultGlowValues(glow0);
    setGlow0(glow0);
    Glow glow;
    glow = glow0;
    vec2 uv = (glow.coords) / resolution.xy;
    vec4 prevColor = texture(tex, gl_FragCoord.xy / resolution.xy);
    vec2 glowRadius = (glow.size) / resolution.xy;
    vec4 Color = vec4(0);
    float glowOffsetValue = (float(glow.offsetAngle) / 360.) * TWOPI;
    for (float d = glowOffsetValue; d < (glow.maxAngle / 360. * TWOPI); d += TWOPI / (glow.directions))
    {
        for (float i = 1.0 / (glow.quality); i <= 1.0; i += 1.0 / (glow.quality))
        {
            vec2 coords = uv + glowRadius * i * vec2(cos(d), sin(d));
            if (coords.x > 0.0 && coords.x < 1.0 && coords.y > 0.0 && coords.y < 1.0)
                Color += texture(tex, coords);
        }
    }
    Color /= (glow.quality) * (glow.directions);
    FragColor = (vec4(glow.color.xyz * glow.color.w, glow.color.w)) * glow.intensity * length(Color);
    FragColor = addColors(glow.blendMode, mix(prevColor, FragColor, glow.onTop), mix(FragColor, prevColor, glow.onTop));
    FragColor *= glowLightVal(length(FragColor), glow.brightnessOffset, glow.lightStrength);
    FragColor.w = mix(prevColor.w, FragColor.w, glow.mixAlpha * 0.5);
    FragColor += (1.0 - FragColor.w) * u_backdrop;
    FragColor *= u_fade;
}
)GLSL";

// Audio transform passes adapted from GLava by jarcode-foss, licensed under
// GPL-3.0.

inline constexpr const char *kAudioPassFs = R"GLSL(#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform sampler2D audioR;
out vec4 fragment;
void main() {
    fragment.r = texelFetch(audioR, ivec2(int(gl_FragCoord.x), 0), 0).r;
}
)GLSL";

inline constexpr const char *kAudioGravityFs = R"GLSL(#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform sampler2D audioR;
uniform float diff;
out vec4 FragColor;
void main()
{
    FragColor.r = texelFetch(audioR, ivec2(int(gl_FragCoord.x), 0), 0).r - diff;
}
)GLSL";

inline constexpr const char *kAudioAverageFs = R"GLSL(#version 320 es
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
void main()
{
    float r = 0.0;
    r += window(float(0), float(avgFrames - 1)) * texelFetch(audioR0, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(1), float(avgFrames - 1)) * texelFetch(audioR1, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(2), float(avgFrames - 1)) * texelFetch(audioR2, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(3), float(avgFrames - 1)) * texelFetch(audioR3, ivec2(int(gl_FragCoord.x), 0), 0).r;
    r += window(float(4), float(avgFrames - 1)) * texelFetch(audioR4, ivec2(int(gl_FragCoord.x), 0), 0).r;
    FragColor.r = (r / float(avgFrames));
}
)GLSL";

inline constexpr const char *kAudioSmoothFs = R"GLSL(#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
#ifndef TWOPI
#define TWOPI 6.28318530718
#endif
#ifndef PI
#define PI 3.14159265359
#endif
#define sinusoidal(x) ((0.5 * sin((PI * (x)) - (PI / 2.0))) + 0.5)
#define ROUND_FORMULA sinusoidal
uniform int sample_mode;
uniform float sample_hybrid_weight;
uniform float sample_scale;
uniform float sample_range;
uniform float smooth_factor;
float scale_audio(float idx)
{
    return -log((-(sample_range)*idx) + 1.0) / (sample_scale);
}
float smooth_audio(in sampler2D tex, int tex_sz, highp float idx)
{
    float smin = scale_audio(clamp(idx - smooth_factor, 0.0, 1.0)) * float(tex_sz),
          smax = scale_audio(clamp(idx + smooth_factor, 0.0, 1.0)) * float(tex_sz);
    float m = ((smax - smin) / 2.0), s, w;
    float rm = smin + m;
    if (sample_mode == 0) {
        float avg = 0.0, weight = 0.0;
        for (s = smin; s <= smax; s += 1.0) {
            w = ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            weight += w;
            avg += texelFetch(tex, ivec2(int(round(s)), 0), 0).r * w;
        }
        avg /= weight;
        return avg;
    } else if (sample_mode == 2) {
        float vmax = 0.0, avg = 0.0, weight = 0.0, v;
        for (s = smin; s < smax; s += 1.0) {
            w = ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            weight += w;
            v = texelFetch(tex, ivec2(int(round(s)), 0), 0).r * w;
            avg += v;
            if (vmax < v)
                vmax = v;
        }
        return (vmax * (1.0 - sample_hybrid_weight)) + ((avg / weight) * sample_hybrid_weight);
    } else if (sample_mode == 1) {
        float vmax = 0.0, v;
        for (s = smin; s < smax; s += 1.0) {
            w = texelFetch(tex, ivec2(int(round(s)), 0), 0).r * ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            if (vmax < w)
                vmax = w;
        }
        return vmax;
    }
    return 0.0;
}
uniform sampler2D audioR;
out vec4 FragColor;
uniform int audioRSize;
uniform int adjacentSampleNums;
#define adjacent(I) FragColor.r += (1. - step(float(I), 1.)) * (smooth_audio(audioR, audioRSize, u + float(I - 1) * aRI) + smooth_audio(audioR, audioRSize, u - float(I - 1) * aRI));
void main()
{
    float u = gl_FragCoord.x / float(audioRSize);
    FragColor.r = 0.0;
    float aRI = 1. / float(audioRSize);
    adjacent(0);
    FragColor.r += (smooth_audio(audioR, audioRSize, u));
    FragColor.r /= 2. * (float(adjacentSampleNums) - 1.) + 1.;
}
)GLSL";

inline std::string ncs1_fs() {
    return std::string(kFsHeader) + kNcsAudioBlock + kLygiaPnoise + kStructs +
           kConfig + kNcsDefaults + kNcs1Main;
}

inline std::string ncs2_fs() {
    return std::string(kFsHeader) + kNcsAudioBlock + kStructs + kConfig +
           kNcsDefaults + kNcs2Main;
}

inline std::string glow_fs() {
    return std::string(kFsHeader) + kGlowDecls + kStructs + kConfig + kGlowMain;
}

} // namespace resonance_shaders