#version 320 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_from;
uniform sampler2D u_to;
uniform vec4 u_from_uv;
uniform vec4 u_to_uv;
uniform vec4 u_fill;
uniform float u_progress;
uniform float u_smoothness;
uniform float u_aspect;
uniform float u_stripe_count;
uniform float u_angle;
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

    float ms = mix(0.001, 0.3, u_smoothness * u_smoothness);

    float rad = radians(u_angle);
    float ca = cos(rad);
    float sa = sin(rad);
    vec2 auv = vec2(uv.x * u_aspect, uv.y);

    float sc = auv.x * ca + auv.y * sa;
    float pc = -auv.x * sa + auv.y * ca;

    float total = max(u_stripe_count, 1.0);
    float maxsc = u_aspect * abs(ca) + abs(sa);
    float sw = maxsc / total;

    float sp = sc / sw;
    float si = floor(sp);
    float odd = mod(si, 2.0);

    float maxp = u_aspect * abs(sa) + abs(ca);
    float np = pc / maxp;

    float delay = abs(np) * 0.3;
    float lp = clamp((u_progress - delay) / (1.0 - delay), 0.0, 1.0);
    float lf = fract(sp);

    float edge;
    float f;
    if (odd > 0.5) {
        edge = mix(1.0 + ms, -ms, lp);
        f = smoothstep(edge - ms, edge + ms, lf);
        fragColor = mix(c1, c2, f);
    } else {
        edge = mix(-ms, 1.0 + ms, lp);
        f = smoothstep(edge - ms, edge + ms, lf);
        fragColor = mix(c2, c1, f);
    }
}
