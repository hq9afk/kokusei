#pragma once

#include <cstdint>

// animation timing
constexpr int kOverviewAnimEnterMs = 400;
constexpr int kOverviewAnimFastMs = 200;

// background border
constexpr float kOverviewBackgroundBorderWidth = 2.0f;

// background & grid layout
constexpr float kOverviewBackgroundOpacity = 1.0f;
constexpr float kOverviewBackgroundPadding = 10.0f;
constexpr int kOverviewColumns = 5;
constexpr float kOverviewElevationMargin = 10.0f;

// focus & indicator
constexpr int kOverviewFocusGrabDelayMs = 150;
constexpr float kOverviewFocusedIndicatorBorderWidth = 2.0f;

// window/preview layout
constexpr float kOverviewIconToWindowRatio = 0.25f;
constexpr float kOverviewOtherMonitorOpacity = 0.4f;
constexpr int kOverviewRaceDelayMs = 150;
constexpr int kOverviewRows = 2;
constexpr float kOverviewScale = 0.15f;
constexpr float kOverviewScreenRounding = 23.0f;

// shadow
constexpr float kOverviewShadowBlurFactor = 0.9f;
constexpr float kOverviewShadowOffsetX = 0.0f;
constexpr float kOverviewShadowOffsetY = 1.0f;
constexpr float kOverviewShadowRadius = 20.0f;
constexpr float kOverviewShadowSpread = 1.0f;

// window & preview
constexpr float kOverviewWindowDraggingZ = 99999.0f;
constexpr float kOverviewWindowPreviewBorderWidth = 2.0f;
constexpr float kOverviewWindowRounding = 18.0f;

// workspace label
constexpr float kOverviewWorkspaceBorderWidth = 2.0f;
constexpr float kOverviewWorkspaceNumberBaseSize = 250.0f;
constexpr float kOverviewWorkspaceNumberTextFade = 0.8f;
constexpr float kOverviewWorkspaceSpacing = 5.0f;

// capture interval
constexpr int kOverviewCaptureIntervalMs = 33;

// animation owners
constexpr uint64_t kOverviewIndicatorXOwner = 3;
constexpr uint64_t kOverviewIndicatorYOwner = 4;
constexpr uint64_t kOverviewSlideOwner = 5;
