uniform sampler2D tex;
vec4 getTrackedColors()
{
    uint colorsV4 = 0u;
    vec4 fetchedColors = vec4(0);
    colorsV4 = imageAtomicExchange(atomicImageTexture1, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.x = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture2, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.y = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture3, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.z = float(colorsV4);
    colorsV4 = 0u;
    colorsV4 = imageAtomicExchange(atomicImageTexture4, ivec2(gl_FragCoord.xy), colorsV4);
    fetchedColors.w = float(colorsV4);
    return fetchedColors;
}
void main()
{
    defaultBaseFormValues();
    defaultParticleValues();
    defaultFractalFieldValues();
    defaultSphereValues();
    init();
    setAudio();
    setProps();
    uint depth = 0u;
    depth = imageAtomicExchange(atomicImageTexture0, ivec2(gl_FragCoord.xy), depth);
    vec4 noiseCoords = vec4(1, 1, 1, 0);
    modifyNoiseCoordinates(noiseCoords);
    fractalField.noise = vec3(1);
    setPropsWithNoise();
    modifySphericalDisplacement();
    particle.color = ((colorTracking) == 0 ? particle.color : (getTrackedColors() / float(depth)));
    float actualDepth = float(depth) / (100000.);
    FragColor = step(0.0, float(depth)) * vec4(particle.color.xyz * particle.color.w, particle.color.w);
    FragColor *= (pow(actualDepth, particle.colorIntensityAddStrength)) * (1.0 - pow(1.0 - particle.color.w, actualDepth));
}
