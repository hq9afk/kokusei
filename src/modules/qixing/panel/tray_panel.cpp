#include <GLES2/gl2.h>
#include <algorithm>
#include <linux/input-event-codes.h>

#include "modules/qixing/panel/tray_panel.h"

#include "render/icon.h"
#include "render/icons.h"
#include "render/image.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

float menu_content_height(const std::vector<MenuEntry> &level) {
    float h = kTrayMenuItemHeight;
    for (const MenuEntry &e : level)
        if (e.visible)
            h +=
                e.is_separator ? kTrayMenuSeparatorHeight : kTrayMenuItemHeight;
    return h;
}

float panel_chrome_top_offset() {
    return kPanelPadding + kPanelHeaderHeight + kPanelHeaderDividerGap + 1.0f +
           kPanelContentGap;
}

float panel_total_height(float content_h) {
    return panel_chrome_top_offset() + content_h + kPanelPadding;
}

std::vector<MenuEntry> *current_menu_level(TrayState &tray,
                                           TrayMenuState &state) {
    auto it = tray.menu_cache.find(state.item_key);
    if (it == tray.menu_cache.end())
        return nullptr;
    std::vector<MenuEntry> *level = &it->second;
    for (int32_t id : state.menu_path) {
        MenuEntry *found = nullptr;
        for (MenuEntry &entry : *level) {
            if (entry.id == id) {
                found = &entry;
                break;
            }
        }
        if (!found)
            return level;
        level = &found->children;
    }
    return level;
}

int32_t tray_menu_level_height(TrayState &tray, TrayMenuState &state) {
    std::vector<MenuEntry> *level = current_menu_level(tray, state);
    float content_h =
        level ? menu_content_height(*level) : kTrayMenuItemHeight * 2.0f;
    return static_cast<int32_t>(2.0f * kTrayMenuPadding + content_h + 0.5f);
}

} // namespace

void tray_menu_close(TrayMenuState &state) {
    if (!state.base.popup && !state.base.surface)
        return;
    popup_window_destroy(state.base);
    state.item_key.clear();
    state.menu_path.clear();
    state.applied_h = 0;
}

void tray_menu_open(TrayMenuState &state, TrayState &tray, const TrayItem &item,
                    const Rect &anchor_cell, const TrayMenuOpenArgs &args) {
    if (state.base.popup && state.item_key != item.key())
        tray_menu_close(state);

    bool fresh = !state.base.popup;
    state.item_key = item.key();
    state.menu_path.clear();
    state.anchor_cell = anchor_cell;
    if (!fresh) {
        popup_window_request_frame(state.base);
        return;
    }

    tray_menu_request(tray, item);
    int32_t menu_h = tray_menu_level_height(tray, state);

    state.renderer = args.renderer;
    state.wm_base = args.wm_base;
    if (!popup_window_create(state.base, args.compositor, args.wm_base,
                             args.parent_layer, anchor_cell,
                             static_cast<int32_t>(kTrayMenuWidth), menu_h,
                             args.seat, args.grab_serial))
        return;
    if (!popup_window_init_egl(state.base, args.display, args.egl_display,
                               args.egl_config, args.egl_context)) {
        popup_window_destroy(state.base);
        return;
    }
    state.applied_h = menu_h;
    state.base.frame_clock.draw = [&state, &tray] {
        tray_menu_paint(state, tray);
    };
    state.base.on_done = [&state] { state.base.done = true; };
    popup_window_request_frame(state.base);
}

