#include <GLES3/gl32.h>
#include <algorithm>
#include <chrono>

#include "app/monitor_output.h"
#include "app/user_info.h"
#include "app/wayland_state.h"

#include "core/log.h"
#include "core/path_home.h"

#include "modules/trulla.h"
#include "modules/trulla/blink_tab.h"
#include "modules/trulla/displays_tab.h"
#include "modules/trulla/resonance_tab.h"
#include "modules/trulla/starward_tab.h"

#include "render/gl.h"
#include "render/icons.h"
#include "render/renderer.h"

#include "service/expanse_service.h"
#include "service/trulla_service.h"

namespace {

constexpr TrullaTabDef kTrullaTabs[kTrullaTabCount] = {
    {kTrullaTabLabels[0], icon::wallpaper},
    {kTrullaTabLabels[1], icon::device_desktop},
    {kTrullaTabLabels[2], icon::moon_stars},
    {kTrullaTabLabels[3], icon::power},
    {kTrullaTabLabels[4], icon::wave_sine},
};

} // namespace

std::string trulla_detail_format_field(const Config &cfg, TrullaFieldId id,
                                       const std::string &monitor) {
    switch (id) {
    case TrullaFieldId::ExpansePath:
        return path_collapse_home(cfg.expanse_path);
    case TrullaFieldId::ExpanseDir:
        return path_collapse_home(cfg.expanse_dir);
    case TrullaFieldId::ExpanseAnimatedDir:
        return path_collapse_home(cfg.expanse_animated_dir);
    case TrullaFieldId::AmbientTimeout:
        return std::to_string(ambient_effective_timeout_seconds(cfg, monitor));
    case TrullaFieldId::ScreensaverTimeout:
        return std::to_string(
            screensaver_effective_timeout_seconds(cfg, monitor));
    case TrullaFieldId::ResonanceFps:
    case TrullaFieldId::ResonanceParticleThin:
    case TrullaFieldId::ResonanceParticleSize:
    case TrullaFieldId::ResonanceComplexity:
    case TrullaFieldId::ResonanceGlowDirections:
    case TrullaFieldId::ResonanceGlowQuality:
        return resonance_field_text(cfg.resonance, id);
    default:
        return "";
    }
}

bool trulla_create_surface(TrullaState &state, wl_compositor *compositor,
                           zwlr_layer_shell_v1 *layer_shell,
                           wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-trulla", output);
}

bool trulla_init_egl(TrullaState &state, const Config &cfg, Renderer &renderer,
                     EGLDisplay display, EGLConfig config, EGLContext context,
                     std::function<std::vector<std::string>()> monitor_names_fn,
                     std::function<std::string()> focused_monitor_fn,
                     std::function<MediaDecodeStatus(const std::string &, int)>
                         expanse_decode_status_fn) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.expanse_decode_status = std::move(expanse_decode_status_fn);
    state.base.frame_clock.draw = [&state, &cfg, monitor_names_fn,
                                   focused_monitor_fn] {
        trulla_paint(state, cfg, monitor_names_fn(), focused_monitor_fn());
    };
    state.expanse_static.picker.request_frame = [&state] {
        trulla_request_frame(state);
    };
    state.expanse_animated.picker.request_frame = [&state] {
        trulla_request_frame(state);
    };

    AnimatedImageStyle pfp_style;
    pfp_style.size = kTrullaProfileAvatarSize;
    pfp_style.circular = true;
    pfp_style.ring_fill = rgba(palette::text_alpha04);
    pfp_style.border_color = rgba(palette::accent);
    pfp_style.border_width = kTrullaProfileAvatarBorderWidth;
    pfp_style.decode = {15, static_cast<int>(kTrullaProfileAvatarSize) * 2};
    animated_image_set_source(state.profile_pic,
                              user_info::profile_media_path(), pfp_style);
    return true;
}

void trulla_request_frame(TrullaState &state) {
    overlay_panel_request_frame(state.base);
}

