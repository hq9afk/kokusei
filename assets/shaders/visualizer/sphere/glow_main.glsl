float glowLightVal(float glowValue, float glowLightStrengthValue, float glowLightDistanceValue)
{
    return glowLightDistanceValue + glowLightDistanceValue / pow(max(glowValue, 1e-4), glowLightStrengthValue);
}
vec4 addColors(float blendMode, vec4 above, vec4 below)
{
    return above + (1. - blendMode * above.w) * below;
}
Glow glow0;
void defaultGlowValues(inout Glow glow)
{
    glow.blendMode = 1.0;
    glow.mixAlpha = 1.0;
    glow.offsetAngle = 0.0;
    glow.size = vec2(10);
    glow.intensity = .5;
    glow.directions = 8.0;
    glow.onTop = 0.0;
    glow.coords = gl_FragCoord.xy;
    glow.maxAngle = 360.0;
    glow.quality = 4.0;
    vec4 color = vec4(0.5, 0.5, 0.5, 1.0);
    glow.brightnessOffset = .0;
    glow.lightStrength = .5;
}
void main()
{
    defaultGlowValues(glow0);
    setGlow0(glow0);
    Glow glow;
    glow = glow0;
    vec2 uv = (glow.coords) / resolution.xy;
    vec4 prevColor = texture(tex, gl_FragCoord.xy / resolution.xy);
    vec2 glowRadius = (glow.size) / resolution.xy;
    vec4 Color = vec4(0);
    float glowOffsetValue = (float(glow.offsetAngle) / 360.) * TWOPI;
    for (float d = glowOffsetValue; d < (glow.maxAngle / 360. * TWOPI); d += TWOPI / (glow.directions))
    {
        for (float i = 1.0 / (glow.quality); i <= 1.0; i += 1.0 / (glow.quality))
        {
            vec2 coords = uv + glowRadius * i * vec2(cos(d), sin(d));
            if (coords.x > 0.0 && coords.x < 1.0 && coords.y > 0.0 && coords.y < 1.0)
                Color += texture(tex, coords);
        }
    }
    Color /= (glow.quality) * (glow.directions);
    FragColor = (vec4(glow.color.xyz * glow.color.w, glow.color.w)) * glow.intensity * length(Color);
    FragColor = addColors(glow.blendMode, mix(prevColor, FragColor, glow.onTop), mix(FragColor, prevColor, glow.onTop));
    FragColor *= glowLightVal(length(FragColor), glow.brightnessOffset, glow.lightStrength);
    FragColor.w = mix(prevColor.w, FragColor.w, glow.mixAlpha * 0.5);
    FragColor *= u_fade;
}
