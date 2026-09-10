void defaultAudioValues()
{
    audio.value = 0.0;
    audio.bass = 0.0;
    audio.mixing = 0.5;
    audio.multiplier = 7.0;
    audio.bassMultiplier = 5.0;
    audio.exponentiationFactor = 1.02;
    audio.samplePoints[0] = 0.1;
    audio.samplePoints[1] = 0.2;
    audio.samplePoints[2] = 0.3;
    audio.samplePoints[3] = 0.4;
    audio.samplePoints[4] = 0.5;
    audio.samplePoints[5] = 0.6;
    audio.samplePoints[6] = 0.7;
    audio.samplePoints[7] = 0.8;
    audio.samplePoints[8] = 0.9;
    audio.samplePointsDifferences[0] = 0.05;
    audio.samplePointsDifferences[1] = 0.05;
    audio.samplePointsDifferences[2] = 0.05;
    audio.samplePointsDifferences[3] = 0.05;
    audio.samplePointsDifferences[4] = 0.05;
    audio.samplePointsDifferences[5] = 0.05;
    audio.samplePointsDifferences[6] = 0.05;
    audio.samplePointsDifferences[7] = 0.05;
    audio.samplePointsDifferences[8] = 0.05;
    audio.intermediateAudios[0] = 0.0;
    audio.intermediateAudios[1] = 0.0;
    audio.intermediateAudios[2] = 0.0;
    audio.intermediateAudios[3] = 0.0;
    audio.intermediateAudios[4] = 0.0;
    audio.intermediateAudios[5] = 0.0;
    audio.intermediateAudios[6] = 0.0;
    audio.intermediateAudios[7] = 0.0;
}
void defaultBaseFormValues()
{
    baseForm.type = 0;
    baseForm.scale = vec3(2.0);
    baseForm.numParticles = vec3(resolution.xy, 1);
    baseForm.zSize = 100.0;
    baseForm.rotations = IDENTITY_MATRIX;
    baseForm.rotationCenter = vec3(resolution.xy / 2.0, 0);
}
void defaultParticleValues()
{
    particle.color = vec4(0, 0, 1, 1);
    particle.size = 3;
    particle.feather = 0.5;
    particle.position = vec3(gl_FragCoord.xy, 0);
    particle.opacityMultiplier = 1.0;
    particle.colorIntensityAddStrength = 0.1;
    particle.antiAlias = 4.5;
}
void defaultFractalFieldValues()
{
    fractalField.octaveMultiplier = 0.5;
    fractalField.octaveScale = 1.5;
    fractalField.complexity = 3;
    fractalField.fScale = 10.0;
    fractalField.gamma = 1.0;
    fractalField.minVal = -1.0;
    fractalField.maxVal = 1.0;
    fractalField.noise = vec3(0);
    fractalField.affectOpacity = 0.0;
    fractalField.affectSize = 0.0;
    fractalField.loop = 0;
    fractalField.loopFrames = 200;
    fractalField.dimensions = vec4(1000);
    fractalField.displacementType = 0;
    fractalField.offset = 0.0;
    fractalField.noiseMultiplier = 1.0;
    fractalField.constantNoiseMultiplier = 0.0;
    fractalField.displacements = vec3(100);
    fractalField.flows = vec4(0, 0, 0, 2.);
}
void defaultSphereValues()
{
    sphere.radius = 0.0;
    sphere.feather = 0.0;
    sphere.scale = vec3(1);
    sphere.strength = 1.0;
    sphere.center = vec3(resolution.xy / 2.0, 0);
}
void setAudio()
{
    float audioRadius = (max(AUDIO1D(audioR,audio.samplePoints[0]).x, AUDIO1D(audioR,audio.samplePoints[0] + audio.samplePointsDifferences[0]).x) + max(AUDIO1D(audioL,audio.samplePoints[0]).x, AUDIO1D(audioL,audio.samplePoints[0] + audio.samplePointsDifferences[0]).x)) / 2.0;
    float audioFractal1 = (max(AUDIO1D(audioR,audio.samplePoints[1]).x, AUDIO1D(audioR,audio.samplePoints[1] + audio.samplePointsDifferences[1]).x) + max(AUDIO1D(audioL,audio.samplePoints[1]).x, AUDIO1D(audioL,audio.samplePoints[1] + audio.samplePointsDifferences[1]).x)) / 2.0;
    float audioFractal2 = (max(AUDIO1D(audioR,audio.samplePoints[2]).x, AUDIO1D(audioR,audio.samplePoints[2] + audio.samplePointsDifferences[2]).x) + max(AUDIO1D(audioL,audio.samplePoints[2]).x, AUDIO1D(audioL,audio.samplePoints[2] + audio.samplePointsDifferences[2]).x)) / 2.0;
    float audioFractal3 = (max(AUDIO1D(audioR,audio.samplePoints[3]).x, AUDIO1D(audioR,audio.samplePoints[3] + audio.samplePointsDifferences[3]).x) + max(AUDIO1D(audioL,audio.samplePoints[3]).x, AUDIO1D(audioL,audio.samplePoints[3] + audio.samplePointsDifferences[3]).x)) / 2.0;
    float audioFractal4 = (max(AUDIO1D(audioR,audio.samplePoints[4]).x, AUDIO1D(audioR,audio.samplePoints[4] + audio.samplePointsDifferences[4]).x) + max(AUDIO1D(audioL,audio.samplePoints[4]).x, AUDIO1D(audioL,audio.samplePoints[4] + audio.samplePointsDifferences[4]).x)) / 2.0;
    float audioFractal5 = (max(AUDIO1D(audioR,audio.samplePoints[5]).x, AUDIO1D(audioR,audio.samplePoints[5] + audio.samplePointsDifferences[5]).x) + max(AUDIO1D(audioL,audio.samplePoints[5]).x, AUDIO1D(audioL,audio.samplePoints[5] + audio.samplePointsDifferences[5]).x)) / 2.0;
    float audioFractal6 = (max(AUDIO1D(audioR,audio.samplePoints[6]).x, AUDIO1D(audioR,audio.samplePoints[6] + audio.samplePointsDifferences[6]).x) + max(AUDIO1D(audioL,audio.samplePoints[6]).x, AUDIO1D(audioL,audio.samplePoints[6] + audio.samplePointsDifferences[6]).x)) / 2.0;
    float audioFractal7 = (max(AUDIO1D(audioR,audio.samplePoints[7]).x, AUDIO1D(audioR,audio.samplePoints[7] + audio.samplePointsDifferences[7]).x) + max(AUDIO1D(audioL,audio.samplePoints[7]).x, AUDIO1D(audioL,audio.samplePoints[7] + audio.samplePointsDifferences[7]).x)) / 2.0;
    float audioFractal8 = (max(AUDIO1D(audioR,audio.samplePoints[8]).x, AUDIO1D(audioR,audio.samplePoints[8] + audio.samplePointsDifferences[8]).x) + max(AUDIO1D(audioL,audio.samplePoints[8]).x, AUDIO1D(audioL,audio.samplePoints[8] + audio.samplePointsDifferences[8]).x)) / 2.0;
    float audios[8] = float[8](audioFractal1, audioFractal2, audioFractal3, audioFractal4, audioFractal5, audioFractal6, audioFractal7, audioFractal8);
    float temp;
    temp = max(audios[0], audios[2]); audios[0] = min(audios[0], audios[2]); audios[2] = temp;
    temp = max(audios[1], audios[3]); audios[1] = min(audios[1], audios[3]); audios[3] = temp;
    temp = max(audios[4], audios[6]); audios[4] = min(audios[4], audios[6]); audios[6] = temp;
    temp = max(audios[5], audios[7]); audios[5] = min(audios[5], audios[7]); audios[7] = temp;
    temp = max(audios[0], audios[4]); audios[0] = min(audios[0], audios[4]); audios[4] = temp;
    temp = max(audios[1], audios[5]); audios[1] = min(audios[1], audios[5]); audios[5] = temp;
    temp = max(audios[2], audios[6]); audios[2] = min(audios[2], audios[6]); audios[6] = temp;
    temp = max(audios[3], audios[7]); audios[3] = min(audios[3], audios[7]); audios[7] = temp;
    temp = max(audios[0], audios[1]); audios[0] = min(audios[0], audios[1]); audios[1] = temp;
    temp = max(audios[2], audios[3]); audios[2] = min(audios[2], audios[3]); audios[3] = temp;
    temp = max(audios[4], audios[5]); audios[4] = min(audios[4], audios[5]); audios[5] = temp;
    temp = max(audios[6], audios[7]); audios[6] = min(audios[6], audios[7]); audios[7] = temp;
    temp = max(audios[2], audios[4]); audios[2] = min(audios[2], audios[4]); audios[4] = temp;
    temp = max(audios[3], audios[5]); audios[3] = min(audios[3], audios[5]); audios[5] = temp;
    temp = max(audios[1], audios[4]); audios[1] = min(audios[1], audios[4]); audios[4] = temp;
    temp = max(audios[3], audios[6]); audios[3] = min(audios[3], audios[6]); audios[6] = temp;
    temp = max(audios[1], audios[2]); audios[1] = min(audios[1], audios[2]); audios[2] = temp;
    temp = max(audios[3], audios[4]); audios[3] = min(audios[3], audios[4]); audios[4] = temp;
    temp = max(audios[5], audios[6]); audios[5] = min(audios[5], audios[6]); audios[6] = temp;
    audio.value = audio.multiplier * mix(mix(audios[7] * audios[6] - audios[1] * audios[0], audios[7] * audios[6], audios[5]), mix(audios[6] * mix(audios[7] - audios[0], audios[6] - audios[3], audios[7] * audios[6]) - pow(audios[1] * audios[0], audio.exponentiationFactor), audios[7] * audios[6], audios[5] * audios[4]), audio.mixing);
    audio.bass = audio.bassMultiplier * abs(audioRadius);
    audio.intermediateAudios[0] = audioFractal1;
    audio.intermediateAudios[1] = audioFractal2;
    audio.intermediateAudios[2] = audioFractal3;
    audio.intermediateAudios[3] = audioFractal4;
    audio.intermediateAudios[4] = audioFractal5;
    audio.intermediateAudios[5] = audioFractal6;
    audio.intermediateAudios[6] = audioFractal7;
    audio.intermediateAudios[7] = audioFractal8;
}
