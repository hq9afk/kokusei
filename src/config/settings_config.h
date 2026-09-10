#pragma once

#include <cstdint>

enum class SettingsFieldId {
    None,
    WallpaperPath,
    WallpaperDir,
    WallpaperAnimatedDir,
    AmbientTimeout,
    ScreensaverTimeout,
    VisualizerFps,
    VisualizerParticleThin,
    VisualizerParticleSize,
    VisualizerComplexity,
    VisualizerGlowDirections,
    VisualizerGlowQuality,
};

// tab count & labels
constexpr int kSettingsTabCount = 6;

inline constexpr const char *kSettingsDisplaysDefaultTag = "__default__";

// nav rail
constexpr float kSettingsRailItemHeight = 36.0f;
constexpr float kSettingsRailItemGap = 4.0f;
constexpr float kSettingsRailIconLabelGap = 10.0f;
constexpr float kSettingsRailPadding = 10.0f;
constexpr float kSettingsRailDividerGap = 16.0f;

// profile block
constexpr float kSettingsProfileAvatarSize = 40.0f;
constexpr float kSettingsProfileAvatarBorderWidth = 2.0f;
constexpr float kSettingsProfileTopPadding = 4.0f;
constexpr float kSettingsProfileAvatarLabelGap = 10.0f;
constexpr float kSettingsProfileLineGap = 2.0f;
constexpr float kSettingsProfileBottomPadding = 12.0f;
constexpr float kSettingsProfileDividerGap = 12.0f;

// row & field layout
constexpr float kSettingsRowHeight = 40.0f;
constexpr float kSettingsRowGap = 10.0f;
constexpr float kSettingsLabelWidth = 170.0f;
constexpr float kSettingsFieldHeight = 28.0f;
constexpr float kSettingsFieldWidth = 240.0f;
constexpr float kSettingsNumberFieldWidth = 72.0f;

// toggle animation owners
constexpr uint64_t kSettingsAutohideToggleOwner = 3;
constexpr uint64_t kSettingsFillModeToggleOwner = 4;
constexpr uint64_t kSettingsFieldTypeAnimOwnerBase = 10000;

// wallpaper thumbnail grid
constexpr float kSettingsWallpaperThumbSize = 115.0f;
constexpr float kSettingsWallpaperThumbGap = 15.0f;
constexpr int kSettingsWallpaperGridColumns = 5;
constexpr float kSettingsWallpaperThumbRadius = 8.0f;
constexpr float kSettingsWallpaperLabelPad = 6.0f;
constexpr float kSettingsWallpaperGridInset = 5.0f;

// wallpaper scroll
constexpr float kSettingsWallpaperScrollSpeed = 3.0f;

// wallpaper warnings & monitor chips
constexpr float kSettingsWallpaperWarningPad = 10.0f;
constexpr float kSettingsMonitorChipHeight = 35.0f;
constexpr float kSettingsMonitorChipGap = 6.0f;

// column stepper
constexpr float kSettingsColumnStepperButtonSize = 28.0f;

// dir bar
constexpr float kSettingsDirBarHeight = 40.0f;
constexpr float kSettingsDirBarLabelMargin = 14.0f;
constexpr float kSettingsDirBarFieldMargin = 10.0f;
constexpr float kSettingsDirBarEdgeMargin = 8.0f;
constexpr float kSettingsDirBarButtonWidth = 72.0f;
constexpr float kSettingsDirBarButtonHeight = 28.0f;

// screen selector
constexpr float kSettingsScreenSelectorHeight = 35.0f;
constexpr float kSettingsScreenSelectorSpacing = 6.0f;
constexpr float kSettingsSelectorBorderWidth = 2.0f;
constexpr float kSettingsTileRadius = 6.0f;

// toggle tile
constexpr float kSettingsToggleTileHeight = 48.0f;
constexpr float kSettingsToggleTileBorderWidth = 2.0f;
constexpr float kSettingsToggleTileContentMargin = 12.0f;
constexpr float kSettingsToggleTileContentSpacing = 10.0f;
constexpr float kSettingsGroupSpacingSm = 8.0f;

