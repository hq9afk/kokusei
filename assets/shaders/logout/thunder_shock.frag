#version 320 es
precision highp float;
in vec2 v_uv;
uniform vec2 u_size;
uniform vec2 u_center;
uniform float u_time;
uniform float u_progress;
uniform float u_radius;
uniform float u_intensity;
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

void main() {
    vec2 p = v_uv * u_size - u_center + vec2(0.001, 0.001);
    float d = length(p);
    float ang = atan(p.y, p.x);
    float tw = mod(u_time, 128.0);

    float front = u_progress * u_radius;
    float wob = (vnoise(ang * 5.0 + tw * 7.0) - 0.5) * 30.0;
    wob += (vnoise(ang * 17.0 - tw * 5.0) - 0.5) * 12.0;

    float ring = abs(d - front - wob);
    float width = max(12.0 + 46.0 * u_progress, 0.001);
    float core = exp(-ring * ring / (width * width));
    float glow = exp(-ring / (width * 1.7));
    float front_safe = max(front, 0.001);
    float inside =
        (1.0 - smoothstep(front_safe * 0.35, front_safe, d)) * 0.14;

    float fade = 1.0 - u_progress;
    float f = (core + glow * 0.5 + inside) * u_intensity * fade;
    f *= float(f == f);
    f = min(f, 8.0);
    float a = clamp(f, 0.0, 1.0);
    vec3 col = mix(u_glow.rgb, u_core.rgb, clamp(f * f, 0.0, 1.0));
    fragColor = vec4(col, a);
}
