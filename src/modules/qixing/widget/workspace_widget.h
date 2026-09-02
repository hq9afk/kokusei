#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "modules/qixing/widget/widget_capsule.h"

#include "render/animation.h"
#include "render/rect.h"
#include "render/texture.h"

#include "service/hyprland_service.h"

struct WorkspaceWidgetState {
    std::unordered_map<int, float> width_t;
    std::unordered_map<int, bool> was_active;
    std::vector<std::pair<int, Rect>> pill_hits;
    Rect liyue_hit{};
};

namespace qixing_detail {
constexpr float kWorkspacePillHeight = 12.0f;
constexpr float kWorkspacePillSpacing = 5.0f;
constexpr float kWorkspaceActiveWidthScale = 2.0f;
constexpr float kWorkspacePillAnimMs = 100.0f;
constexpr uint64_t kWorkspacePillOwnerBase = 200;
float draw_workspace_row(Node *root, WorkspaceWidgetState &wstate,
                         AnimationManager &animations, float x, float height,
                         const std::vector<Workspace> &ws_list, int active_id,
                         const float pill_bg[4], const Texture &liyue_icon);

int workspace_row_hit_workspace(const WorkspaceWidgetState &wstate, float x,
                                float y);

bool workspace_row_hit_liyue(const WorkspaceWidgetState &wstate, float x,
                             float y);

} // namespace qixing_detail
