#pragma once

#include <cstdint>

// animation timing
constexpr int kLiyueAnimEnterMs = 400;
constexpr int kLiyueAnimFastMs = 200;

// background border
constexpr float kLiyueBackgroundBorderWidth = 2.0f;

// background & grid layout
constexpr float kLiyueBackgroundOpacity = 1.0f;
constexpr float kLiyueBackgroundPadding = 10.0f;
constexpr int kLiyueColumns = 5;
constexpr float kLiyueElevationMargin = 10.0f;

// focus & indicator
constexpr int kLiyueFocusGrabDelayMs = 150;
constexpr float kLiyueFocusedIndicatorBorderWidth = 2.0f;

// window/preview layout
constexpr float kLiyueIconToWindowRatio = 0.25f;
constexpr float kLiyueOtherMonitorOpacity = 0.4f;
constexpr int kLiyueRaceDelayMs = 150;
constexpr int kLiyueRows = 2;
constexpr float kLiyueScale = 0.15f;
constexpr float kLiyueScreenRounding = 23.0f;

// shadow
constexpr float kLiyueShadowBlurFactor = 0.9f;
constexpr float kLiyueShadowOffsetX = 0.0f;
constexpr float kLiyueShadowOffsetY = 1.0f;
constexpr float kLiyueShadowRadius = 20.0f;
constexpr float kLiyueShadowSpread = 1.0f;

// window & preview
constexpr float kLiyueWindowDraggingZ = 99999.0f;
constexpr float kLiyueWindowPreviewBorderWidth = 2.0f;
constexpr float kLiyueWindowRounding = 18.0f;

// workspace label
constexpr float kLiyueWorkspaceBorderWidth = 2.0f;
constexpr float kLiyueWorkspaceNumberBaseSize = 250.0f;
constexpr float kLiyueWorkspaceNumberTextFade = 0.8f;
constexpr float kLiyueWorkspaceSpacing = 5.0f;

// capture interval
constexpr int kLiyueCaptureIntervalMs = 33;

// animation owners
constexpr uint64_t kLiyueIndicatorXOwner = 3;
constexpr uint64_t kLiyueIndicatorYOwner = 4;
constexpr uint64_t kLiyueSlideOwner = 5;
