#pragma once

#include <cstdint>

// surface size
constexpr int kNotificationSurfaceWidth = 400;
constexpr int kNotificationSurfaceHeight = 480;

// card geometry
constexpr float kNotificationCardPadding = 16.0f;
constexpr float kNotificationCardGap = 8.0f;
constexpr float kNotificationCardRadius = 10.0f;
constexpr float kNotificationCardBorderWidth = 2.0f;
constexpr float kNotificationContentSpacing = 10.0f;
constexpr float kNotificationHeaderSpacing = 6.0f;
constexpr float kNotificationUrgencyDotSize = 6.0f;
constexpr float kNotificationUrgencyDotRadius = 3.0f;
constexpr float kNotificationProgressHeight = 4.0f;
constexpr float kNotificationProgressTrackOpacity = 0.3f;

// animation timing
constexpr float kNotificationAnimNormal = 220.0f;
constexpr float kNotificationAnimExitBuffer = 60.0f;
constexpr float kNotificationSlideOffset = 24.0f;

// close button
constexpr float kNotificationCloseHitAreaSize = 40.0f;
constexpr float kNotificationCloseIconOpacityIdle = 0.4f;
constexpr int kNotificationCloseIconPx = 16;
constexpr float kNotificationCardExtraHeight = 8.0f;
constexpr float kNotificationCardContentTrailingGap = 8.0f;

// content scale
constexpr int32_t kContentScale = 1;
