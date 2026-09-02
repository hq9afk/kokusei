#include "modules/trulla/blink_tab.h"
#include "modules/trulla/displays_tab.h"

#include "render/icons.h"

using panel_chrome_detail::cached_icon;
using panel_chrome_detail::cached_text;

namespace {

std::string blink_monitor_from_tag(const std::string &tag) {
    return tag == kTrullaDisplaysDefaultTag ? "" : tag;
}

void draw_blink_tier_tile(TrullaState &state, Node *parent, int32_t scale,
                          float x, float y, float w, const std::string &label,
                          TrullaFieldId field_id, uint32_t value,
                          uint32_t default_value, bool enabled_value,
                          bool show_reset, const char *reset_tag,
                          const char *toggle_tag) {
    float h = kTrullaToggleTileHeight;
    node_add_rrect(parent, x, y, w, h, kTrullaTileRadius,
                   kTrullaToggleTileBorderWidth, rgba(palette::text_alpha04),
                   rgba(palette::text_alpha07));
    float inset = kTrullaToggleTileContentMargin;

    const Texture *label_tex = cached_text(state.tcache, label, scale);
    if (label_tex)
        node_add_texture(parent, x + inset, y + (h - label_tex->height) / 2.0f,
                         *label_tex, rgba(palette::text_alpha85));

    float switch_x = x + w - inset - kTrullaToggleTrackWidth;
    float divider_x = switch_x - kTrullaToggleTileContentSpacing;
    float reset_x =
        divider_x - kTrullaToggleTileContentSpacing - kTrullaIdleResetIconSize;
    float field_w = kTrullaNumberFieldWidth;
    float field_x = reset_x - kTrullaToggleTileContentSpacing - field_w;
    float field_y = y + (h - kTrullaFieldHeight) / 2.0f;

    bool focused = state.focused_field == field_id;
    node_add_rrect(parent, field_x, field_y, field_w, kTrullaFieldHeight,
                   metrics::radius_sm, metrics::border_thin,
                   rgba(palette::field_bg),
                   focused ? rgba(palette::accent) : kPanelNoBorder);
    float field_center_y = field_y + kTrullaFieldHeight / 2.0f;
    if (focused) {
        float advance = draw_text_field_value(
            parent, state.tcache, scale, state.field_buffer.text, field_x + 8,
            field_center_y, rgba(palette::text), &state.field_anim);
        float cursor_x = field_x + 8 + advance + 2;
        draw_text_field_preedit(parent, state.tcache, scale,
                                state.field_buffer.preedit, cursor_x,
                                field_center_y, rgba(palette::text));
        Rect caret = {cursor_x, field_y + 5, 1.5f, kTrullaFieldHeight - 10};
        state.field_buffer.cursor_rect = caret;
        draw_text_field_caret(parent, state.field_buffer, caret,
                              rgba(palette::text), true);
    } else {
        const Texture *value_tex =
            cached_text(state.tcache, std::to_string(value), scale);
        if (value_tex)
            node_add_texture(
                parent, field_x + 8,
                field_y + (kTrullaFieldHeight - value_tex->height) / 2.0f,
                *value_tex, rgba(palette::text));
    }
    state.click_regions.push_back(
        {PanelClickKind::FieldFocus,
         {field_x, field_y, field_w, kTrullaFieldHeight},
         std::to_string(static_cast<int>(field_id))});

    if (show_reset && value != default_value) {
        float reset_y = y + (h - kTrullaIdleResetIconSize) / 2.0f;
        const Texture *reset_icon =
            cached_icon(state.tcache, icon::refresh, scale);
        if (reset_icon)
            node_add_texture(
                parent,
                reset_x + (kTrullaIdleResetIconSize - reset_icon->width) / 2.0f,
                reset_y +
                    (kTrullaIdleResetIconSize - reset_icon->height) / 2.0f,
                *reset_icon, rgba(palette::text_dim));
        state.click_regions.push_back(
            {PanelClickKind::ToggleFlip,
             {reset_x, reset_y, kTrullaIdleResetIconSize,
              kTrullaIdleResetIconSize},
             reset_tag});
    }

    node_add_rect(parent, divider_x, y + 10.0f, 1.0f, h - 20.0f,
                  rgba(palette::text_alpha11));

    float switch_y = y + (h - kTrullaToggleTrackHeight) / 2.0f;
    draw_toggle_switch(state, parent, switch_x, switch_y, enabled_value,
                       toggle_tag);
}

} // namespace

