#include "modules/settings/rain_tab.h"

using panel_chrome_detail::cached_text;

void rain_tab_paint(SettingsState &state, Node *root, int32_t scale, float x,
                    float y, float w, const Config &cfg) {
    const RainParams &p = cfg.rain;

    static const char *kModeLabels[2] = {"Matrix", "Stiletto"};
    static const char *kModeTags[2] = {"rainmodematrix", "rainmodestiletto"};
    bool active_flags[2] = {p.mode == RainMode::Matrix,
                            p.mode == RainMode::Stiletto};

    float tile_w = (w - kSettingsScreenSelectorSpacing) / 2.0f;
    float cx = x;
    for (int i = 0; i < 2; ++i) {
        bool active = active_flags[i];
        node_add_rrect(root, cx, y, tile_w, kSettingsScreenSelectorHeight,
                       kSettingsTileRadius, kSettingsSelectorBorderWidth,
                       rgba(palette::lavender_alpha20),
                       active ? rgba(palette::accent_alt) : kPanelNoBorder);
        const Texture *tex = cached_text(state.tcache, kModeLabels[i], scale);
        if (tex)
            node_add_texture(root, cx + (tile_w - tex->width) / 2.0f,
                             y + (kSettingsScreenSelectorHeight - tex->height) /
                                     2.0f,
                             *tex, rgba(palette::text));
        state.click_regions.push_back(
            {PanelClickKind::ToggleFlip,
             {cx, y, tile_w, kSettingsScreenSelectorHeight},
             kModeTags[i]});
        cx += tile_w + kSettingsScreenSelectorSpacing;
    }

    y += kSettingsScreenSelectorHeight + kPanelRowGap;
    draw_toggle_row(state, root, scale, x, y, w, "Asynchronous fall speed",
                    p.async_speed, "rainasyncspeed", true);
}

bool rain_tab_handle_click(SettingsState &state, const Config &cfg,
                           const SettingsCommitFn &on_commit,
                           const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;

    if (region.tag == "rainasyncspeed") {
        settings_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        updated.rain.async_speed = !cfg.rain.async_speed;
        on_commit(updated);
        settings_request_frame(state);
        return true;
    }

    if (region.tag != "rainmodematrix" && region.tag != "rainmodestiletto")
        return false;

    RainMode mode = region.tag == "rainmodestiletto" ? RainMode::Stiletto
                                                     : RainMode::Matrix;
    if (cfg.rain.mode != mode) {
        settings_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        updated.rain.mode = mode;
        on_commit(updated);
        settings_request_frame(state);
    }
    return true;
}