void trulla_commit_focused_field(TrullaState &state, const Config &cfg,
                                 const TrullaCommitFn &on_commit) {
    if (state.focused_field == TrullaFieldId::None)
        return;
    Config updated = cfg;
    trulla_service_apply_field_text(updated, state.focused_field,
                                    state.field_buffer.text,
                                    state.blink_selected_monitor);
    on_commit(updated);
    state.focused_field = TrullaFieldId::None;
    state.field_buffer.preedit.clear();
    text_field_type_anim_clear(state.field_anim, state.base.animations,
                               kTrullaFieldTypeAnimOwnerBase);
    if (state.sync_text_input_focus)
        state.sync_text_input_focus(false);
}

void trulla_toggle(TrullaState &state, const Config &cfg,
                   const TrullaCommitFn &on_commit) {
    if (!state.base.layer_surface || state.base.egl_surface == EGL_NO_SURFACE) {
        klog("settings: toggle ignored, surface not ready (layer_surface=%p "
             "egl_surface_ready=%d)",
             static_cast<void *>(state.base.layer_surface),
             state.base.egl_surface != EGL_NO_SURFACE);
        return;
    }
    klog("settings: toggle called (was_open=%d opacity=%.2f "
         "focused_field=%d)",
         state.base.open, static_cast<double>(state.base.opacity),
         static_cast<int>(state.focused_field));
    bool opening = !state.base.open;
    if (state.base.open) {
        trulla_commit_focused_field(state, cfg, on_commit);
    } else {
        state.active_tab = TrullaTab::Expanse;
    }
    overlay_panel_toggle(state.base);
    if (opening)
        animated_image_show(state.profile_pic,
                            [&state] { trulla_request_frame(state); });
    trulla_request_frame(state);
}

std::vector<IpcHandler> trulla_ipc_handlers(TrullaState &trulla,
                                            WaylandState &state) {
    return {
        {"trulla",
         [&trulla, &state] {
             if (!trulla.base.open && state.trulla_enabled) {
                 MonitorOutput *target =
                     app_detail::active_target_monitor(state);
                 if (target &&
                     (target->output.wl != state.trulla_bound_output ||
                      !trulla.base.layer_surface))
                     app_detail::trulla_retarget(state, trulla, *target);
             }
             trulla_toggle(trulla, state.cfg, [&state](Config c) {
                 app_detail::save_and_apply_config_update(state, c);
             });
         },
         "toggle the settings panel"},
    };
}

void trulla_focus_field(TrullaState &state, const Config &cfg,
                        const TrullaCommitFn &on_commit, TrullaFieldId id) {
    if (id == state.focused_field)
        return;
    trulla_commit_focused_field(state, cfg, on_commit);
    state.focused_field = id;
    state.field_buffer.text =
        trulla_detail_format_field(cfg, id, state.blink_selected_monitor);
    state.field_buffer.cursor_blink_visible = true;
    text_field_type_anim_settle(state.field_anim, state.base.animations,
                                kTrullaFieldTypeAnimOwnerBase,
                                state.field_buffer.text);
    if (state.sync_text_input_focus)
        state.sync_text_input_focus(true);
}

