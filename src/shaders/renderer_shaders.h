#pragma once

constexpr const char *kRendererQuadVs = R"(
    attribute vec2 a_pos;
    uniform vec2 u_viewport;
    uniform vec4 u_rect;
    uniform vec4 u_model_ab;
    uniform vec2 u_model_t;
    varying vec2 v_uv;
    void main() {
        v_uv = a_pos;
        vec2 px = u_rect.xy + a_pos * u_rect.zw;
        px = vec2(u_model_ab.x * px.x + u_model_ab.y * px.y + u_model_t.x,
                  u_model_ab.z * px.x + u_model_ab.w * px.y + u_model_t.y);
        vec2 ndc = vec2(px.x / u_viewport.x * 2.0 - 1.0,
                         1.0 - px.y / u_viewport.y * 2.0);
        gl_Position = vec4(ndc, 0.0, 1.0);
    }
)";

constexpr const char *kRendererRectFs = R"(
    precision mediump float;
    uniform vec4 u_color;
    void main() { gl_FragColor = u_color; }
)";

constexpr const char *kRendererTexFs = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_tex;
    uniform vec4 u_color;
    void main() { gl_FragColor = texture2D(u_tex, v_uv) * u_color; }
)";

constexpr const char *kRendererVideoFs = R"(
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    varying vec2 v_uv;
    uniform samplerExternalOES u_tex;
    uniform float u_opacity;
    void main() {
        gl_FragColor = texture2D(u_tex, v_uv);
        gl_FragColor.a *= u_opacity;
    }
)";

constexpr const char *kRendererRrectFs = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform vec2 u_size;
    uniform float u_radius;
    uniform float u_border_width;
    uniform vec4 u_fill_color;
    uniform vec4 u_border_color;
    void main() {
        vec2 half_size = u_size * 0.5;
        vec2 p = (v_uv - 0.5) * u_size;
        vec2 b = half_size - u_radius;
        float d = length(max(abs(p) - b, 0.0)) - u_radius;
        float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
        vec4 color = d <= -u_border_width ? u_fill_color : u_border_color;
        gl_FragColor = color * alpha;
    }
)";

constexpr const char *kThunderBurstFs = R"(
    precision highp float;
    varying vec2 v_uv;
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
        gl_FragColor = vec4(col, a);
    }
)";

constexpr const char *kThunderShockFs = R"(
    precision highp float;
    varying vec2 v_uv;
    uniform vec2 u_size;
    uniform vec2 u_center;
    uniform float u_time;
    uniform float u_progress;
    uniform float u_radius;
    uniform float u_intensity;
    uniform vec4 u_core;
    uniform vec4 u_glow;

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
        gl_FragColor = vec4(col, a);
    }
)";

constexpr const char *kRendererRoundedTexFs = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_tex;
    uniform vec4 u_color;
    uniform vec2 u_size;
    uniform float u_radius;
    void main() {
        vec2 half_size = u_size * 0.5;
        vec2 p = (v_uv - 0.5) * u_size;
        vec2 b = half_size - u_radius;
        float d = length(max(abs(p) - b, 0.0)) - u_radius;
        float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
        gl_FragColor = texture2D(u_tex, v_uv) * u_color * alpha;
    }
)";
