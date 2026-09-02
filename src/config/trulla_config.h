#pragma once

#include <cstdint>

#include "render/icons.h"

enum class TrullaFieldId {
    None,
    ExpansePath,
    ExpanseDir,
    ExpanseAnimatedDir,
    AmbientTimeout,
    ScreensaverTimeout,
};

constexpr int kTrullaTabCount = 4;

inline constexpr const char *kTrullaDisplaysDefaultTag = "__default__";

constexpr float kTrullaRailItemHeight = 36.0f;
constexpr float kTrullaRailItemGap = 4.0f;
constexpr float kTrullaRailIconLabelGap = 10.0f;
constexpr float kTrullaRailPadding = 10.0f;
constexpr float kTrullaRailDividerGap = 16.0f;

constexpr float kTrullaProfileAvatarSize = 40.0f;
constexpr float kTrullaProfileAvatarBorderWidth = 2.0f;
constexpr float kTrullaProfileTopPadding = 4.0f;
constexpr float kTrullaProfileAvatarLabelGap = 10.0f;
constexpr float kTrullaProfileLineGap = 2.0f;
constexpr float kTrullaProfileBottomPadding = 12.0f;
constexpr float kTrullaProfileDividerGap = 12.0f;
constexpr float kTrullaRowHeight = 40.0f;
constexpr float kTrullaRowGap = 10.0f;
constexpr float kTrullaLabelWidth = 170.0f;
constexpr float kTrullaFieldHeight = 28.0f;
constexpr float kTrullaFieldWidth = 240.0f;
constexpr float kTrullaNumberFieldWidth = 72.0f;

constexpr uint64_t kTrullaAutohideToggleOwner = 3;
constexpr uint64_t kTrullaFillModeToggleOwner = 4;
constexpr uint64_t kTrullaFieldTypeAnimOwnerBase = 10000;

constexpr float kTrullaExpanseThumbSize = 115.0f;
constexpr float kTrullaExpanseThumbGap = 15.0f;
constexpr int kTrullaExpanseGridColumns = 5;
constexpr float kTrullaExpanseThumbRadius = 8.0f;
constexpr float kTrullaExpanseLabelPad = 6.0f;
constexpr float kTrullaExpanseGridInset = 5.0f;

constexpr float kTrullaExpanseScrollSpeed = 3.0f;

constexpr float kTrullaExpanseWarningPad = 10.0f;
constexpr float kTrullaMonitorChipHeight = 35.0f;
constexpr float kTrullaMonitorChipGap = 6.0f;

constexpr float kTrullaColumnStepperButtonSize = 28.0f;

constexpr float kTrullaDirBarHeight = 40.0f;
constexpr float kTrullaDirBarLabelMargin = 14.0f;
constexpr float kTrullaDirBarFieldMargin = 10.0f;
constexpr float kTrullaDirBarEdgeMargin = 8.0f;
constexpr float kTrullaDirBarButtonWidth = 72.0f;
constexpr float kTrullaDirBarButtonHeight = 28.0f;

constexpr float kTrullaScreenSelectorHeight = 35.0f;
constexpr float kTrullaScreenSelectorSpacing = 6.0f;
constexpr float kTrullaSelectorBorderWidth = 2.0f;
constexpr float kTrullaTileRadius = 6.0f;

constexpr float kTrullaToggleTileHeight = 48.0f;
constexpr float kTrullaToggleTileBorderWidth = 2.0f;
constexpr float kTrullaToggleTileContentMargin = 12.0f;
constexpr float kTrullaToggleTileContentSpacing = 10.0f;
constexpr float kTrullaGroupSpacingSm = 8.0f;

constexpr float kTrullaToggleTrackWidth = 36.0f;
constexpr float kTrullaToggleTrackHeight = 20.0f;
constexpr float kTrullaToggleKnobSize = 14.0f;
constexpr float kTrullaToggleKnobInset = 3.0f;

constexpr float kTrullaIdleResetIconSize = 20.0f;
constexpr int kTrullaIdleTimeoutMin = 10;
constexpr int kTrullaIdleTimeoutMax = 1800;

constexpr float kTrullaWindowCardWidthInset = 80.0f;
constexpr float kTrullaWindowCardMaxWidth = 920.0f;
constexpr float kTrullaWindowCardHeightInset = 80.0f;
constexpr float kTrullaWindowCardMaxHeight = 680.0f;

constexpr float kTrullaNavRailExpandedWidth = 200.0f;
constexpr float kTrullaNavRailCollapsedWidth = 64.0f;
constexpr float kTrullaNavRailCollapseBreakpoint = 700.0f;

constexpr float kTrullaCharLabelOpacity = 0.7f;
constexpr float kTrullaDimTextOpacity = 0.45f;
constexpr float kTrullaDisabledOpacity = 0.4f;
constexpr float kTrullaFaintOpacity = 0.3f;
constexpr float kTrullaFieldLabelOpacity = 0.5f;
constexpr float kTrullaHintTextOpacity = 0.4f;
constexpr float kTrullaLabelOpacity = 0.85f;
constexpr float kTrullaMutedTextOpacity = 0.55f;
constexpr float kTrullaUnselectedOptionOpacity = 0.6f;

constexpr float kTrullaDragReflowAnimMs = 200.0f;
constexpr float kTrullaQuickColorAnimMs = 100.0f;
constexpr float kTrullaToggleAnimMs = 150.0f;

constexpr float kTrullaGroupExtraHeight = 24.0f;
constexpr float kTrullaGroupPadding = 12.0f;
constexpr float kTrullaGroupRadius = 8.0f;
constexpr float kTrullaGroupSpacing = 8.0f;

constexpr float kTrullaWidgetCardContentMargin = 8.0f;
constexpr float kTrullaWidgetCardContentSpacing = 4.0f;
constexpr float kTrullaWidgetCardHeight = 34.0f;
constexpr float kTrullaWidgetCardRadius = 6.0f;
constexpr float kTrullaWidgetCardWidth = 160.0f;
constexpr int kTrullaWidgetGridColumns = 4;
constexpr float kTrullaWidgetRemoveHitSlop = -4.0f;
constexpr float kTrullaWidgetRowSpacing = 8.0f;

constexpr float kTrullaPopupContentSpacing = 10.0f;
constexpr float kTrullaPopupFieldBoxHeight = 26.0f;
constexpr float kTrullaPopupFieldGroupSpacing = 4.0f;
constexpr float kTrullaPopupOptionButtonHeight = 24.0f;
constexpr float kTrullaPopupPadding = 12.0f;
constexpr float kTrullaPopupPowerCharWidth = 24.0f;
constexpr float kTrullaPopupSaveButtonHeight = 28.0f;
constexpr float kTrullaPopupWidthNarrow = 260.0f;
constexpr float kTrullaPopupWidthWide = 360.0f;
constexpr float kTrullaTextFieldInset = 6.0f;

constexpr float kTrullaSpinnerDotInset = -1.0f;
constexpr float kTrullaSpinnerDotRadius = 5.0f;
constexpr float kTrullaSpinnerDotSize = 10.0f;
constexpr float kTrullaSpinnerRotationMs = 900.0f;

struct TrullaTabDef {
    const char *label;
    const char *icon;
};

inline constexpr TrullaTabDef kTrullaTabs[kTrullaTabCount] = {
    {"Wallpaper", icon::wallpaper},
    {"Displays", icon::device_desktop},
    {"Idle", icon::moon_stars},
    {"Starward", icon::power},
};
