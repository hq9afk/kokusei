#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
#ifndef TWOPI
#define TWOPI 6.28318530718
#endif
#ifndef PI
#define PI 3.14159265359
#endif
#define sinusoidal(x) ((0.5 * sin((PI * (x)) - (PI / 2.0))) + 0.5)
#define ROUND_FORMULA sinusoidal
uniform int sample_mode;
uniform float sample_hybrid_weight;
uniform float sample_scale;
uniform float sample_range;
uniform float smooth_factor;
float scale_audio(float idx)
{
    return -log((-(sample_range)*idx) + 1.0) / (sample_scale);
}
float smooth_audio(in sampler2D tex, int tex_sz, highp float idx)
{
    float smin = scale_audio(clamp(idx - smooth_factor, 0.0, 1.0)) * float(tex_sz),
          smax = scale_audio(clamp(idx + smooth_factor, 0.0, 1.0)) * float(tex_sz);
    float m = ((smax - smin) / 2.0), s, w;
    float rm = smin + m;
    if (sample_mode == 0) {
        float avg = 0.0, weight = 0.0;
        for (s = smin; s <= smax; s += 1.0) {
            w = ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            weight += w;
            avg += texelFetch(tex, ivec2(int(round(s)), 0), 0).r * w;
        }
        avg /= weight;
        return avg;
    } else if (sample_mode == 2) {
        float vmax = 0.0, avg = 0.0, weight = 0.0, v;
        for (s = smin; s < smax; s += 1.0) {
            w = ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            weight += w;
            v = texelFetch(tex, ivec2(int(round(s)), 0), 0).r * w;
            avg += v;
            if (vmax < v)
                vmax = v;
        }
        return (vmax * (1.0 - sample_hybrid_weight)) + ((avg / weight) * sample_hybrid_weight);
    } else if (sample_mode == 1) {
        float vmax = 0.0, v;
        for (s = smin; s < smax; s += 1.0) {
            w = texelFetch(tex, ivec2(int(round(s)), 0), 0).r * ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0.0, 1.0));
            if (vmax < w)
                vmax = w;
        }
        return vmax;
    }
    return 0.0;
}
uniform sampler2D audioR;
out vec4 FragColor;
uniform int audioRSize;
uniform int adjacentSampleNums;
#define adjacent(I) FragColor.r += (1. - step(float(I), 1.)) * (smooth_audio(audioR, audioRSize, u + float(I - 1) * aRI) + smooth_audio(audioR, audioRSize, u - float(I - 1) * aRI));
void main()
{
    float u = gl_FragCoord.x / float(audioRSize);
    FragColor.r = 0.0;
    float aRI = 1. / float(audioRSize);
    adjacent(0);
    FragColor.r += (smooth_audio(audioR, audioRSize, u));
    FragColor.r /= 2. * (float(adjacentSampleNums) - 1.) + 1.;
}
