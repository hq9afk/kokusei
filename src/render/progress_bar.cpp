#include <algorithm>

#include "render/progress_bar.h"

void draw_flat_bar(Node *parent, float x, float y, float w, float h,
                   float radius, float fraction01, float min_fill_w,
                   const float *track_color, const float *fill_color) {
    node_add_rrect(parent, x, y, w, h, radius, 0.0f, track_color,
                   kNodeTransparent);
    float fill_w = std::max(min_fill_w, w * std::clamp(fraction01, 0.0f, 1.0f));
    node_add_rrect(parent, x, y, fill_w, h, radius, 0.0f, fill_color,
                   kNodeTransparent);
}
