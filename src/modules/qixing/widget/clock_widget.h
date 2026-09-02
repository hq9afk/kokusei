#pragma once

#include <cstdint>

#include "modules/qixing/widget/widget_capsule.h"

#include "render/rect.h"
#include "render/texture.h"

struct MonitorOutput;

namespace qixing_detail {

void update_clock(Texture &clock_texture);

Rect draw_clock_pill(Node *root, float height, int32_t surface_width,
                     const Texture &clock_texture, const float tint[4],
                     const float pill_bg[4]);

void clock_pill_clicked(MonitorOutput &mon);

} // namespace qixing_detail
