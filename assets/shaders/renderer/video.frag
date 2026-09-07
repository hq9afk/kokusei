#version 320 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
in vec2 v_uv;
uniform samplerExternalOES u_tex;
uniform float u_opacity;
out vec4 fragColor;
void main() {
    fragColor = texture(u_tex, v_uv);
    fragColor.a *= u_opacity;
}