void tray_menu_paint(TrayMenuState &state, TrayState &tray) {
    using namespace panel_chrome_detail;

    if (state.base.egl_surface == EGL_NO_SURFACE || state.base.done)
        return;

    std::vector<MenuEntry> *menu_level = current_menu_level(tray, state);
    bool item_exists = false;
    for (const TrayItem &it : tray.items)
        if (it.key() == state.item_key)
            item_exists = true;
    if (!item_exists) {
        state.base.done = true;
        return;
    }

    int32_t menu_h = tray_menu_level_height(tray, state);
    if (menu_h != state.applied_h) {
        popup_window_reposition(state.base, state.wm_base, state.anchor_cell,
                                static_cast<int32_t>(kTrayMenuWidth), menu_h);
        state.applied_h = menu_h;
        popup_window_request_frame(state.base);
        return;
    }
    if (!state.base.configured) {
        popup_window_request_frame(state.base);
        return;
    }

    eglMakeCurrent(state.base.egl_display, state.base.egl_surface,
                   state.base.egl_surface, state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.click_regions.clear();
    state.panel_rect = {};
    state.scene.rebuild();

    int32_t panel_h = state.base.height;

    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    Node *root = &state.scene.root;
    const float *white = rgba(palette::text);
    const float *dim = rgba(palette::text_dim);
    float panel_x = 0.0f;
    float panel_y = 0.0f;
    float panel_w = kTrayMenuWidth;
    float panel_h_f = static_cast<float>(panel_h);
    state.panel_rect = {panel_x, panel_y, panel_w, panel_h_f};

    node_add_rrect(root, panel_x, panel_y, panel_w, panel_h_f, kTrayMenuRadius,
                   kTrayMenuBorderWidth, rgba(palette::overlay),
                   rgba(palette::accent));

    float content_y = panel_y + kTrayMenuPadding;

    Rect back_rect = {panel_x + kTrayMenuPadding, content_y,
                      kTrayMenuItemHeight, kTrayMenuItemHeight};
    const Texture *back_tex =
        cached_icon(state.tcache, icon::chevron_left, scale);
    if (back_tex)
        node_add_texture(root,
                         back_rect.x + (back_rect.w - back_tex->width) / 2.0f,
                         back_rect.y + (back_rect.h - back_tex->height) / 2.0f,
                         *back_tex, white);
    state.click_regions.push_back(
        {PanelClickKind::TrayMenuBack, back_rect, ""});

    static const std::vector<MenuEntry> kEmptyLevel;
    const std::vector<MenuEntry> &rows_level =
        menu_level ? *menu_level : kEmptyLevel;

    float row_y = content_y + kTrayMenuItemHeight;
    for (const MenuEntry &entry : rows_level) {
        if (!entry.visible)
            continue;
        if (entry.is_separator) {
            Rect row_rect = {panel_x + kTrayMenuPadding, row_y,
                             panel_w - 2 * kTrayMenuPadding,
                             kTrayMenuSeparatorHeight};
            node_add_rect(
                root, row_rect.x + kTrayMenuSeparatorWidthOffset / 2.0f,
                row_y + kTrayMenuSeparatorHeight / 2.0f,
                row_rect.w - kTrayMenuSeparatorWidthOffset,
                kTrayMenuSeparatorLineHeight, rgba(palette::text_alpha06));
            row_y += kTrayMenuSeparatorHeight;
            continue;
        }
        Rect row_rect = {panel_x + kTrayMenuPadding, row_y,
                         panel_w - 2 * kTrayMenuPadding, kTrayMenuItemHeight};
        const float *label_color = entry.enabled ? white : dim;
        float text_x = row_rect.x + kTrayMenuRowPaddingH;
        if (entry.is_checkbox) {
            const Texture *check_tex =
                cached_icon(state.tcache, icon::check, scale);
            if (entry.checked && check_tex)
                node_add_texture(
                    root, text_x,
                    row_y + (kTrayMenuItemHeight - check_tex->height) / 2.0f,
                    *check_tex, label_color);
            text_x +=
                (check_tex ? check_tex->width : 0.0f) + kTrayMenuRowPaddingH;
        }
        int label_max_w = static_cast<int>(row_rect.x + row_rect.w - text_x -
                                           kTrayMenuLabelWidthOffset);
        const Texture *label_tex = cached_text_clipped(
            state.tcache, entry.label, scale, std::max(0, label_max_w));
        if (label_tex)
            node_add_texture(root, text_x,
                             row_y + (kTrayMenuItemHeight - label_tex->height) /
                                         2.0f,
                             *label_tex, label_color);
        if (!entry.children.empty()) {
            const Texture *chevron_tex =
                cached_icon(state.tcache, icon::chevron_right, scale);
            if (chevron_tex)
                node_add_texture(
                    root,
                    row_rect.x + row_rect.w - kTrayMenuRowPaddingH -
                        chevron_tex->width,
                    row_y + (kTrayMenuItemHeight - chevron_tex->height) / 2.0f,
                    *chevron_tex, dim);
        }
        if (entry.enabled)
            state.click_regions.push_back({PanelClickKind::TrayMenuEntry,
                                           row_rect, std::to_string(entry.id)});
        row_y += kTrayMenuItemHeight;
    }

    state.scene.draw(*state.renderer);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
}

void tray_menu_handle_click(TrayMenuState &state, TrayState &tray, double px,
                            double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        switch (region.kind) {
        case PanelClickKind::Close:
            tray_menu_close(state);
            return;
        case PanelClickKind::TrayMenuBack:
            if (state.menu_path.empty())
                tray_menu_close(state);
            else
                state.menu_path.pop_back();
            return;
        case PanelClickKind::TrayMenuEntry: {
            int32_t id = std::stoi(region.tag);
            std::vector<MenuEntry> *level = current_menu_level(tray, state);
            if (!level)
                return;
            for (const MenuEntry &entry : *level) {
                if (entry.id != id)
                    continue;
                if (!entry.children.empty()) {
                    state.menu_path.push_back(id);
                } else {
                    const TrayItem *item = nullptr;
                    for (const TrayItem &it : tray.items)
                        if (it.key() == state.item_key) {
                            item = &it;
                            break;
                        }
                    if (item)
                        tray_menu_event_clicked(tray, *item, id);
                    tray_menu_close(state);
                }
                return;
            }
            return;
        }
        default:
            return;
        }
    }
}

