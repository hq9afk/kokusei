#include <cassert>
#include <cmath>

#include "config/lock_config.h"

#include "modules/lock/layout.h"

static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

void test_lock_layout() {
    assert(near(lock_icon_box_size(),
                kLockFontIcon + kLockIconBoxMargin * 4.0f));

    assert(
        near(lock_card_height(1440.0f), 1440.0f * kLockCardHeightMult));
    assert(near(lock_card_width(1440.0f),
                1440.0f * kLockCardHeightMult * kLockCardRatio));

    assert(near(lock_center_scale(1440.0f), 1.0f));
    assert(near(lock_center_scale(720.0f), 0.5f));
    assert(near(lock_center_scale(2160.0f), 1.0f));

    float cw = lock_card_width(1440.0f);
    float ch = lock_card_height(1440.0f);
    LockRect left, center, right;
    lock_columns(cw, ch, kLockCenterWidth, left, center, right);
    assert(near(left.x, kLockPanelGap));
    assert(near(center.w, kLockCenterWidth));
    assert(near(left.w, right.w));
    assert(near(center.x, left.x + left.w + kLockPanelGap));
    assert(near(right.x, center.x + center.w + kLockPanelGap));
    assert(near(right.x + right.w, cw - kLockPanelGap));
    assert(near(left.h, ch - 2.0f * kLockPanelGap));

    float sc = lock_side_card_height(600.0f);
    assert(near(sc * 2.0f + kLockPanelGap, 600.0f));

    float h = lock_content_height(120.0f, 34.0f, 20.0f);
    assert(near(h, 120.0f + kLockGapClockDate + 34.0f +
                       kLockGapDateAvatar + kLockProfileSize +
                       kLockGapAvatarInput + kLockInputHeight +
                       kLockGapInputMessage + 20.0f));

    assert(lock_fetch_colour_count(0.0f, 8) == 0);
    assert(lock_fetch_colour_count(kLockFetchColorBox, 8) == 1);
    assert(lock_fetch_colour_count(1000.0f, 8) == 8);

    assert(near(lock_dot_row_width(3), 3.0f * kLockDotSize));
    float w = 400.0f;
    float x0 = lock_dot_x(0, 4, w);
    float x1 = lock_dot_x(1, 4, w);
    assert(near(x1 - x0, kLockDotSize));
    assert(near(x0, (w - 4.0f * kLockDotSize) * 0.5f));
    assert(near(lock_dot_row_width(0), 0.0f));

    float px = 0, py = 0;
    lock_panel_origin(1920.0f, 1080.0f, 540.0f, 400.0f, px, py);
    assert(near(px, (1920.0f - 540.0f) * 0.5f));
    assert(near(py, (1080.0f - 400.0f) * 0.5f));
}
