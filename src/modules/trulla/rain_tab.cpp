#include "modules/trulla/rain_tab.h"

using panel_chrome_detail::cached_text;

void rain_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                    float y, float w, const Config &cfg) {
    const RainParams &p = cfg.rain;

    static const char *kModeLabels[2] = {"Matrix", "Stiletto"};
    static const char *kModeTags[2] = {"rainmodematrix", "rainmodestiletto"};
    bool active_flags[2] = {p.mode == RainMode::Matrix,
                            p.mode == RainMode::Stiletto};

    float tile_w = (w - kTrullaScreenSelectorSpacing) / 2.0f;
    float cx = x;
    for (int i = 0; i < 2; ++i) {
        bool active = active_flags[i];
        node_add_rrect(root, cx, y, tile_w, kTrullaScreenSelectorHeight,
                       kTrullaTileRadius, kTrullaSelectorBorderWidth,
                       rgba(palette::lavender_alpha20),
                       active ? rgba(palette::accent_alt) : kPanelNoBorder);
        const Texture *tex = cached_text(state.tcache, kModeLabels[i], scale);
        if (tex)
            node_add_texture(root, cx + (tile_w - tex->width) / 2.0f,
                             y + (kTrullaScreenSelectorHeight - tex->height) /
                                     2.0f,
                             *tex, rgba(palette::text));
        state.click_regions.push_back(
            {PanelClickKind::ToggleFlip,
             {cx, y, tile_w, kTrullaScreenSelectorHeight},
             kModeTags[i]});
        cx += tile_w + kTrullaScreenSelectorSpacing;
    }

    y += kTrullaScreenSelectorHeight + kPanelRowGap;
    draw_toggle_row(state, root, scale, x, y, w, "Asynchronous fall speed",
                    p.async_speed, "rainasyncspeed", true);
}

bool rain_tab_handle_click(TrullaState &state, const Config &cfg,
                           const TrullaCommitFn &on_commit,
                           const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;

    if (region.tag == "rainasyncspeed") {
        trulla_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        updated.rain.async_speed = !cfg.rain.async_speed;
        on_commit(updated);
        trulla_request_frame(state);
        return true;
    }

    if (region.tag != "rainmodematrix" && region.tag != "rainmodestiletto")
        return false;

    RainMode mode = region.tag == "rainmodestiletto" ? RainMode::Stiletto
                                                     : RainMode::Matrix;
    if (cfg.rain.mode != mode) {
        trulla_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        updated.rain.mode = mode;
        on_commit(updated);
        trulla_request_frame(state);
    }
    return true;
}
