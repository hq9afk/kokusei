float octaveNoise(vec4 p, vec4 flow, vec4 rep)
{
    float total = 0.0;
    float frequency = 1.0;
    float amplitude = 1.0;
    float value = 0.0;
    for (int i = 0; i < fractalField.complexity; i += 1) {
        vec4 fractalFieldInput = p;
        modifyNoiseCoordinates(fractalFieldInput);
        fractalFieldInput += flow * time;
        fractalFieldInput *= frequency;
        value += (pnoise(vec4((fractalFieldInput)), rep)) * amplitude;
        total += amplitude;
        amplitude *= fractalField.octaveMultiplier;
        frequency *= fractalField.octaveScale;
    }
    return value / total;
}
float fbm3(vec4 p, vec4 flow)
{
    vec4 flowXLoopFrames = flow * float(fractalField.loopFrames);
    vec4 rep = vec4(fractalField.loop * ivec4(fractalField.fScale * flowXLoopFrames / fractalField.dimensions));
    flowXLoopFrames = mix(vec4(1), flowXLoopFrames, 1. - step(abs(flowXLoopFrames), vec4(0)));
    vec4 newFScale = mix(fractalField.dimensions * rep / (flowXLoopFrames), vec4(fractalField.fScale), vec4(1) - abs(float(fractalField.loop) * sign(flow)));
    p = newFScale * p / fractalField.dimensions;
    flow *= newFScale / fractalField.dimensions;
    vec3 originalSphereCenter = sphere.center;
    sphere.center *= newFScale.xyz / fractalField.dimensions.xyz;
    float oN = (fractalField.constantNoiseMultiplier + audio.value) * (octaveNoise(p, flow, rep));
    oN = sign(oN) * pow(abs(oN), fractalField.gamma);
    sphere.center = originalSphereCenter;
    oN = fractalField.offset + fractalField.noiseMultiplier * oN;
    oN = clamp(oN, fractalField.minVal, fractalField.maxVal);
    return oN;
}
vec3 sphereCoords(vec3 particleCoords, float zLayer, float zLayerDistance)
{
    vec3 newPos;
    float u = (TWOPI * (((particleCoords.x) / (resolution.x))));
    float v = PI * (particleCoords.y / resolution.y);
    newPos.x = resolution.x * sin(u) * sin(v);
    newPos.z = baseForm.zSize * cos(u) * sin(v);
    newPos.y = resolution.y * cos(v);
    newPos.xy += resolution.xy / 2.;
    newPos -= zLayer * (vec3(resolution.xy / 2., baseForm.zSize / 2.) / baseForm.numParticles.z) * normalize(newPos - vec3(resolution.xy / 2.0, 0));
    return newPos;
}
vec3 transformedCoords(vec3 particleCoords)
{
    particleCoords = (baseForm.rotations) * (particleCoords - baseForm.rotationCenter);
    return (particleCoords + vec3(resolution.xy / 2.0, 0));
}
void processZLayer(int zLayer, float zLayerDistance)
{
    vec3 particleCoords = particle.position;
    particleCoords = mix(particleCoords, sphereCoords(particleCoords, float(zLayer), zLayerDistance), float(baseForm.type));
    vec4 old = vec4(particleCoords, 0);
    vec3 displacementValues = vec3(0);
    vec4 flows = fractalField.flows;
    float xFBM3 = fbm3(old.xyzw, flows);
    float yFBM3 = fbm3(old.yzxw, flows.yzxw);
    float zFBM3 = fbm3(old.zxyw, flows.zxyw);
    fractalField.noise = vec3(xFBM3, yFBM3, zFBM3);
    setPropsWithNoise();
    displacementValues.xyz += mix(vec3((fractalField.displacements.x) * xFBM3, (fractalField.displacements.y) * yFBM3, (fractalField.displacements.z) * zFBM3), (fractalField.displacements.x) * xFBM3 * normalize(particleCoords.xyz - vec3(resolution.xy / 2.0, 0)), float(fractalField.displacementType));
    particleCoords.xyz += displacementValues;
    float radius = sphere.radius, blurSize = particle.antiAlias / resolution.y;
    radius += audio.bass;
    vec3 sphereCenterCoords = sphere.center;
    vec3 vectorFromSphereCenter = (particleCoords - sphereCenterCoords);
    vec3 normalizedVector = normalize(vectorFromSphereCenter);
    vec3 newPos = (sphereCenterCoords + radius * normalizedVector);
    float diff = length(newPos - particleCoords);
    diff *= (clamp((smoothstep(0.0, sphere.feather * (radius), diff)) + blurSize, blurSize, 1.0 + blurSize));
    particleCoords += step(length(vectorFromSphereCenter), radius) * sphere.strength * diff * normalizedVector * sphere.scale;
    modifySphericalDisplacement();
    particle.size = int(max(0., float(particle.size) + fractalField.affectSize * (xFBM3 + yFBM3 + zFBM3)));
    particle.opacityMultiplier = max(particle.opacityMultiplier + fractalField.affectOpacity * (xFBM3 + yFBM3 + zFBM3), 0.);
    for (int i = -particle.size; i <= particle.size; i++)
        for (int j = -particle.size; j <= particle.size; j++) {
            float distanceFromParticleCenter = length(vec2(i, j));
            distanceFromParticleCenter = mix(step(distanceFromParticleCenter, float(particle.size)), (1. - smoothstep(float(particle.size) - particle.feather * float(particle.size), float(particle.size), distanceFromParticleCenter)), particle.feather);
            distanceFromParticleCenter *= 100000. * particle.opacityMultiplier;
            vec3 finalCoords = vec3(transformedCoords(particleCoords.xyz) + vec3(i, j, 0)) / baseForm.scale;
            finalCoords += vec3(resolution.xy / 2., 0) * (1. - 1. / (baseForm.scale));
            uint depth = imageAtomicAdd(atomicImageTexture0, ivec2(finalCoords.xy), uint((distanceFromParticleCenter)));
            if ((colorTracking) == 1) {
                uint colorValue = imageAtomicAdd(atomicImageTexture1, ivec2(finalCoords.xy), uint(clamp(particle.color.x * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture2, ivec2(finalCoords.xy), uint(clamp(particle.color.y * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture3, ivec2(finalCoords.xy), uint(clamp(particle.color.z * distanceFromParticleCenter, 0., 100000.)));
                colorValue = imageAtomicAdd(atomicImageTexture4, ivec2(finalCoords.xy), uint(clamp(particle.color.w * distanceFromParticleCenter, 0., 100000.)));
            }
        }
}
void main()
{
    defaultAudioValues();
    defaultBaseFormValues();
    defaultParticleValues();
    defaultFractalFieldValues();
    defaultSphereValues();
    init();
    setAudio();
    vec3 denom = vec3(baseForm.numParticles - step(float(baseForm.type), 0.0) * vec3(1, 1, 1));
    denom = max(denom, vec3(0.0001));
    ivec3 spaces = ivec3(round((vec3(resolution.xy, baseForm.zSize)) / denom));
    spaces -= int(step(float(baseForm.type), 0.0)) * ivec3(1, 1, 1);
    spaces.x = max(spaces.x, 1);
    spaces.y = max(spaces.y, 1);
    spaces.z = (baseForm.numParticles.z <= 1.0) ? int(baseForm.zSize) + 1 : max(spaces.z, 1);
    particle.position.z -= (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.;
    particle.position.xy += step(baseForm.numParticles.xy, vec2(1)) * vec2(resolution.xy / 2.);
    int currentZIndex = 0;
    for (float i = 0.0; i < baseForm.zSize; i += float(spaces.z)) {
        if (length(vec3(ivec3(mod(vec3(gl_FragCoord.xy, 0.0), vec3(spaces))))) > 0.0)
            discard;
        highp vec2 _pcell = floor(gl_FragCoord.xy);
        if (fract(sin(mod(dot(_pcell, vec2(127.1, 311.7)), 6.2831853)) * 43758.5453123) < particleThin)
            discard;
        setProps();
        processZLayer(currentZIndex, (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.);
        currentZIndex += 1;
        baseForm.rotations = IDENTITY_MATRIX;
        sphere.center = vec3(resolution.xy / 2., 0);
        particle.position.xy = gl_FragCoord.xy;
        particle.position.z = i + float(spaces.z) - (baseForm.numParticles.z - 1.0) * float(spaces.z) / 2.;
        particle.position.xy += step(baseForm.numParticles.xy, vec2(1)) * vec2(resolution.xy / 2.);
    }
}