void tray_menu_handle_key_event(TrayMenuState &state, const KeyEvent &event) {
    if (event.kind != KeyKind::Escape)
        return;
    if (state.menu_path.empty())
        tray_menu_close(state);
    else
        state.menu_path.pop_back();
}

const Texture *tray_panel_detail_item_icon_texture(TrayPanelState &state,
                                                   const TrayItem &item) {
    std::string path = tray_item_icon_path(item);
    if (path.empty())
        return nullptr;
    auto it = state.icon_cache.find(path);
    if (it == state.icon_cache.end())
        it =
            state.icon_cache
                .emplace(path, load_image_texture(
                                   path, static_cast<int>(kTrayIconTargetSize)))
                .first;
    return it->second.id ? &it->second : nullptr;
}

bool tray_panel_create_surface(TrayPanelState &state, wl_compositor *compositor,
                               zwlr_layer_shell_v1 *layer_shell,
                               wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-tray-panel", output);
}

bool tray_panel_init_egl(TrayPanelState &state, Renderer &renderer,
                         TrayState &tray, EGLDisplay display, EGLConfig config,
                         EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state, &tray] {
        tray_panel_paint(state, tray, state.pending_pill_center_x,
                         state.pending_qixing_height,
                         state.pending_qixing_top_margin);
    };
    return true;
}

void tray_panel_request_frame(TrayPanelState &state, float pill_center_x,
                              float qixing_height, float qixing_top_margin) {
    state.pending_pill_center_x = pill_center_x;
    state.pending_qixing_height = qixing_height;
    state.pending_qixing_top_margin = qixing_top_margin;
    overlay_panel_request_frame(state.base);
}

void tray_panel_toggle(TrayPanelState &state, float pill_center_x) {
    panel_penance_toggle(
        state.base, state.locked_center_x, pill_center_x,
        [&state] { panel_reveal_open(state.reveal); },
        [&state] {
            panel_reveal_close(state.reveal, state.base,
                               [&state] { state.locked_center_x = -1.0f; });
        });
}

namespace {

float grid_content_height(size_t item_count) {
    size_t count = std::max<size_t>(item_count, 1);
    size_t rows = (count + kTrayColumns - 1) / kTrayColumns;
    return static_cast<float>(rows) * kTrayCellSize +
           static_cast<float>(rows - 1) * kTrayGridGap;
}

} // namespace

