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
uniform float u_cell_size;
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

vec2 hex_round(float q, float r) {
    float x = q;
    float z = r;
    float y = -x - z;

    float rx = floor(x + 0.5);
    float ry = floor(y + 0.5);
    float rz = floor(z + 0.5);

    float dx = abs(rx - x);
    float dy = abs(ry - y);
    float dz = abs(rz - z);

    if (dx > dy && dx > dz)
        rx = -ry - rz;
    else if (dy > dz)
        ry = -rx - rz;
    else
        rz = -rx - ry;

    return vec2(rx, rz);
}

void main() {
    vec2 uv = v_uv;
    vec4 c1 = s_from(uv);
    vec4 c2 = s_to(uv);

    float ms = mix(0.001, 0.5, u_smoothness * u_smoothness);
    float size = max(u_cell_size, 0.001);
    vec2 auv = vec2(uv.x * u_aspect, uv.y);

    float q = (auv.x * (2.0 / 3.0)) / size;
    float r = ((-auv.x / 3.0) + (sqrt(3.0) / 3.0) * auv.y) / size;

    vec2 hc = hex_round(q, r);

    float cx = (hc.x * 1.5) * size;
    float cy = (hc.x * sqrt(3.0) / 2.0 + hc.y * sqrt(3.0)) * size;
    vec2 cc = vec2(cx, cy);

    vec2 wo = vec2(u_center.x * u_aspect, u_center.y);
    float dist = distance(cc, wo);

    float md = 0.0;
    md = max(md, distance(wo, vec2(0.0, 0.0)));
    md = max(md, distance(wo, vec2(u_aspect, 0.0)));
    md = max(md, distance(wo, vec2(0.0, 1.0)));
    md = max(md, distance(wo, vec2(u_aspect, 1.0)));

    float radius = u_progress * (md + 2.0 * ms) - ms;
    float f = smoothstep(radius - ms, radius + ms, dist);
    fragColor = mix(c2, c1, f);
}
