#version 320 es
precision highp float;
in vec2 v_uv;
uniform vec2 u_size;
uniform vec2 u_a;
uniform vec2 u_b;
uniform float u_time;
uniform float u_progress;
uniform float u_intensity;
uniform float u_seed;
uniform float u_amp;
uniform float u_thick;
uniform vec4 u_core;
uniform vec4 u_glow;
out vec4 fragColor;

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

float vnoise(float x) {
    float i = floor(x);
    float f = fract(x);
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash1(i), hash1(i + 1.0), u);
}

float bolt_field(vec2 p, vec2 a, vec2 b, float seed) {
    vec2 ab = b - a;
    float len = max(length(ab), 1.0);
    vec2 dir = ab / len;
    vec2 nrm = vec2(-dir.y, dir.x);
    vec2 rel = p - a;
    float t = dot(rel, dir) / len;
    float perp = dot(rel, nrm);
    float thick = max(u_thick, 0.001);
    float tw = mod(u_time, 128.0);

    float disp = (vnoise(t * 7.0 + seed * 13.0 + tw * 9.0) - 0.5);
    disp += (vnoise(t * 17.0 + seed * 31.0 + tw * 17.0) - 0.5) * 0.45;
    disp += (vnoise(t * 3.0 + seed * 7.0 - tw * 4.0) - 0.5) * 1.4;
    disp *= u_amp * smoothstep(0.0, 0.12, t) *
        (1.0 - smoothstep(0.82, 1.0, t));

    float d = abs(perp - disp);
    float head = 1.0 - smoothstep(u_progress - 0.10, u_progress + 0.02, t);
    float span = step(-0.02, t) * step(t, 1.02);
    float core = exp(-d * d * 0.05 / (thick * thick));
    float glow = exp(-d * 0.055 / thick);
    return (core * 1.0 + glow * 0.4) * head * span;
}

void main() {
    vec2 p = v_uv * u_size;
    float f = bolt_field(p, u_a, u_b, u_seed);
    vec2 mid = mix(u_a, u_b, 0.5) + vec2(u_amp * 1.5, -u_amp);
    f += bolt_field(p, u_a, mid, u_seed + 5.0) * 0.5;
    f *= u_intensity;
    f *= float(f == f);
    f = min(f, 8.0);
    float a = clamp(f, 0.0, 1.0);
    vec3 col = mix(u_glow.rgb, u_core.rgb, clamp(f * f, 0.0, 1.0));
    fragColor = vec4(col, a);
}
