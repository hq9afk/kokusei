#version 320 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec4 u_color;
uniform vec2 u_size;
uniform float u_radius;
out vec4 fragColor;
void main() {
    vec2 half_size = u_size * 0.5;
    vec2 p = (v_uv - 0.5) * u_size;
    vec2 b = half_size - u_radius;
    float d = length(max(abs(p) - b, 0.0)) - u_radius;
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    fragColor = texture(u_tex, v_uv) * u_color * alpha;
}
