#include <algorithm>

#include "config/lock_config.h"

#include "modules/lock/layout.h"

float lock_icon_box_size() {
    return kLockFontIcon + kLockIconBoxMargin * 4.0f;
}

float lock_card_height(float output_h) {
    return output_h * kLockCardHeightMult;
}

float lock_card_width(float output_h) {
    return lock_card_height(output_h) * kLockCardRatio;
}

float lock_center_scale(float output_h) {
    float s = output_h / kLockCenterRefHeight;
    return s < 1.0f ? s : 1.0f;
}

void lock_columns(float card_w, float card_h, float center_w,
                     LockRect &left, LockRect &center,
                     LockRect &right) {
    float inner_x = kLockPanelGap;
    float inner_y = kLockPanelGap;
    float inner_h = card_h - 2.0f * kLockPanelGap;
    float avail = card_w - 4.0f * kLockPanelGap - center_w;
    float side_w = std::max(0.0f, avail * 0.5f);

    left = {inner_x, inner_y, side_w, inner_h};
    center = {inner_x + side_w + kLockPanelGap, inner_y, center_w, inner_h};
    right = {center.x + center_w + kLockPanelGap, inner_y, side_w, inner_h};
}

float lock_side_card_height(float column_h) {
    return std::max(0.0f, (column_h - kLockPanelGap) * 0.5f);
}

float lock_content_height(float clock_h, float date_h, float message_h) {
    return clock_h + kLockGapClockDate + date_h + kLockGapDateAvatar +
           kLockProfileSize + kLockGapAvatarInput + kLockInputHeight +
           kLockGapInputMessage + message_h;
}

int lock_fetch_colour_count(float available_w, int max_count) {
    int n = 0;
    if (available_w >= kLockFetchColorBox)
        n = 1 +
            static_cast<int>((available_w - kLockFetchColorBox) /
                             (kLockFetchColorBox + kLockFetchColorGap));
    return std::clamp(n, 0, max_count);
}

float lock_dot_row_width(int count) {
    return static_cast<float>(std::max(count, 0)) * kLockDotSize;
}

float lock_dot_x(int index, int count, float field_width) {
    float row = lock_dot_row_width(count);
    return (field_width - row) * 0.5f +
           static_cast<float>(index) * kLockDotSize;
}

void lock_panel_origin(float output_w, float output_h, float panel_w,
                          float panel_h, float &out_x, float &out_y) {
    out_x = (output_w - panel_w) * 0.5f;
    out_y = (output_h - panel_h) * 0.5f;
}
