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
uniform vec2 u_center;
uniform float u_aspect;
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

    vec2 auv = vec2(uv.x * u_aspect, uv.y);
    vec2 ac = vec2(u_center.x * u_aspect, u_center.y);
    float dist = distance(auv, ac);

    float md = 0.0;
    md = max(md, distance(ac, vec2(0.0, 0.0)));
    md = max(md, distance(ac, vec2(u_aspect, 0.0)));
    md = max(md, distance(ac, vec2(0.0, 1.0)));
    md = max(md, distance(ac, vec2(u_aspect, 1.0)));

    float radius = u_progress * (md + 2.0 * ms) - ms;
    float f = smoothstep(radius - ms, radius + ms, dist);
    fragColor = mix(c2, c1, f);
}
