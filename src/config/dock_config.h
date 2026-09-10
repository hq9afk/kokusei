#pragma once

#include <cstdint>

// capsule geometry
constexpr int kDockCapsuleHeight = 50;
constexpr float kDockPaddingH = 12.0f;
constexpr int kDockMarginBottom = 10;

// icons
constexpr int kDockIconSize = 22;
constexpr float kDockIconSpacing = 10.0f;
constexpr float kDockIconFocusedOpacity = 1.0f;
constexpr float kDockIconUnfocusedOpacity = 0.5f;

// timing
constexpr float kDockReorderMs = 200.0f;

// autohide
constexpr int kDockPeekHeight = 1;
constexpr float kDockAutoHideRevealMs = 150.0f;
constexpr float kDockAutoHideHideMs = 150.0f;

// animation owners
constexpr uint64_t kDockAnimOwnerBase = 1;
constexpr uint64_t kDockWidgetAnimOwnerBase = 300;
constexpr uint64_t kDockAutoHideAnimOwner = 299;
