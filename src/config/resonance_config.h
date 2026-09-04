#pragma once

#include "render/palette.h"

constexpr int kResonanceSphereCanvas = 1000;
inline constexpr Color kResonanceWindowBackground = {0.0f, 0.0f, 0.0f, 0.7f};

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
