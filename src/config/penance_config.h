#pragma once

#include <cstdint>

// layer namespace
inline constexpr const char *kPenanceLayerNamespace = "kokusei-penance";

// background & dot
inline constexpr float kPenanceBgBorderWidth = 5.0f;

inline constexpr float kPenanceDotSize = 18.0f;

// fonts
inline constexpr float kPenanceFontIcon = 200.0f;
inline constexpr float kPenanceFontClock = 120.0f;
inline constexpr float kPenanceFontDate = 34.0f;
inline constexpr float kPenanceFontNormal = 20.0f;
inline constexpr float kPenanceFontMono = 18.0f;

// card layout
inline constexpr float kPenanceCardHeightMult = 0.7f;
inline constexpr float kPenanceCardRatio = 16.0f / 9.0f;
inline constexpr float kPenanceCardRadius = 40.0f;
inline constexpr float kPenancePanelGap = 25.0f;
inline constexpr float kPenanceCenterWidth = 560.0f;
inline constexpr float kPenanceCenterRefHeight = 1440.0f;

// icon box
inline constexpr float kPenanceIconBoxMargin = 20.0f;

// clock/date/avatar/input gaps
inline constexpr float kPenanceClockGap = 10.0f;
inline constexpr float kPenanceGapClockDate = 6.0f;
inline constexpr float kPenanceGapDateAvatar = 40.0f;
inline constexpr float kPenanceGapAvatarInput = 34.0f;
inline constexpr float kPenanceGapInputMessage = 18.0f;

// input & pill
inline constexpr float kPenanceInputHeight = 56.0f;
inline constexpr float kPenanceInputWidthFrac = 0.85f;
inline constexpr float kPenancePillPad = 12.0f;
inline constexpr float kPenancePillIconSize = 26.0f;
inline constexpr float kPenancePillButtonSize = 44.0f;

// profile
inline constexpr float kPenanceProfileBorderWidth = 4.0f;
inline constexpr float kPenanceProfileSize = 180.0f;

// side panel
inline constexpr float kPenanceSidePanelRadius = 24.0f;
inline constexpr float kPenanceSidePanelPad = 20.0f;

// fetch
inline constexpr float kPenanceFetchLineGap = 6.0f;
inline constexpr float kPenanceFetchChipPad = 8.0f;
inline constexpr float kPenanceFetchColorBox = 26.0f;
inline constexpr float kPenanceFetchColorGap = 10.0f;

// card chrome
inline constexpr float kPenanceCardBorderWidth = 2.0f;
inline constexpr float kPenanceCardHeaderGap = 12.0f;
inline constexpr float kPenanceBatteryRowGap = 8.0f;
inline constexpr float kPenanceBatteryIconGap = 10.0f;
inline constexpr float kPenanceBatteryBarHeight = 6.0f;
inline constexpr float kPenanceBatteryBarRadius = 3.0f;

// media
inline constexpr float kPenanceMediaArt = 116.0f;
inline constexpr float kPenanceMediaBtnSize = 40.0f;
inline constexpr float kPenanceMediaBtnGap = 12.0f;
inline constexpr float kPenanceMediaTextGap = 4.0f;

// resource gauges
inline constexpr float kPenanceResTileGap = 14.0f;
inline constexpr float kPenanceResTileRadius = 18.0f;
inline constexpr float kPenanceResIconSize = 26.0f;
inline constexpr float kPenanceResValueFont = 32.0f;
inline constexpr float kPenanceResTempWarnC = 85.0f;
inline constexpr float kPenanceResGaugeStrokeRatio = 6.0f / 68.0f;
inline constexpr float kPenanceResGaugeIconValueGap = 2.0f;
inline constexpr float kPenanceResGaugeLabelGap = 6.0f;
inline constexpr const char *kPenanceResGaugeGpuColorHex = "#3FB6C8";

// notifications
inline constexpr float kPenanceNotifCardGap = 8.0f;
inline constexpr float kPenanceNotifCardPad = 12.0f;
inline constexpr float kPenanceNotifCardRadius = 14.0f;
inline constexpr int kPenanceNotifMaxCards = 6;

// scale state
inline constexpr float kPenanceScaleHidden = 0.0f;
inline constexpr float kPenanceScaleFull = 1.0f;

// animation durations
inline constexpr float kPenanceAnimContentFadeInMs = 350.0f;
inline constexpr float kPenanceAnimContentFadeOutMs = 200.0f;
inline constexpr float kPenanceAnimContentScaleInMs = 400.0f;
inline constexpr float kPenanceAnimContentScaleOutMs = 300.0f;
inline constexpr float kPenanceAnimExpandMs = 400.0f;
inline constexpr float kPenanceAnimFastMs = 100.0f;
inline constexpr float kPenanceAnimIconFadeInMs = 250.0f;
inline constexpr float kPenanceAnimIconFadeOutMs = 300.0f;
inline constexpr float kPenanceAnimShrinkMs = 350.0f;
inline constexpr float kPenanceAnimSpinMs = 500.0f;

// fail timer
inline constexpr float kPenanceTimerFailMs = 3000.0f;

// text strings
inline constexpr const char *kPenanceFailText = "Skill Issue";
inline constexpr const char *kPenancePlaceholderText = "Enter your password";
inline constexpr const char *kPenanceLoadingText = "Loading...";

// avatar fps
inline constexpr float kPenanceAvatarFps = 15.0f;

enum PenanceAnimOwner : uint64_t {
    kPenanceOwnerPanelScale = 1,
    kPenanceOwnerPanelRotation = 2,
    kPenanceOwnerPanelWidth = 3,
    kPenanceOwnerPanelHeight = 4,
    kPenanceOwnerIconAlpha = 5,
    kPenanceOwnerContentAlpha = 6,
    kPenanceOwnerContentScale = 7,
    kPenanceOwnerDotRowX = 8,
    kPenanceOwnerSequence = 9,
    kPenanceOwnerAvatarFrame = 10,
    kPenanceOwnerDotBase = 1000,
};
