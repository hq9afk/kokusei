#pragma once

#include "render/palette.h"

constexpr float kResonanceBarOpacity = 0.6f;
constexpr float kResonanceBarHeightRatio = 0.7f;
constexpr float kResonanceBarRadius = 3.0f;
constexpr float kResonanceBarSpacing = 7.0f;
constexpr float kResonanceBarWidth = 10.0f;
constexpr float kResonanceBarsAnimDurationMs = 60.0f;

constexpr int kResonanceDefaultWindowWidth = 480;
constexpr int kResonanceDefaultWindowHeight = 350;
inline constexpr Color kResonanceWindowBackground = {0.0f, 0.0f, 0.0f, 0.7f};

constexpr int kSpectrumFftSize = 4096;
constexpr int kSpectrumIdleThreshold = 30;
constexpr int kSpectrumLowerCutoffHz = 50;
constexpr int kSpectrumUpperCutoffHz = 12000;
constexpr float kSpectrumNoiseReduction = 0.77f;
constexpr bool kSpectrumSmoothing = true;
