#pragma once

#include <array>
#include <cmath>
#include <cstdint>

constexpr int kStarwardButtonCount = 8;
constexpr float kStarwardButtonSize = 110.0f;
constexpr float kStarwardButtonsRadius = 300.0f;
constexpr float kStarwardButtonCornerRadius = kStarwardButtonSize / 5.0f;
constexpr float kStarwardLogoSize = 250.0f;
constexpr float kStarwardBorderWidth = 5.0f;

constexpr float kStarwardGlyphPx = kStarwardButtonSize / 2.0f;

constexpr float kStarwardStartAngle = -static_cast<float>(M_PI) / 2.0f;
constexpr float kStarwardStepAngle =
    2.0f * static_cast<float>(M_PI) / kStarwardButtonCount;

constexpr float kStarwardLogoAnimMs = 600.0f;
constexpr float kStarwardButtonScaleMs = 140.0f;
constexpr float kStarwardButtonBorderMs = 160.0f;
constexpr float kStarwardHighlightScale = 1.05f;

constexpr float kStarwardHoldMs = 300.0f;
constexpr float kStarwardSlashMs = 120.0f;
constexpr float kStarwardSlashAdvanceFrac = 0.75f;
constexpr float kStarwardSlashOvershoot = 0.3f;
constexpr float kStarwardBurstMs = 500.0f;
constexpr float kStarwardPushMs = 260.0f;

constexpr float kStarwardFinishSpan = kStarwardButtonsRadius * 5.2f;
constexpr float kStarwardFinishRise = 230.0f;
constexpr float kStarwardFinishThick = 3.4f;
constexpr float kStarwardFinishSweep = 2.5f;
constexpr float kStarwardFinishIntensity = 2.8f;
constexpr float kStarwardFinishLingerIntensity = 1.4f;

constexpr float kStarwardExitSpread = 0.55f;
constexpr float kStarwardExitFadeMs = kStarwardBurstMs * 0.8f;

constexpr int kStarwardStarStep = 3;
constexpr float kStarwardBurstRingMax = kStarwardButtonsRadius * 1.5f;
constexpr float kStarwardBoltAmp = 13.0f;

constexpr uint64_t kStarwardLogoOwner = 1;
constexpr uint64_t kStarwardInputReadyOwner = 2;
constexpr uint64_t kStarwardCloseChainOwner = 3;
constexpr uint64_t kStarwardExitOwner = 4;
constexpr uint64_t kStarwardBurstOwner = 5;
constexpr uint64_t kStarwardHoldOwner = 6;

struct StarwardAction {
    const char *glyph_utf8;
    const char *command;
};

inline constexpr std::array<StarwardAction, kStarwardButtonCount>
    kStarwardActions = {{
        {"劍", "systemctl poweroff"},
        {"光", "systemctl reboot"},
        {"如", "kokusei penance"},
        {"我", "systemctl reboot --firmware-setup"},
        {"斬", ""},
        {"盡", ""},
        {"蕪", ""},
        {"雜", ""},
    }};
