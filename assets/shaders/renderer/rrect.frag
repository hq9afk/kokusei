#version 320 es
precision highp float;
in vec2 v_uv;
uniform vec2 u_size;
uniform float u_radius;
uniform float u_border_width;
uniform vec4 u_fill_color;
uniform vec4 u_border_color;
out vec4 fragColor;
void main() {
    vec2 half_size = u_size * 0.5;
    vec2 p = (v_uv - 0.5) * u_size;
    vec2 b = half_size - u_radius;
    float d = length(max(abs(p) - b, 0.0)) - u_radius;
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    vec4 color = d <= -u_border_width ? u_fill_color : u_border_color;
    fragColor = color * alpha;
}
