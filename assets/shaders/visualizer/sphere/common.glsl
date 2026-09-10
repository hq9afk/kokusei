struct BaseForm {
    int type;
    vec3 scale;
    vec3 numParticles;
    float zSize;
    mat3 rotations;
    vec3 rotationCenter;
} baseForm;
struct Audio {
    float multiplier;
    float bassMultiplier;
    float mixing;
    float bass;
    float exponentiationFactor;
    float samplePoints[9];
    float samplePointsDifferences[9];
    float intermediateAudios[8];
    float value;
} audio;
struct Particle {
    vec4 color;
    float opacityMultiplier;
    int size;
    float feather;
    float colorIntensityAddStrength;
    float antiAlias;
    vec3 position;
} particle;
struct FractalField {
    float octaveMultiplier;
    float octaveScale;
    int complexity;
    float fScale;
    vec4 dimensions;
    float gamma;
    float minVal;
    float maxVal;
    float offset;
    float noiseMultiplier;
    float constantNoiseMultiplier;
    float affectOpacity;
    float affectSize;
    int loop;
    int loopFrames;
    int displacementType;
    vec3 displacements;
    vec4 flows;
    vec3 noise;
} fractalField;
struct Sphere {
    float radius;
    float feather;
    float strength;
    vec3 center;
    vec3 scale;
} sphere;
struct Glow {
    float blendMode;
    float mixAlpha;
    float offsetAngle;
    float maxAngle;
    vec2 size;
    float intensity;
    float directions;
    vec2 coords;
    float quality;
    vec4 color;
    float brightnessOffset;
    float lightStrength;
    float onTop;
};
#ifndef TWOPI
#define TWOPI (6.2831853071794)
#endif
#ifndef PI
#define PI (3.1415926535897)
#endif
#ifndef RAD_PI
#define RAD_PI (PI / 180.)
#endif
mat3 rotateX(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(1, 0, 0),
        vec3(0, cosAngle, sinAngle),
        vec3(0, -sinAngle, cosAngle));
    return rotationMatrix;
}
mat3 rotateY(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(cosAngle, 0, -sinAngle),
        vec3(0, 1, 0),
        vec3(sinAngle, 0, cosAngle));
    return rotationMatrix;
}
mat3 rotateZ(float angle)
{
    float angleRads = angle * RAD_PI;
    float sinAngle = sin(angleRads), cosAngle = cos(angleRads);
    mat3x3 rotationMatrix = mat3x3(
        vec3(cosAngle, sinAngle, 0),
        vec3(-sinAngle, cosAngle, 0),
        vec3(0, 0, 1));
    return rotationMatrix;
}
#define IDENTITY_MATRIX mat3(vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1))
#define colorTracking 0
uniform vec3 u_accent;
uniform highp float particleThin;
uniform int u_particleSize;
uniform int u_complexity;
uniform highp float u_glowDirections;
uniform highp float u_glowQuality;
void init()
{
    audio.multiplier = 6.4;
    audio.bassMultiplier = .5263 * resolution.x;
}
void setProps()
{
    particle.color = vec4(u_accent, 0.3);
    particle.size = u_particleSize;
    particle.feather = 1.0;
    particle.colorIntensityAddStrength = 0.38;
    particle.antiAlias = 8.5;
    fractalField.octaveMultiplier = 0.25;
    fractalField.octaveScale = 1.0;
    fractalField.complexity = u_complexity;
    fractalField.fScale = 9.473;
    fractalField.gamma = 1.0;
    fractalField.minVal = -5.0;
    fractalField.maxVal = 5.0;
    fractalField.flows = vec4(0, 3.8, 0, 1.3);
    fractalField.displacements = vec3(.3884 * resolution.x, .3884 * resolution.x - 20.0, .3884 * resolution.x - 5.0);
    sphere.radius = .7236 * resolution.x;
    sphere.feather = 0.45;
}
void modifyNoiseCoordinates(inout vec4 coords) {}
void setPropsWithNoise() {}
void modifySphericalDisplacement() {}
void setGlow0(inout Glow glow)
{
    glow.blendMode = 1.0;
    glow.mixAlpha = 1.0;
    glow.intensity = 1.0;
    glow.size = vec2(18);
    glow.directions = u_glowDirections;
    glow.quality = u_glowQuality;
    glow.color = vec4(u_accent, 1.0);
    glow.brightnessOffset = .0;
    glow.lightStrength = .5;
}