void blink_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                     float y, float w, const Config &cfg) {
    draw_toggle_row(state, root, scale, x, y, w, "Enable Idle Management",
                    cfg.blink_management_enabled, "idlemanagementenabled",
                    true);
    y += kTrullaToggleTileHeight + kPanelRowGap;

    if (!cfg.blink_management_enabled)
        return;

    trulla_draw_monitor_row(state, root, scale, x, y, w,
                            state.blink_selected_monitor);
    y += kTrullaScreenSelectorHeight + kPanelRowGap;

    bool is_default = state.blink_selected_monitor.empty();
    const MonitorOverride *ov = nullptr;
    if (!is_default) {
        auto it = cfg.monitor_overrides.find(state.blink_selected_monitor);
        if (it != cfg.monitor_overrides.end())
            ov = &it->second;
    }
    bool override_enabled = ov && ov->enabled;

    if (!is_default) {
        draw_toggle_row(state, root, scale, x, y, w,
                        "Override default settings", override_enabled,
                        "idleoverride", false);
        y += kTrullaToggleTrackHeight + kPanelRowGap;
    }

    if (is_default || override_enabled) {
        bool ambient_enabled_val =
            is_default ? cfg.ambient_enabled : ov->ambient_enabled;
        uint32_t ambient_timeout_val = is_default ? cfg.ambient_timeout_seconds
                                                  : ov->ambient_timeout_seconds;
        bool screensaver_enabled_val =
            is_default ? cfg.screensaver_enabled : ov->screensaver_enabled;
        uint32_t screensaver_timeout_val =
            is_default ? cfg.screensaver_timeout_seconds
                       : ov->screensaver_timeout_seconds;

        draw_blink_tier_tile(state, root, scale, x, y, w, "Ambient Mode",
                             TrullaFieldId::AmbientTimeout, ambient_timeout_val,
                             cfg.ambient_timeout_seconds, ambient_enabled_val,
                             !is_default, "idleambientreset",
                             "idleambientenabled");
        y += kTrullaToggleTileHeight + kTrullaGroupSpacingSm;

        draw_blink_tier_tile(
            state, root, scale, x, y, w, "Screensaver",
            TrullaFieldId::ScreensaverTimeout, screensaver_timeout_val,
            cfg.screensaver_timeout_seconds, screensaver_enabled_val,
            !is_default, "idlescreensaverreset", "idlescreensaverenabled");
        y += kTrullaToggleTileHeight + kPanelRowGap;
    }
}

bool blink_tab_handle_click(TrullaState &state, const Config &cfg,
                            const TrullaCommitFn &on_commit,
                            const PanelClickRegion &region) {
    if (region.kind == PanelClickKind::MonitorSelect) {
        state.blink_selected_monitor = blink_monitor_from_tag(region.tag);
        trulla_request_frame(state);
        return true;
    }
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;

    trulla_commit_focused_field(state, cfg, on_commit);
    const std::string &mon = state.blink_selected_monitor;
    bool is_default = mon.empty();

    if (region.tag == "idlemanagementenabled") {
        Config updated = cfg;
        updated.blink_management_enabled = !cfg.blink_management_enabled;
        on_commit(updated);
    } else if (region.tag == "idleoverride") {
        Config updated = cfg;
        MonitorOverride &ov = updated.monitor_overrides[mon];
        if (!ov.enabled) {
            ov.spark = cfg.default_spark_enabled;
            ov.heralds = cfg.default_heralds_enabled;
            ov.autohide = cfg.autohide;
            ov.ambient_enabled = cfg.ambient_enabled;
            ov.ambient_timeout_seconds = cfg.ambient_timeout_seconds;
            ov.screensaver_enabled = cfg.screensaver_enabled;
            ov.screensaver_timeout_seconds = cfg.screensaver_timeout_seconds;
        }
        ov.enabled = !ov.enabled;
        on_commit(updated);
    } else if (region.tag == "idleambientenabled" ||
               region.tag == "idlescreensaverenabled") {
        Config updated = cfg;
        if (is_default) {
            if (region.tag == "idleambientenabled")
                updated.ambient_enabled = !cfg.ambient_enabled;
            else
                updated.screensaver_enabled = !cfg.screensaver_enabled;
        } else {
            MonitorOverride &ov = updated.monitor_overrides[mon];
            if (region.tag == "idleambientenabled")
                ov.ambient_enabled = !ov.ambient_enabled;
            else
                ov.screensaver_enabled = !ov.screensaver_enabled;
        }
        on_commit(updated);
    } else if (region.tag == "idleambientreset" && !is_default) {
        Config updated = cfg;
        updated.monitor_overrides[mon].ambient_timeout_seconds =
            cfg.ambient_timeout_seconds;
        on_commit(updated);
    } else if (region.tag == "idlescreensaverreset" && !is_default) {
        Config updated = cfg;
        updated.monitor_overrides[mon].screensaver_timeout_seconds =
            cfg.screensaver_timeout_seconds;
        on_commit(updated);
    } else {
        return false;
    }
    trulla_request_frame(state);
    return true;
}
