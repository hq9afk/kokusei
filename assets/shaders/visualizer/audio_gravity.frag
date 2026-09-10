#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
uniform sampler2D audioR;
uniform float diff;
out vec4 FragColor;
void main()
{
    FragColor.r = texelFetch(audioR, ivec2(int(gl_FragCoord.x), 0), 0).r - diff;
}
