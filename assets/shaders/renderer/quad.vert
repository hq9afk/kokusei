#version 320 es
in vec2 a_pos;
uniform vec2 u_viewport;
uniform vec4 u_rect;
uniform vec4 u_model_ab;
uniform vec2 u_model_t;
out vec2 v_uv;
void main() {
    v_uv = a_pos;
    vec2 px = u_rect.xy + a_pos * u_rect.zw;
    px = vec2(u_model_ab.x * px.x + u_model_ab.y * px.y + u_model_t.x,
              u_model_ab.z * px.x + u_model_ab.w * px.y + u_model_t.y);
    vec2 ndc = vec2(px.x / u_viewport.x * 2.0 - 1.0,
                     1.0 - px.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