void trulla_handle_click(TrullaState &state, const Config &cfg,
                         const TrullaCommitFn &on_commit, double px,
                         double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };

    klog("settings: handle_click at (%.0f,%.0f), panel_rect=%.0f,%.0f "
         "%.0fx%.0f",
         px, py, static_cast<double>(state.panel_rect.x),
         static_cast<double>(state.panel_rect.y),
         static_cast<double>(state.panel_rect.w),
         static_cast<double>(state.panel_rect.h));
    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        klog("settings: click (%.0f,%.0f) hit region kind=%d", px, py,
             static_cast<int>(region.kind));
        switch (region.kind) {
        case PanelClickKind::Close:
            trulla_toggle(state, cfg, on_commit);
            return;
        case PanelClickKind::TabSelect:
            trulla_commit_focused_field(state, cfg, on_commit);
            state.active_tab = static_cast<TrullaTab>(std::stoi(region.tag));
            trulla_request_frame(state);
            return;
        case PanelClickKind::ToggleFlip:
            if (!expanse_tab_handle_click(state, cfg, on_commit, region) &&
                !displays_tab_handle_click(state, cfg, on_commit, region) &&
                !blink_tab_handle_click(state, cfg, on_commit, region) &&
                !resonance_tab_handle_click(state, cfg, on_commit, region))
                starward_tab_handle_click(state, cfg, on_commit, region);
            return;
        case PanelClickKind::FieldFocus:
            trulla_focus_field(
                state, cfg, on_commit,
                static_cast<TrullaFieldId>(std::stoi(region.tag)));
            trulla_request_frame(state);
            return;
        case PanelClickKind::MonitorSelect:
            if (state.active_tab == TrullaTab::Displays)
                displays_tab_handle_click(state, cfg, on_commit, region);
            else if (state.active_tab == TrullaTab::Blink)
                blink_tab_handle_click(state, cfg, on_commit, region);
            else
                expanse_tab_handle_click(state, cfg, on_commit, region);
            return;
        case PanelClickKind::RegionSelect:
        case PanelClickKind::AnimatedRegionSelect:
            expanse_tab_handle_click(state, cfg, on_commit, region);
            return;
        case PanelClickKind::ExpanseSelect:
        case PanelClickKind::AnimatedExpanseSelect:
            expanse_tab_handle_click(state, cfg, on_commit, region);
            return;
        default:
            return;
        }
    }

    klog("settings: click (%.0f,%.0f) hit no region (%zu checked, "
         "panel_rect=%.0f,%.0f %.0fx%.0f)",
         px, py, state.click_regions.size(),
         static_cast<double>(state.panel_rect.x),
         static_cast<double>(state.panel_rect.y),
         static_cast<double>(state.panel_rect.w),
         static_cast<double>(state.panel_rect.h));
    if (!hit(state.panel_rect, px, py)) {
        trulla_toggle(state, cfg, on_commit);
        return;
    }
}

void trulla_handle_key_event(TrullaState &state, const Config &cfg,
                             const TrullaCommitFn &on_commit,
                             const KeyEvent &event) {
    if (state.focused_field == TrullaFieldId::None) {
        if (event.kind == KeyKind::Escape) {
            klog("settings: escape with no field focused -> toggle close");
            trulla_toggle(state, cfg, on_commit);
        }
        return;
    }
    switch (text_field_handle_key(state.field_buffer, event)) {
    case TextFieldResult::Changed:
        text_field_type_anim_sync(state.field_anim, state.base.animations,
                                  kTrullaFieldTypeAnimOwnerBase,
                                  state.field_buffer.text);
        trulla_request_frame(state);
        break;
    case TextFieldResult::Committed:
        klog("settings: field %d committed",
             static_cast<int>(state.focused_field));
        trulla_commit_focused_field(state, cfg, on_commit);
        trulla_request_frame(state);
        break;
    case TextFieldResult::Cancelled:
        klog("settings: field %d edit cancelled",
             static_cast<int>(state.focused_field));
        state.focused_field = TrullaFieldId::None;
        text_field_type_anim_clear(state.field_anim, state.base.animations,
                                   kTrullaFieldTypeAnimOwnerBase);
        trulla_request_frame(state);
        break;
    case TextFieldResult::None:
        break;
    }
}

using panel_chrome_detail::cached_icon;
using panel_chrome_detail::cached_text;

