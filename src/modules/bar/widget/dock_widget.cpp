#include "modules/bar/widget/dock_widget.h"

#include "config/bar_config.h"
#include "config/dock_config.h"

#include "render/palette.h"

namespace bar_detail {

float draw_dock_capsule(Node *root, DockWidgetState &st,
                        AnimationManager &animations, float x, float height,
                        const std::vector<DockEntry> &entries,
                        const float pill_bg[4]) {
    if (entries.empty())
        return x;

    float row_w = dock_row_width(entries);
    float capsule_w = row_w + kPillPad * 2.0f;
    node_add_rrect(root, x, 0.0f, capsule_w, height, metrics::radius_md,
                   metrics::border_thin, pill_bg, rgba(palette::accent));
    draw_dock_row(root, st.icons, st.row, animations, x + kPillPad,
                  height / 2.0f, entries, kDockWidgetAnimOwnerBase);
    return x + capsule_w + kCapsuleGap;
}

} // namespace bar_detail