// toggle switch
constexpr float kSettingsToggleTrackWidth = 36.0f;
constexpr float kSettingsToggleTrackHeight = 20.0f;
constexpr float kSettingsToggleKnobSize = 14.0f;
constexpr float kSettingsToggleKnobInset = 3.0f;

// idle reset
constexpr float kSettingsIdleResetIconSize = 20.0f;
constexpr int kSettingsIdleTimeoutMin = 10;
constexpr int kSettingsIdleTimeoutMax = 1800;

// window card
constexpr float kSettingsWindowCardWidthInset = 80.0f;
constexpr float kSettingsWindowCardMaxWidth = 920.0f;
constexpr float kSettingsWindowCardHeightInset = 80.0f;
constexpr float kSettingsWindowCardMaxHeight = 680.0f;

// nav rail responsive breakpoints
constexpr float kSettingsNavRailExpandedWidth = 200.0f;
constexpr float kSettingsNavRailCollapsedWidth = 64.0f;
constexpr float kSettingsNavRailCollapseBreakpoint = 700.0f;

// text opacity levels
constexpr float kSettingsCharLabelOpacity = 0.7f;
constexpr float kSettingsDimTextOpacity = 0.45f;
constexpr float kSettingsDisabledOpacity = 0.4f;
constexpr float kSettingsFaintOpacity = 0.3f;
constexpr float kSettingsFieldLabelOpacity = 0.5f;
constexpr float kSettingsHintTextOpacity = 0.4f;
constexpr float kSettingsLabelOpacity = 0.85f;
constexpr float kSettingsMutedTextOpacity = 0.55f;
constexpr float kSettingsUnselectedOptionOpacity = 0.6f;

// misc animation timing
constexpr float kSettingsDragReflowAnimMs = 200.0f;
constexpr float kSettingsQuickColorAnimMs = 100.0f;
constexpr float kSettingsToggleAnimMs = 150.0f;

// group layout
constexpr float kSettingsGroupExtraHeight = 24.0f;
constexpr float kSettingsGroupPadding = 12.0f;
constexpr float kSettingsGroupRadius = 8.0f;
constexpr float kSettingsGroupSpacing = 8.0f;

// widget card
constexpr float kSettingsWidgetCardContentMargin = 8.0f;
constexpr float kSettingsWidgetCardContentSpacing = 4.0f;
constexpr float kSettingsWidgetCardHeight = 34.0f;
constexpr float kSettingsWidgetCardRadius = 6.0f;
constexpr float kSettingsWidgetCardWidth = 160.0f;
constexpr int kSettingsWidgetGridColumns = 4;
constexpr float kSettingsWidgetRemoveHitSlop = -4.0f;
constexpr float kSettingsWidgetRowSpacing = 8.0f;

// popup
constexpr float kSettingsPopupContentSpacing = 10.0f;
constexpr float kSettingsPopupFieldBoxHeight = 26.0f;
constexpr float kSettingsPopupFieldGroupSpacing = 4.0f;
constexpr float kSettingsPopupOptionButtonHeight = 24.0f;
constexpr float kSettingsPopupPadding = 12.0f;
constexpr float kSettingsPopupPowerCharWidth = 24.0f;
constexpr float kSettingsPopupSaveButtonHeight = 28.0f;
constexpr float kSettingsPopupWidthNarrow = 260.0f;
constexpr float kSettingsPopupWidthWide = 360.0f;
constexpr float kSettingsTextFieldInset = 6.0f;

// spinner
constexpr float kSettingsSpinnerDotInset = -1.0f;
constexpr float kSettingsSpinnerDotRadius = 5.0f;
constexpr float kSettingsSpinnerDotSize = 10.0f;
constexpr float kSettingsSpinnerRotationMs = 900.0f;

struct SettingsTabDef {
    const char *label;
    const char *icon;
};

inline constexpr const char *kSettingsTabLabels[kSettingsTabCount] = {
    "Wallpaper", "Displays", "Idle", "Logout", "Visualizer", "Rain",
};