namespace {

float draw_profile_block(TrullaState &state, Node *parent, int32_t scale,
                         float x, float y, float w) {
    float avatar_x = x;
    float avatar_y = y + kTrullaProfileTopPadding;
    animated_image_draw(state.profile_pic, parent, avatar_x, avatar_y,
                        kTrullaProfileAvatarSize, kTrullaProfileAvatarSize,
                        1.0f);
    if (state.profile_pic.frames.empty()) {
        const Texture *avatar_icon =
            cached_icon(state.tcache, icon::user, scale);
        if (avatar_icon)
            node_add_texture(
                parent,
                avatar_x +
                    (kTrullaProfileAvatarSize - avatar_icon->width) / 2.0f,
                avatar_y +
                    (kTrullaProfileAvatarSize - avatar_icon->height) / 2.0f,
                *avatar_icon, rgba(palette::text));
    }

    float text_x =
        avatar_x + kTrullaProfileAvatarSize + kTrullaProfileAvatarLabelGap;
    const Texture *name_tex =
        cached_text(state.tcache, user_info::username(), scale);
    const Texture *uptime_tex =
        cached_text(state.tcache, user_info::uptime_string(), scale);
    float info_h = (name_tex ? name_tex->height : 0) + kTrullaProfileLineGap +
                   (uptime_tex ? uptime_tex->height : 0);
    float text_y = avatar_y + (kTrullaProfileAvatarSize - info_h) / 2.0f;
    if (name_tex) {
        node_add_texture(parent, text_x, text_y, *name_tex,
                         rgba(palette::text));
        text_y += name_tex->height + kTrullaProfileLineGap;
    }
    if (uptime_tex)
        node_add_texture(parent, text_x, text_y, *uptime_tex,
                         rgba(palette::text_dim));

    float block_h = kTrullaProfileTopPadding + kTrullaProfileAvatarSize +
                    kTrullaProfileBottomPadding;
    node_add_rect(parent, x, y + block_h, w, 1.0f, rgba(palette::text_alpha11));
    return block_h + kTrullaProfileDividerGap;
}

void draw_nav_rail(TrullaState &state, Node *parent, int32_t scale, float x,
                   float y, float w, float h, bool expanded) {
    node_add_rrect(parent, x, y, w, h, metrics::radius_md, 0.0f,
                   rgba(palette::text_alpha04), kPanelNoBorder);
    float row_y = y + kTrullaRailPadding;
    for (int i = 0; i < kTrullaTabCount; ++i) {
        bool active = static_cast<int>(state.active_tab) == i;
        const float *row_color =
            active ? rgba(palette::accent) : rgba(palette::text_dim);
        node_add_rrect(parent, x, row_y, w, kTrullaRailItemHeight,
                       metrics::radius_sm, 0.0f,
                       active ? rgba(palette::accent_alpha19) : kPanelNoBorder,
                       kPanelNoBorder);

        float icon_x = x + kTrullaRailPadding;
        const Texture *icon_tex =
            cached_icon(state.tcache, kTrullaTabs[i].icon, scale);
        if (icon_tex)
            node_add_texture(
                parent, icon_x,
                row_y + (kTrullaRailItemHeight - icon_tex->height) / 2.0f,
                *icon_tex, row_color);

        if (expanded) {
            float label_x = icon_x + (icon_tex ? icon_tex->width : 0) +
                            kTrullaRailIconLabelGap;
            const Texture *label_tex =
                cached_text(state.tcache, kTrullaTabs[i].label, scale);
            if (label_tex)
                node_add_texture(
                    parent, label_x,
                    row_y + (kTrullaRailItemHeight - label_tex->height) / 2.0f,
                    *label_tex, row_color);
        }

        state.click_regions.push_back({PanelClickKind::TabSelect,
                                       {x, row_y, w, kTrullaRailItemHeight},
                                       std::to_string(i)});
        row_y += kTrullaRailItemHeight + kTrullaRailItemGap;
    }
}

} // namespace

void draw_toggle_switch(TrullaState &state, Node *parent, float x, float y,
                        bool active, const char *tag) {
    panel_draw_toggle_switch(parent, state.click_regions, x, y,
                             kTrullaToggleTrackWidth, kTrullaToggleTrackHeight,
                             kTrullaToggleKnobSize, kTrullaToggleKnobInset,
                             active, PanelClickKind::ToggleFlip, tag);
}

