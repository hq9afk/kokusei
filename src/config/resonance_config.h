#pragma once

#include "render/palette.h"

constexpr float kResonanceCanvasFraction = 0.9f;
constexpr int kResonanceCanvasMin = 200;

inline constexpr Color kResonanceWindowBackground = {0.0f, 0.0f, 0.0f, 0.7f};

constexpr int kResonanceDefaultWindow = 1000;

constexpr unsigned int kResonanceSampleRate = 11000;
constexpr int kResonanceChannels = 2;
constexpr int kResonanceSampleSize = 1024;
constexpr int kResonanceFragmentSize = 4096;
constexpr float kResonanceFftScale = 10.2f;
constexpr float kResonanceFftCutOff = 0.3f;

constexpr int kResonanceGravityAverageFrames = 5;
constexpr float kResonanceGravityStep = 4.2f;
constexpr float kResonanceSampleHybridWeight = 0.065f;
constexpr int kResonanceSampleMode = 0;
constexpr int kResonanceAdjacentSampleNums = 1;
constexpr float kResonanceSampleRange = 0.9f;
constexpr float kResonanceSampleScale = 8.0f;
constexpr float kResonanceSmoothFactor = 0.025f;
constexpr int kResonanceFps = 60;

constexpr int kResonanceFpsMin = 15;
constexpr int kResonanceFpsMax = 144;
constexpr float kResonanceParticleThinMin = 0.0f;
constexpr float kResonanceParticleThinMax = 0.95f;
constexpr int kResonanceParticleSizeMin = 1;
constexpr int kResonanceParticleSizeMax = 4;
constexpr int kResonanceComplexityMin = 1;
constexpr int kResonanceComplexityMax = 3;
constexpr float kResonanceGlowDirectionsMin = 4.0f;
constexpr float kResonanceGlowDirectionsMax = 32.0f;
constexpr float kResonanceGlowQualityMin = 2.0f;
constexpr float kResonanceGlowQualityMax = 8.0f;

struct ResonanceParams {
    int fps = kResonanceFps;
    float particle_thin = 0.12f;
    int particle_size = 4;
    int fractal_complexity = 3;
    float glow_directions = 16.0f;
    float glow_quality = 6.0f;
};
