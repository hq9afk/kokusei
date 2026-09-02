#include <cassert>
#include <cmath>

#include "config/penance_config.h"

#include "modules/penance/layout.h"

static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

void test_penance_layout() {
    assert(near(penance_icon_box_size(),
                kPenanceFontIcon + kPenanceIconBoxMargin * 4.0f));

    assert(
        near(penance_card_height(1440.0f), 1440.0f * kPenanceCardHeightMult));
    assert(near(penance_card_width(1440.0f),
                1440.0f * kPenanceCardHeightMult * kPenanceCardRatio));

    assert(near(penance_center_scale(1440.0f), 1.0f));
    assert(near(penance_center_scale(720.0f), 0.5f));
    assert(near(penance_center_scale(2160.0f), 1.0f));

    float cw = penance_card_width(1440.0f);
    float ch = penance_card_height(1440.0f);
    PenanceRect left, center, right;
    penance_columns(cw, ch, kPenanceCenterWidth, left, center, right);
    assert(near(left.x, kPenancePanelGap));
    assert(near(center.w, kPenanceCenterWidth));
    assert(near(left.w, right.w));
    assert(near(center.x, left.x + left.w + kPenancePanelGap));
    assert(near(right.x, center.x + center.w + kPenancePanelGap));
    assert(near(right.x + right.w, cw - kPenancePanelGap));
    assert(near(left.h, ch - 2.0f * kPenancePanelGap));

    float sc = penance_side_card_height(600.0f);
    assert(near(sc * 2.0f + kPenancePanelGap, 600.0f));

    float h = penance_content_height(120.0f, 34.0f, 20.0f);
    assert(near(h, 120.0f + kPenanceGapClockDate + 34.0f +
                       kPenanceGapDateAvatar + kPenanceProfileSize +
                       kPenanceGapAvatarInput + kPenanceInputHeight +
                       kPenanceGapInputMessage + 20.0f));

    assert(penance_fetch_colour_count(0.0f, 8) == 0);
    assert(penance_fetch_colour_count(kPenanceFetchColorBox, 8) == 1);
    assert(penance_fetch_colour_count(1000.0f, 8) == 8);

    assert(near(penance_dot_row_width(3), 3.0f * kPenanceDotSize));
    float w = 400.0f;
    float x0 = penance_dot_x(0, 4, w);
    float x1 = penance_dot_x(1, 4, w);
    assert(near(x1 - x0, kPenanceDotSize));
    assert(near(x0, (w - 4.0f * kPenanceDotSize) * 0.5f));
    assert(near(penance_dot_row_width(0), 0.0f));

    float px = 0, py = 0;
    penance_panel_origin(1920.0f, 1080.0f, 540.0f, 400.0f, px, py);
    assert(near(px, (1920.0f - 540.0f) * 0.5f));
    assert(near(py, (1080.0f - 400.0f) * 0.5f));
}
