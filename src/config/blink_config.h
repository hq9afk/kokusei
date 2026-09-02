#pragma once

#include <cstdint>

constexpr uint32_t kBlinkRecentActivityPulseSeconds = 2;

constexpr float kBlinkOverlayFadeMs = 400.0f;
constexpr uint64_t kBlinkAmbientFadeOwner = 1;
constexpr uint64_t kBlinkScreensaverFadeOwner = 2;

constexpr float kBlinkLogoSpeed = 90.0f;
constexpr float kBlinkLogoSize = 200.0f;

constexpr const char *kBlinkOverlayLayerNamespace = "kokusei-blink-overlay";
