#pragma once

#include <array>
#include <cmath>
#include <cstdint>

// button & logo geometry
constexpr int kLogoutButtonCount = 8;
constexpr float kLogoutButtonSize = 110.0f;
constexpr float kLogoutButtonsRadius = 300.0f;
constexpr float kLogoutButtonCornerRadius = kLogoutButtonSize / 5.0f;
constexpr float kLogoutLogoSize = 250.0f;
constexpr float kLogoutBorderWidth = 5.0f;

// glyph size
constexpr float kLogoutGlyphPx = kLogoutButtonSize / 2.0f;

// ring angle math
constexpr float kLogoutStartAngle = -static_cast<float>(M_PI) / 2.0f;
constexpr float kLogoutStepAngle =
    2.0f * static_cast<float>(M_PI) / kLogoutButtonCount;

// entry animation
constexpr float kLogoutLogoAnimMs = 600.0f;
constexpr float kLogoutButtonScaleMs = 140.0f;
constexpr float kLogoutButtonBorderMs = 160.0f;
constexpr float kLogoutHighlightScale = 1.05f;

// choreography timing
constexpr float kLogoutHoldMs = 300.0f;
constexpr float kLogoutSlashMs = 120.0f;
constexpr float kLogoutSlashAdvanceFrac = 0.75f;
constexpr float kLogoutSlashOvershoot = 0.3f;
constexpr float kLogoutBurstMs = 500.0f;
constexpr float kLogoutPushMs = 260.0f;

// finishing slash
constexpr float kLogoutFinishSpan = kLogoutButtonsRadius * 5.2f;
constexpr float kLogoutFinishRise = 230.0f;
constexpr float kLogoutFinishThick = 3.4f;
constexpr float kLogoutFinishSweep = 2.5f;
constexpr float kLogoutFinishIntensity = 2.8f;
constexpr float kLogoutFinishLingerIntensity = 1.4f;

// exit
constexpr float kLogoutExitSpread = 0.55f;
constexpr float kLogoutExitFadeMs = kLogoutBurstMs * 0.8f;

// star path
constexpr int kLogoutStarStep = 3;
constexpr float kLogoutBurstRingMax = kLogoutButtonsRadius * 1.5f;
constexpr float kLogoutBoltAmp = 13.0f;

// animation owners
constexpr uint64_t kLogoutLogoOwner = 1;
constexpr uint64_t kLogoutInputReadyOwner = 2;
constexpr uint64_t kLogoutCloseChainOwner = 3;
constexpr uint64_t kLogoutExitOwner = 4;
constexpr uint64_t kLogoutBurstOwner = 5;
constexpr uint64_t kLogoutHoldOwner = 6;

struct LogoutAction {
    const char *glyph_utf8;
    const char *command;
};

// button action table
inline constexpr std::array<LogoutAction, kLogoutButtonCount>
    kLogoutActions = {{
        {"劍", "systemctl poweroff"},
        {"光", "systemctl reboot"},
        {"如", "kokusei lock"},
        {"我", "systemctl reboot --firmware-setup"},
        {"斬", ""},
        {"盡", ""},
        {"蕪", ""},
        {"雜", ""},
    }};