void tray_panel_paint(TrayPanelState &state, TrayState &tray,
                      float pill_center_x, float qixing_height,
                      float qixing_top_margin) {
    using namespace panel_chrome_detail;

    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    state.base.animations.tick(std::chrono::steady_clock::now());
    eglMakeCurrent(state.base.egl_display, state.base.egl_surface,
                   state.base.egl_surface, state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.click_regions.clear();
    state.panel_rect = {};
    state.scene.rebuild();

    if (!state.base.open) {
        state.scene.draw(*state.renderer);
        eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
        return;
    }

    Node *root = &state.scene.root;
    const float *white = rgba(palette::text);
    const float *dim = rgba(palette::text_dim);

    float panel_w = kTrayPanelWidth;

    if (state.locked_center_x < 0.0f)
        state.locked_center_x = pill_center_x;
    float target_h = panel_total_height(grid_content_height(tray.items.size()));
    float clip_h = panel_reveal_tick(state.reveal, state.base, target_h);
    float panel_h = std::max(0.0f, state.reveal.target);
    float panel_x = std::clamp(
        state.locked_center_x - panel_w / 2.0f, kPanelSideMargin,
        static_cast<float>(state.base.width) - panel_w - kPanelSideMargin);
    float panel_y = qixing_height + qixing_top_margin + kPanelGap;
    state.panel_rect = {panel_x, panel_y, panel_w, panel_h};

    panel_draw_box(root, panel_x, panel_y, panel_w, panel_h);
    panel_draw_header(root, state.tcache, scale, "Tray", panel_x, panel_y,
                      panel_w, state.click_regions);

    float divider_y =
        panel_y + kPanelPadding + kPanelHeaderHeight + kPanelHeaderDividerGap;
    node_add_rect(root, panel_x + kPanelPadding, divider_y,
                  panel_w - 2 * kPanelPadding, 1.0f,
                  rgba(palette::text_alpha06));

    float content_y = divider_y + 1.0f + kPanelContentGap;

    if (tray.items.empty()) {
        const Texture *t = cached_text(state.tcache, "No tray icons", scale);
        if (t)
            node_add_texture(root, panel_x + (panel_w - t->width) / 2.0f,
                             content_y, *t, dim);
    } else {
        for (size_t i = 0; i < tray.items.size(); ++i) {
            const TrayItem &item = tray.items[i];
            size_t col = i % kTrayColumns;
            size_t row = i / kTrayColumns;
            float cx = panel_x + kPanelPadding +
                       static_cast<float>(col) * (kTrayCellSize + kTrayGridGap);
            float cy = content_y +
                       static_cast<float>(row) * (kTrayCellSize + kTrayGridGap);
            Rect cell = {cx, cy, kTrayCellSize, kTrayCellSize};
            node_add_rrect(root, cell.x, cell.y, cell.w, cell.h, 8.0f, 0.0f,
                           rgba(palette::overlay), kPanelNoBorder);
            const Texture *icon_tex =
                tray_panel_detail_item_icon_texture(state, item);
            if (icon_tex) {
                node_add_texture_rect(
                    root, cell.x + (cell.w - kTrayIconTargetSize) / 2.0f,
                    cell.y + (cell.h - kTrayIconTargetSize) / 2.0f,
                    kTrayIconTargetSize, kTrayIconTargetSize, *icon_tex, white);
            } else {
                const Texture *fallback =
                    cached_icon(state.tcache, icon::apps, scale);
                if (fallback)
                    node_add_texture(
                        root, cell.x + (cell.w - fallback->width) / 2.0f,
                        cell.y + (cell.h - fallback->height) / 2.0f, *fallback,
                        white);
            }
            state.click_regions.push_back(
                {PanelClickKind::TrayActivate, cell, item.key()});
        }
    }

    if (clip_h + 0.5f < panel_h) {
        ScopedClip clip(*state.renderer, panel_x, panel_y, panel_w,
                        std::max(0.0f, clip_h));
        state.scene.draw(*state.renderer);
    } else {
        state.scene.draw(*state.renderer);
    }
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive())
        overlay_panel_request_frame(state.base);
}

TrayPanelClickResult tray_panel_handle_click(TrayPanelState &state,
                                             TrayState &tray,
                                             TrayMenuState &menu, double px,
                                             double py, uint32_t button) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        switch (region.kind) {
        case PanelClickKind::Close:
            tray_menu_close(menu);
            tray_panel_toggle(state);
            return {};
        case PanelClickKind::TrayActivate: {
            const TrayItem *item = nullptr;
            for (const TrayItem &it : tray.items)
                if (it.key() == region.tag) {
                    item = &it;
                    break;
                }
            if (!item)
                return {};
            if (button == BTN_RIGHT) {
                if (!item->has_menu)
                    return {};
                return {item, region.rect};
            }
            tray_menu_close(menu);
            tray_activate(tray, *item, false);
            return {};
        }
        default:
            return {};
        }
    }

    if (!hit(state.panel_rect, px, py)) {
        if (menu.base.popup)
            tray_menu_close(menu);
        else
            tray_panel_toggle(state);
    }
    return {};
}
