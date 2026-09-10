#version 320 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_from;
uniform sampler2D u_to;
uniform vec4 u_from_uv;
uniform vec4 u_to_uv;
uniform vec4 u_fill;
uniform float u_progress;
out vec4 fragColor;

vec4 s_from(vec2 uv) {
    vec2 p = uv * u_from_uv.xy + u_from_uv.zw;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
        return u_fill;
    return texture(u_from, p);
}

vec4 s_to(vec2 uv) {
    vec2 p = uv * u_to_uv.xy + u_to_uv.zw;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
        return u_fill;
    return texture(u_to, p);
}

void main() {
    vec2 uv = v_uv;
    float zoom = 0.15;

    float s1 = 1.0 + zoom * u_progress;
    vec2 uv1 = (uv - 0.5) / s1 + 0.5;

    float s2 = 1.0 + zoom * (1.0 - u_progress);
    vec2 uv2 = (uv - 0.5) / s2 + 0.5;

    fragColor = mix(s_from(uv1), s_to(uv2), u_progress);
}
