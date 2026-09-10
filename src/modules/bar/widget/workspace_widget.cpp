#include "modules/bar/widget/workspace_widget.h"

#include "render/palette.h"

namespace bar_detail {

namespace {

bool point_in_rect(float x, float y, const Rect &r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

float workspace_pill_width(WorkspaceWidgetState &wstate,
                           AnimationManager &animations, int ws_id,
                           bool active) {
    auto it = wstate.was_active.find(ws_id);
    if (it == wstate.was_active.end()) {
        wstate.width_t[ws_id] = active ? 1.0f : 0.0f;
        wstate.was_active[ws_id] = active;
    } else if (it->second != active) {
        it->second = active;
        animations.animate(
            wstate.width_t[ws_id], active ? 1.0f : 0.0f, kWorkspacePillAnimMs,
            Easing::Linear,
            [&wstate, ws_id](float v) { wstate.width_t[ws_id] = v; }, {},
            kWorkspacePillOwnerBase + static_cast<uint64_t>(ws_id));
    }
    float t = wstate.width_t[ws_id];
    return kWorkspacePillHeight +
           kWorkspacePillHeight * (kWorkspaceActiveWidthScale - 1.0f) * t;
}

float workspace_row_width(WorkspaceWidgetState &wstate,
                          AnimationManager &animations,
                          const std::vector<Workspace> &ws_list,
                          int active_id) {
    float w = 0;
    bool first = true;
    for (const Workspace &ws : ws_list) {
        if (!first)
            w += kWorkspacePillSpacing;
        w +=
            workspace_pill_width(wstate, animations, ws.id, ws.id == active_id);
        first = false;
    }
    return w;
}

} // namespace

float draw_workspace_row(Node *root, WorkspaceWidgetState &wstate,
                         AnimationManager &animations, float x, float height,
                         const std::vector<Workspace> &ws_list, int active_id,
                         const float pill_bg[4], const Texture &overview_icon) {
    wstate.pill_hits.clear();
    wstate.overview_hit = {};

    float ws_row_w =
        workspace_row_width(wstate, animations, ws_list, active_id);
    if (ws_row_w <= 0)
        return x;

    bool has_icon = overview_icon.id != 0;
    float icon_w = has_icon ? static_cast<float>(overview_icon.width) : 0.0f;
    float row_w = ws_row_w + kPillPad * 2 +
                  (has_icon ? kWorkspaceOverviewGap + icon_w : 0.0f);

    node_add_rrect(root, x, 0, row_w, height, metrics::radius_md,
                   metrics::border_thin, pill_bg, rgba(palette::accent));

    float wx = x + kPillPad;
    float wy = (height - kWorkspacePillHeight) / 2.0f;
    for (const Workspace &ws : ws_list) {
        bool is_active = ws.id == active_id;
        float pw = workspace_pill_width(wstate, animations, ws.id, is_active);
        const float *color = is_active     ? rgba(palette::accent_alt)
                             : ws.occupied ? rgba(palette::accent)
                                           : rgba(palette::text_dim);
        node_add_rrect(root, wx, wy, pw, kWorkspacePillHeight,
                       kWorkspacePillHeight / 2.0f, 0.0f, color, color);
        wstate.pill_hits.push_back(
            {ws.id, Rect{wx - kWorkspacePillSpacing / 2.0f, 0.0f,
                         pw + kWorkspacePillSpacing, height}});
        wx += pw + kWorkspacePillSpacing;
    }

    if (has_icon) {
        float icon_x = wx - kWorkspacePillSpacing + kWorkspaceOverviewGap;
        float icon_y = (height - static_cast<float>(overview_icon.height)) / 2.0f;
        node_add_texture(root, icon_x, icon_y, overview_icon, rgba(palette::text));
        wstate.overview_hit = {icon_x - kWorkspaceOverviewGap / 2.0f, 0.0f,
                            icon_w + kWorkspaceOverviewGap, height};
    }

    return x + row_w + kCapsuleGap;
}

int workspace_row_hit_workspace(const WorkspaceWidgetState &wstate, float x,
                                float y) {
    for (const auto &[id, r] : wstate.pill_hits)
        if (point_in_rect(x, y, r))
            return id;
    return -1;
}

bool workspace_row_hit_overview(const WorkspaceWidgetState &wstate, float x,
                             float y) {
    return wstate.overview_hit.w > 0.0f && point_in_rect(x, y, wstate.overview_hit);
}

} // namespace bar_detail
