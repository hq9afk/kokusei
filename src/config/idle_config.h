#pragma once

#include <cstdint>

// activity pulse
constexpr uint32_t kIdleRecentActivityPulseSeconds = 2;

// overlay fade
constexpr float kIdleOverlayFadeMs = 400.0f;
constexpr uint64_t kIdleAmbientFadeOwner = 1;
constexpr uint64_t kIdleScreensaverFadeOwner = 2;

// screensaver logo
constexpr float kIdleLogoSpeed = 90.0f;
constexpr float kIdleLogoSize = 200.0f;

// layer-shell namespace
constexpr const char *kIdleOverlayLayerNamespace = "kokusei-idle-overlay";
