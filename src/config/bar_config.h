#pragma once

#include <chrono>
#include <cstdint>

namespace bar_detail {
// bar height
constexpr int32_t kBarHeight = 35;

// pill layout & timing
constexpr float kPillPad = 10.0f;
constexpr float kCapsuleGap = 10.0f;
constexpr float kWorkspaceOverviewGap = 8.0f;
constexpr float kPillExpandMs = 150.0f;
constexpr auto kPillCloseLingerMs = std::chrono::milliseconds(80);

// top margin
constexpr int32_t kBarTopMargin = 10;

// autohide
constexpr int32_t kAutoHideStripPx = 1;
constexpr float kAutoHideRevealMs = 150.0f;
constexpr float kAutoHideHideMs = 150.0f;
constexpr uint64_t kAutoHideAnimOwner = 1000;
} // namespace bar_detail
