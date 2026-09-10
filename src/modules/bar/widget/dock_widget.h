#pragma once

#include <vector>

#include "render/animation.h"
#include "render/dock_row.h"
#include "render/node.h"

#include "service/dock_service.h"

struct DockWidgetState {
    DockIconCache icons;
    DockRowState row;
};

namespace bar_detail {

float draw_dock_capsule(Node *root, DockWidgetState &st,
                        AnimationManager &animations, float x, float height,
                        const std::vector<DockEntry> &entries,
                        const float pill_bg[4]);

} // namespace bar_detail
