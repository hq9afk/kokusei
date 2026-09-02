#pragma once

#include <chrono>
#include <cstdint>

namespace qixing_detail {
constexpr int32_t kQixingHeight = 35;

constexpr float kPillPad = 10.0f;
constexpr float kCapsuleGap = 10.0f;
constexpr float kWorkspaceLiyueGap = 8.0f;
constexpr float kPillExpandMs = 150.0f;
constexpr auto kPillCloseLingerMs = std::chrono::milliseconds(80);

constexpr int32_t kQixingTopMargin = 10;

constexpr int32_t kAutoHideStripPx = 1;
constexpr float kAutoHideRevealMs = 150.0f;
constexpr float kAutoHideHideMs = 150.0f;
constexpr uint64_t kAutoHideAnimOwner = 1000;
} // namespace qixing_detail