void draw_toggle_row(TrullaState &state, Node *parent, int32_t scale, float x,
                     float y, float w, const std::string &label, bool value,
                     const char *tag, bool tiled) {
    float row_h = tiled ? kTrullaToggleTileHeight : kTrullaToggleTrackHeight;
    float inset = tiled ? kTrullaToggleTileContentMargin : 0.0f;

    if (tiled)
        node_add_rrect(parent, x, y, w, row_h, kTrullaTileRadius,
                       kTrullaToggleTileBorderWidth,
                       rgba(palette::text_alpha04),
                       rgba(palette::text_alpha07));

    const Texture *label_tex = cached_text(state.tcache, label, scale);
    if (label_tex)
        node_add_texture(parent, x + inset,
                         y + (row_h - label_tex->height) / 2.0f, *label_tex,
                         tiled ? rgba(palette::text_alpha85)
                               : rgba(palette::text));

    float switch_x = x + w - inset - kTrullaToggleTrackWidth;
    float switch_y = y + (row_h - kTrullaToggleTrackHeight) / 2.0f;
    draw_toggle_switch(state, parent, switch_x, switch_y, value, tag);
}

void trulla_paint(TrullaState &state, const Config &cfg,
                  const std::vector<std::string> &monitor_names,
                  const std::string &focused_monitor) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    state.monitor_names = monitor_names;
    auto sync_region_selection = [&](ExpanseSubtabState &sub, bool animated) {
        if (monitor_names.size() == 1) {
            sub.selected_region = monitor_names[0];
        } else if (sub.selected_region.empty() ||
                   std::find(monitor_names.begin(), monitor_names.end(),
                             sub.selected_region) == monitor_names.end()) {
            sub.selected_region =
                !focused_monitor.empty() &&
                        std::find(monitor_names.begin(), monitor_names.end(),
                                  focused_monitor) != monitor_names.end()
                    ? focused_monitor
                : monitor_names.empty() ? ""
                                        : monitor_names[0];
        }
        int count = sub.selected_region.empty()
                        ? 1
                        : expanse_service_column_count(cfg, sub.selected_region,
                                                       animated);
        sub.selected_column = std::clamp(sub.selected_column, 0, count - 1);
    };
    sync_region_selection(state.expanse_static, false);
    sync_region_selection(state.expanse_animated, true);

    if (!state.displays_selected_monitor.empty() &&
        std::find(monitor_names.begin(), monitor_names.end(),
                  state.displays_selected_monitor) == monitor_names.end())
        state.displays_selected_monitor.clear();
    if (!state.blink_selected_monitor.empty() &&
        std::find(monitor_names.begin(), monitor_names.end(),
                  state.blink_selected_monitor) == monitor_names.end())
        state.blink_selected_monitor.clear();
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    if (state.base.open)
        animated_image_tick(state.profile_pic, now);
    else
        animated_image_hide(state.profile_pic);
    gl_make_current(state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    state.renderer->set_opacity(state.base.opacity);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();
    state.click_regions.clear();
    Node *root = &state.scene.root;

    if (state.base.opacity > 0.0f) {
        float panel_w = std::min(static_cast<float>(state.base.width) -
                                     kTrullaWindowCardWidthInset,
                                 kTrullaWindowCardMaxWidth);
        float panel_h = std::min(static_cast<float>(state.base.height) -
                                     kTrullaWindowCardHeightInset,
                                 kTrullaWindowCardMaxHeight);
        float panel_x = (static_cast<float>(state.base.width) - panel_w) / 2.0f;
        float panel_y =
            (static_cast<float>(state.base.height) - panel_h) / 2.0f;
        state.panel_rect = {panel_x, panel_y, panel_w, panel_h};

        panel_draw_box(root, panel_x, panel_y, panel_w, panel_h,
                       metrics::border_thick);
        panel_draw_header(root, state.tcache, scale, "Settings", panel_x,
                          panel_y, panel_w, state.click_regions);

        float content_y = panel_y + kPanelPadding + kPanelHeaderHeight +
                          kPanelHeaderDividerGap;
        node_add_rect(root, panel_x + kPanelPadding, content_y,
                      panel_w - kPanelPadding * 2.0f, 1.0f,
                      rgba(palette::text_alpha11));
        content_y += 1.0f + kPanelContentGap;

        bool rail_expanded = panel_w > kTrullaNavRailCollapseBreakpoint;
        float rail_width = rail_expanded ? kTrullaNavRailExpandedWidth
                                         : kTrullaNavRailCollapsedWidth;

        float rail_x = panel_x + kPanelPadding;
        float profile_h = draw_profile_block(state, root, scale, rail_x,
                                             content_y, rail_width);
        float rail_y = content_y + profile_h;
        float rail_h = panel_y + panel_h - kPanelPadding - rail_y;
        draw_nav_rail(state, root, scale, rail_x, rail_y, rail_width, rail_h,
                      rail_expanded);

        float divider_x = rail_x + rail_width + kTrullaRailDividerGap;
        node_add_rect(root, divider_x, content_y, 1.0f, rail_h,
                      rgba(palette::text_alpha11));

        float label_x = divider_x + kTrullaRailDividerGap;
        float y = content_y;

        switch (state.active_tab) {
        case TrullaTab::Expanse:
            expanse_tab_paint(state, root, scale, label_x, y, cfg);
            break;
        case TrullaTab::Displays: {
            float row_w = panel_x + panel_w - kPanelPadding - label_x;
            displays_tab_paint(state, root, scale, label_x, y, row_w, cfg);
            break;
        }
        case TrullaTab::Blink: {
            float row_w = panel_x + panel_w - kPanelPadding - label_x;
            blink_tab_paint(state, root, scale, label_x, y, row_w, cfg);
            break;
        }
        case TrullaTab::Starward: {
            float row_w = panel_x + panel_w - kPanelPadding - label_x;
            starward_tab_paint(state, root, scale, label_x, y, row_w, cfg);
            break;
        }
        case TrullaTab::Resonance: {
            float row_w = panel_x + panel_w - kPanelPadding - label_x;
            resonance_tab_paint(state, root, scale, label_x, y, row_w, cfg);
            break;
        }
        }
    }

    state.scene.draw(*state.renderer);
    if (state.base.animations.hasActive() ||
        animated_image_animating(state.profile_pic))
        overlay_panel_request_frame(state.base);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
}

