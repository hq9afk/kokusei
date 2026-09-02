#pragma once

#include "render/palette.h"

constexpr float kStilettoFontPx = 20.0f;
constexpr float kStilettoCellWidth = 12.0f;
constexpr float kStilettoCellHeight = 24.0f;

constexpr float kStilettoBoldChance = 0.5f;
constexpr float kStilettoFadeAlpha = 0.05f;
constexpr float kStilettoFallIntervalMs = 45.0f;
constexpr float kStilettoResetChance = 0.025f;

constexpr int kStilettoDefaultWindowWidth = 480;
constexpr int kStilettoDefaultWindowHeight = 600;

inline constexpr Color kStilettoWindowBackground = {0.0f, 0.0f, 0.0f, 0.7f};

inline constexpr Color kStilettoHeadColor = palette::text;
inline constexpr Color kStilettoTailColor = palette::accent;
