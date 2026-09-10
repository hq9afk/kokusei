#pragma once

#include <cstdint>

// layer namespace
inline constexpr const char *kLockLayerNamespace = "kokusei-lock";

// background & dot
inline constexpr float kLockBgBorderWidth = 5.0f;

inline constexpr float kLockDotSize = 18.0f;

// fonts
inline constexpr float kLockFontIcon = 200.0f;
inline constexpr float kLockFontClock = 120.0f;
inline constexpr float kLockFontDate = 34.0f;
inline constexpr float kLockFontNormal = 20.0f;
inline constexpr float kLockFontMono = 18.0f;

// card layout
inline constexpr float kLockCardHeightMult = 0.7f;
inline constexpr float kLockCardRatio = 16.0f / 9.0f;
inline constexpr float kLockCardRadius = 40.0f;
inline constexpr float kLockPanelGap = 25.0f;
inline constexpr float kLockCenterWidth = 560.0f;
inline constexpr float kLockCenterRefHeight = 1440.0f;

// icon box
inline constexpr float kLockIconBoxMargin = 20.0f;

// clock/date/avatar/input gaps
inline constexpr float kLockClockGap = 10.0f;
inline constexpr float kLockGapClockDate = 6.0f;
inline constexpr float kLockGapDateAvatar = 40.0f;
inline constexpr float kLockGapAvatarInput = 34.0f;
inline constexpr float kLockGapInputMessage = 18.0f;

// input & pill
inline constexpr float kLockInputHeight = 56.0f;
inline constexpr float kLockInputWidthFrac = 0.85f;
inline constexpr float kLockPillPad = 12.0f;
inline constexpr float kLockPillIconSize = 26.0f;
inline constexpr float kLockPillButtonSize = 44.0f;

// profile
inline constexpr float kLockProfileBorderWidth = 4.0f;
inline constexpr float kLockProfileSize = 180.0f;

// side panel
inline constexpr float kLockSidePanelRadius = 24.0f;
inline constexpr float kLockSidePanelPad = 20.0f;

// fetch
inline constexpr float kLockFetchLineGap = 6.0f;
inline constexpr float kLockFetchChipPad = 8.0f;
inline constexpr float kLockFetchColorBox = 26.0f;
inline constexpr float kLockFetchColorGap = 10.0f;

// card chrome
inline constexpr float kLockCardBorderWidth = 2.0f;
inline constexpr float kLockCardHeaderGap = 12.0f;
inline constexpr float kLockBatteryRowGap = 8.0f;
inline constexpr float kLockBatteryIconGap = 10.0f;
inline constexpr float kLockBatteryBarHeight = 6.0f;
inline constexpr float kLockBatteryBarRadius = 3.0f;

// media
inline constexpr float kLockMediaArt = 116.0f;
inline constexpr float kLockMediaBtnSize = 40.0f;
inline constexpr float kLockMediaBtnGap = 12.0f;
inline constexpr float kLockMediaTextGap = 4.0f;

// resource gauges
inline constexpr float kLockResTileGap = 14.0f;
inline constexpr float kLockResTileRadius = 18.0f;
inline constexpr float kLockResIconSize = 26.0f;
inline constexpr float kLockResValueFont = 32.0f;
inline constexpr float kLockResTempWarnC = 85.0f;
inline constexpr float kLockResGaugeStrokeRatio = 6.0f / 68.0f;
inline constexpr float kLockResGaugeIconValueGap = 2.0f;
inline constexpr float kLockResGaugeLabelGap = 6.0f;
inline constexpr const char *kLockResGaugeGpuColorHex = "#3FB6C8";

// notifications
inline constexpr float kLockNotifCardGap = 8.0f;
inline constexpr float kLockNotifCardPad = 12.0f;
inline constexpr float kLockNotifCardRadius = 14.0f;
inline constexpr int kLockNotifMaxCards = 6;

// scale state
inline constexpr float kLockScaleHidden = 0.0f;
inline constexpr float kLockScaleFull = 1.0f;

// animation durations
inline constexpr float kLockAnimContentFadeInMs = 350.0f;
inline constexpr float kLockAnimContentFadeOutMs = 200.0f;
inline constexpr float kLockAnimContentScaleInMs = 400.0f;
inline constexpr float kLockAnimContentScaleOutMs = 300.0f;
inline constexpr float kLockAnimExpandMs = 400.0f;
inline constexpr float kLockAnimFastMs = 100.0f;
inline constexpr float kLockAnimIconFadeInMs = 250.0f;
inline constexpr float kLockAnimIconFadeOutMs = 300.0f;
inline constexpr float kLockAnimShrinkMs = 350.0f;
inline constexpr float kLockAnimSpinMs = 500.0f;

// fail timer
inline constexpr float kLockTimerFailMs = 3000.0f;

// text strings
inline constexpr const char *kLockFailText = "Skill Issue";
inline constexpr const char *kLockPlaceholderText = "Enter your password";
inline constexpr const char *kLockLoadingText = "Loading...";

// avatar fps
inline constexpr float kLockAvatarFps = 15.0f;

enum LockAnimOwner : uint64_t {
    kLockOwnerPanelScale = 1,
    kLockOwnerPanelRotation = 2,
    kLockOwnerPanelWidth = 3,
    kLockOwnerPanelHeight = 4,
    kLockOwnerIconAlpha = 5,
    kLockOwnerContentAlpha = 6,
    kLockOwnerContentScale = 7,
    kLockOwnerDotRowX = 8,
    kLockOwnerSequence = 9,
    kLockOwnerAvatarFrame = 10,
    kLockOwnerDotBase = 1000,
};
