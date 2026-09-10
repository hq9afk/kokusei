#include "modules/settings/logout_tab.h"

void logout_tab_paint(SettingsState &state, Node *root, int32_t scale, float x,
                        float y, float w, const Config &cfg) {
    draw_toggle_row(state, root, scale, x, y, w, "Animated central logo",
                    cfg.logout_animated_logo, "logoutanimatedlogo", true);
}

bool logout_tab_handle_click(SettingsState &state, const Config &cfg,
                               const SettingsCommitFn &on_commit,
                               const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip ||
        region.tag != "logoutanimatedlogo")
        return false;

    Config updated = cfg;
    updated.logout_animated_logo = !cfg.logout_animated_logo;
    on_commit(updated);
    settings_request_frame(state);
    return true;
}