bool trulla_point_is_clickable(const TrullaState &state, double px, double py) {
    return panel_region_hit(state.click_regions, px, py);
}

void trulla_handle_scroll(TrullaState &state, double dy) {
    if (state.active_tab != TrullaTab::Expanse)
        return;
    expanse_tab_handle_scroll(state, dy);
}

TextInputState trulla_text_input_state(const TrullaState &state) {
    TextInputState s;
    s.purpose = TextInputPurpose::Normal;
    const Rect &r = state.field_buffer.cursor_rect;
    s.cursor_rect_x = static_cast<int32_t>(r.x);
    s.cursor_rect_y = static_cast<int32_t>(r.y);
    s.cursor_rect_w = static_cast<int32_t>(r.w);
    s.cursor_rect_h = static_cast<int32_t>(r.h);
    return s;
}

void trulla_text_input_apply_edit(TrullaState &state,
                                  const TextInputEdit &edit) {
    if (edit.has_delete)
        for (uint32_t i = 0; i < edit.delete_before_length; ++i)
            text_field_backspace(state.field_buffer.text);
    if (edit.has_commit_text) {
        state.field_buffer.text += edit.commit_text;
        state.field_buffer.preedit.clear();
    }
    if (edit.has_preedit)
        state.field_buffer.preedit = edit.preedit_text;
    state.field_buffer.cursor_blink_visible = true;
    if (edit.has_delete || edit.has_commit_text)
        text_field_type_anim_sync(state.field_anim, state.base.animations,
                                  kTrullaFieldTypeAnimOwnerBase,
                                  state.field_buffer.text);
}
