#version 320 es
precision highp float;
precision highp sampler2D;
uniform sampler2D audioL;
uniform sampler2D audioR;
uniform vec2 u_resolution;
uniform float u_fade;
uniform vec3 u_accent;
uniform int u_barCount;
uniform float u_barWidth;
uniform float u_barSpacing;
uniform float u_barRadius;
uniform float u_barHeightRatio;
uniform float u_barOpacity;
uniform float u_minBarHeight;
out vec4 FragColor;

float barMagnitude(int i) {
    float p = (float(i) + 0.5) / float(u_barCount);
    float tc = pow(max(p, 1e-4), 1.8) * 0.5;
    float l = texture(audioL, vec2(tc, 0.5)).r;
    float r = texture(audioR, vec2(tc, 0.5)).r;
    return clamp(max(l, r), 0.0, 1.0);
}

void main() {
    float px = gl_FragCoord.x;
    float py = gl_FragCoord.y;

    float total = float(u_barCount) * u_barWidth +
                  float(u_barCount - 1) * u_barSpacing;
    float startX = (u_resolution.x - total) * 0.5;
    float rel = px - startX;
    if (rel < 0.0 || rel > total)
        discard;

    float slot = u_barWidth + u_barSpacing;
    int idx = int(floor(rel / slot));
    if (idx < 0 || idx >= u_barCount)
        discard;
    float within = rel - float(idx) * slot;
    if (within > u_barWidth)
        discard;

    float mag = barMagnitude(idx);
    float h = max(u_minBarHeight, mag * u_barHeightRatio * u_resolution.y);
    if (py > h)
        discard;

    float rad = min(u_barRadius, u_barWidth * 0.5);
    vec2 d = vec2(min(within, u_barWidth - within), h - py);
    float aa = 1.0;
    if (d.x < rad && d.y < rad) {
        float dist = length(vec2(rad) - d);
        aa = 1.0 - smoothstep(rad - 1.0, rad + 1.0, dist);
    }
    if (aa <= 0.0)
        discard;

    float a = u_barOpacity * u_fade * aa;
    FragColor = vec4(u_accent * a, a);
}
