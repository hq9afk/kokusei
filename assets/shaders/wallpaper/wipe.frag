#version 320 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_from;
uniform sampler2D u_to;
uniform vec4 u_from_uv;
uniform vec4 u_to_uv;
uniform vec4 u_fill;
uniform float u_progress;
uniform float u_direction;
uniform float u_smoothness;
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
    vec4 c1 = s_from(uv);
    vec4 c2 = s_to(uv);

    float ms = mix(0.001, 0.5, u_smoothness * u_smoothness);
    float ep = u_progress * (1.0 + 2.0 * ms) - ms;
    float edge;
    float f;

    if (u_direction < 0.5) {
        edge = 1.0 - ep;
        f = smoothstep(edge - ms, edge + ms, uv.x);
        fragColor = mix(c1, c2, f);
    } else if (u_direction < 1.5) {
        edge = ep;
        f = smoothstep(edge - ms, edge + ms, uv.x);
        fragColor = mix(c2, c1, f);
    } else if (u_direction < 2.5) {
        edge = 1.0 - ep;
        f = smoothstep(edge - ms, edge + ms, uv.y);
        fragColor = mix(c1, c2, f);
    } else {
        edge = ep;
        f = smoothstep(edge - ms, edge + ms, uv.y);
        fragColor = mix(c2, c1, f);
    }
}
