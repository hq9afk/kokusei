#include <algorithm>

#include "config/penance_config.h"

#include "modules/penance/layout.h"

float penance_icon_box_size() {
    return kPenanceFontIcon + kPenanceIconBoxMargin * 4.0f;
}

float penance_card_height(float output_h) {
    return output_h * kPenanceCardHeightMult;
}

float penance_card_width(float output_h) {
    return penance_card_height(output_h) * kPenanceCardRatio;
}

float penance_center_scale(float output_h) {
    float s = output_h / kPenanceCenterRefHeight;
    return s < 1.0f ? s : 1.0f;
}

void penance_columns(float card_w, float card_h, float center_w,
                     PenanceRect &left, PenanceRect &center,
                     PenanceRect &right) {
    float inner_x = kPenancePanelGap;
    float inner_y = kPenancePanelGap;
    float inner_h = card_h - 2.0f * kPenancePanelGap;
    float avail = card_w - 4.0f * kPenancePanelGap - center_w;
    float side_w = std::max(0.0f, avail * 0.5f);

    left = {inner_x, inner_y, side_w, inner_h};
    center = {inner_x + side_w + kPenancePanelGap, inner_y, center_w, inner_h};
    right = {center.x + center_w + kPenancePanelGap, inner_y, side_w, inner_h};
}

float penance_side_card_height(float column_h) {
    return std::max(0.0f, (column_h - kPenancePanelGap) * 0.5f);
}

float penance_content_height(float clock_h, float date_h, float message_h) {
    return clock_h + kPenanceGapClockDate + date_h + kPenanceGapDateAvatar +
           kPenanceProfileSize + kPenanceGapAvatarInput + kPenanceInputHeight +
           kPenanceGapInputMessage + message_h;
}

int penance_fetch_colour_count(float available_w, int max_count) {
    int n = 0;
    if (available_w >= kPenanceFetchColorBox)
        n = 1 +
            static_cast<int>((available_w - kPenanceFetchColorBox) /
                             (kPenanceFetchColorBox + kPenanceFetchColorGap));
    return std::clamp(n, 0, max_count);
}

float penance_dot_row_width(int count) {
    return static_cast<float>(std::max(count, 0)) * kPenanceDotSize;
}

float penance_dot_x(int index, int count, float field_width) {
    float row = penance_dot_row_width(count);
    return (field_width - row) * 0.5f +
           static_cast<float>(index) * kPenanceDotSize;
}

void penance_panel_origin(float output_w, float output_h, float panel_w,
                          float panel_h, float &out_x, float &out_y) {
    out_x = (output_w - panel_w) * 0.5f;
    out_y = (output_h - panel_h) * 0.5f;
}
