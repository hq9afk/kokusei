#include "modules/trulla/starward_tab.h"

void starward_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                        float y, float w, const Config &cfg) {
    draw_toggle_row(state, root, scale, x, y, w, "Animated central logo",
                    cfg.starward_animated_logo, "starwardanimatedlogo", true);
}

bool starward_tab_handle_click(TrullaState &state, const Config &cfg,
                               const TrullaCommitFn &on_commit,
                               const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip ||
        region.tag != "starwardanimatedlogo")
        return false;

    Config updated = cfg;
    updated.starward_animated_logo = !cfg.starward_animated_logo;
    on_commit(updated);
    trulla_request_frame(state);
    return true;
}
