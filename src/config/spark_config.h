#pragma once

#include <chrono>
#include <cstdint>

constexpr int kSparkSurfaceWidth = 280;
constexpr int kSparkSurfaceHeight = 50;
constexpr std::chrono::milliseconds kSparkVisibleFor{2000};
constexpr int kSparkContentMargin = 14;
constexpr int kSparkQixingMargin = 10;
constexpr int kSparkLabelWidth = 44;
constexpr float kSparkAnimNormal = 220.0f;
constexpr float kSparkAnimFast = 150.0f;
constexpr std::chrono::milliseconds kSparkReadyDelay{1000};

constexpr uint64_t kSparkOwnerOpacity = 1;
constexpr uint64_t kSparkOwnerBarFill = 2;
constexpr uint64_t kSparkOwnerIconColor = 3;
