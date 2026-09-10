#pragma once

#include <cstdint>

inline constexpr const char *kWallpaperLayerNamespace = "kokusei-wallpaper";

enum class WallpaperTransition : uint8_t {
    None,
    Fade,
    Wipe,
    Disc,
    Stripes,
    Zoom,
    Honeycomb,
    Random,
};

// ponytail: fixed transition timing; promote to config knobs only if asked.
inline constexpr float kWallpaperTransitionDurationMs = 900.0f;
inline constexpr float kWallpaperTransitionSmoothness = 0.3f;
