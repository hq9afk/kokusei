#include <algorithm>
#include <cmath>
#include <functional>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "core/log.h"

#include "modules/liyue.h"

#include "render/color_ops.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

const HyprMonitor *find_monitor_by_name(const HyprlandState &hypr,
                                        const std::string &name) {
    for (const HyprMonitor &m : hypr.monitors)
        if (m.name == name)
            return &m;
    return nullptr;
}

const HyprMonitor *find_monitor_by_id(const HyprlandState &hypr, int id) {
    for (const HyprMonitor &m : hypr.monitors)
        if (m.id == id)
            return &m;
    return nullptr;
}

double source_work_area_w(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    double base = rotated ? m.height : m.width;
    double reserved_lo = rotated ? m.reserved[1] : m.reserved[0];
    double reserved_hi = rotated ? m.reserved[3] : m.reserved[2];
    double scale = m.scale > 0.0 ? m.scale : 1.0;
    return base / scale - reserved_lo - reserved_hi;
}

double source_work_area_h(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    double base = rotated ? m.width : m.height;
    double reserved_lo = rotated ? m.reserved[0] : m.reserved[1];
    double reserved_hi = rotated ? m.reserved[2] : m.reserved[3];
    double scale = m.scale > 0.0 ? m.scale : 1.0;
    return base / scale - reserved_lo - reserved_hi;
}

int workspaces_shown() { return kLiyueRows * kLiyueColumns; }

int active_workspace_id(const HyprlandState &hypr,
                        const std::string &monitor_name) {
    auto it = hypr.by_monitor.find(monitor_name);
    if (it == hypr.by_monitor.end() || it->second.active_id < 0)
        return 1;
    return it->second.active_id;
}

struct GridLayout {
    Rect root;
    Rect background;
    Rect grid;
    float cell_w = 0.0f, cell_h = 0.0f;
};

GridLayout compute_grid_layout(const HyprMonitor &target, int surface_w,
                               int surface_h, float slide_y) {
    GridLayout g;
    double src_w = std::max(1.0, source_work_area_w(target));
    double src_h = std::max(1.0, source_work_area_h(target));
    g.cell_w = std::round(static_cast<float>(src_w * kLiyueScale));
    g.cell_h = std::round(static_cast<float>(src_h * kLiyueScale));
    float spacing = std::round(kLiyueWorkspaceSpacing);

    float grid_w = g.cell_w * kLiyueColumns + spacing * (kLiyueColumns - 1);
    float grid_h = g.cell_h * kLiyueRows + spacing * (kLiyueRows - 1);
    float bg_w = grid_w + kLiyueBackgroundPadding * 2.0f;
    float bg_h = grid_h + kLiyueBackgroundPadding * 2.0f;
    float root_w = bg_w + kLiyueElevationMargin * 2.0f;
    float root_h = bg_h + kLiyueElevationMargin * 2.0f;

    g.root = {std::round((surface_w - root_w) / 2.0f),
              std::round((surface_h - root_h) / 2.0f) + slide_y, root_w,
              root_h};
    g.background = {g.root.x + kLiyueElevationMargin,
                    g.root.y + kLiyueElevationMargin, bg_w, bg_h};
    g.grid = {g.background.x + kLiyueBackgroundPadding,
              g.background.y + kLiyueBackgroundPadding, grid_w, grid_h};
    return g;
}

Rect cell_rect(const GridLayout &g, int row, int col) {
    float spacing = std::round(kLiyueWorkspaceSpacing);
    return {g.grid.x + static_cast<float>(col) * (g.cell_w + spacing),
            g.grid.y + static_cast<float>(row) * (g.cell_h + spacing), g.cell_w,
            g.cell_h};
}

int workspace_id_at(int workspace_group, int row, int col) {
    return workspace_group * workspaces_shown() + row * kLiyueColumns + col + 1;
}

uint64_t tile_anim_owner(const std::string &address, int component) {
    return 1000 + (std::hash<std::string>{}(address) << 2) +
           static_cast<uint64_t>(component);
}

void animate_tile_rect(LiyueState &state, const std::string &address,
                       const Rect &from, const Rect &to) {
    auto anim = [&](int component, float from_v, float to_v,
                    float Rect::*field) {
        state.base.animations.animate(
            from_v, to_v, kLiyueAnimFastMs, Easing::EaseOutCubic,
            [&state, address, field](float v) {
                state.tile_anim[address].current.*field = v;
            },
            {}, tile_anim_owner(address, component));
    };
    anim(0, from.x, to.x, &Rect::x);
    anim(1, from.y, to.y, &Rect::y);
    anim(2, from.w, to.w, &Rect::w);
    anim(3, from.h, to.h, &Rect::h);
}

bool rect_equal(const Rect &a, const Rect &b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void rebuild_tiles(LiyueState &state, WaylandState &app, const GridLayout &g,
                   const HyprMonitor &target) {
    state.tiles.clear();
    int shown = workspaces_shown();
    int min_ws = state.workspace_group * shown + 1;
    int max_ws = (state.workspace_group + 1) * shown;

    std::vector<const HyprClient *> visible;
    for (const HyprClient &c : app.hypr.clients)
        if (c.workspace_id >= min_ws && c.workspace_id <= max_ws)
            visible.push_back(&c);

    std::sort(visible.begin(), visible.end(),
              [](const HyprClient *a, const HyprClient *b) {
                  if (a->pinned != b->pinned)
                      return !a->pinned;
                  if (a->floating != b->floating)
                      return !a->floating;
                  if ((a->fullscreen > 0) != (b->fullscreen > 0))
                      return !(a->fullscreen > 0);
                  if (a->workspace_id != b->workspace_id)
                      return a->workspace_id < b->workspace_id;
                  return a->focus_history_id > b->focus_history_id;
              });

    for (const HyprClient *c : visible) {
        const HyprMonitor *src = find_monitor_by_id(app.hypr, c->monitor_id);
        if (!src)
            src = &target;

        int local = (c->workspace_id - 1) % shown;
        int row = local / kLiyueColumns;
        int col = local % kLiyueColumns;
        Rect cell = cell_rect(g, row, col);

        double src_w = std::max(1.0, source_work_area_w(*src));
        double src_h = std::max(1.0, source_work_area_h(*src));
        double scale = std::min(cell.w / src_w, cell.h / src_h);

        double raw_x =
            std::max((c->at[0] - src->x - src->reserved[0]) * scale, 0.0);
        double raw_y =
            std::max((c->at[1] - src->y - src->reserved[1]) * scale, 0.0);
        double raw_w = std::max(1.0, c->size[0] * scale);
        double raw_h = std::max(1.0, c->size[1] * scale);

        double base_w = std::min(raw_w, static_cast<double>(cell.w));
        double base_h = std::min(raw_h, static_cast<double>(cell.h));
        double base_x = std::clamp(raw_x, 0.0, std::max(0.0, cell.w - base_w));
        double base_y = std::clamp(raw_y, 0.0, std::max(0.0, cell.h - base_h));

        LiyueWindowTile tile;
        tile.address = c->address;
        tile.workspace_id = c->workspace_id;
        tile.rect = {static_cast<float>(cell.x + base_x),
                     static_cast<float>(cell.y + base_y),
                     static_cast<float>(base_w), static_cast<float>(base_h)};
        state.tiles.push_back(tile);

        LiyueTileAnim &anim = state.tile_anim[tile.address];
        if (!anim.seen) {
            anim.current = tile.rect;
            anim.target = tile.rect;
            anim.seen = true;
        } else if (!rect_equal(anim.target, tile.rect)) {
            animate_tile_rect(state, tile.address, anim.target, tile.rect);
            anim.target = tile.rect;
        }
    }

    for (auto it = state.tile_anim.begin(); it != state.tile_anim.end();) {
        bool live = std::any_of(
            state.tiles.begin(), state.tiles.end(),
            [&](const LiyueWindowTile &t) { return t.address == it->first; });
        it = live ? std::next(it) : state.tile_anim.erase(it);
    }
}

bool point_in_rect(double px, double py, const Rect &r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

} // namespace

bool liyue_create_surface(LiyueState &state, wl_compositor *compositor,
                          zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-liyue", output);
}

bool liyue_init_egl(LiyueState &state, Renderer &renderer, EGLDisplay display,
                    EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state] {
        liyue_paint(state, *state.app_ptr);
    };
    return true;
}

void liyue_retarget(LiyueState &state, wl_compositor *compositor,
                    zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                    Renderer &renderer, EGLDisplay egl_display,
                    EGLConfig egl_config, EGLContext egl_context,
                    wl_output *target_output, const char *target_name) {
    wl_output *bound = overlay_panel_retarget(
        state.base, display, state.bound_output, target_output, target_name,
        [&](wl_output *out) {
            return liyue_create_surface(state, compositor, layer_shell, out);
        },
        [&] {
            return liyue_init_egl(state, renderer, egl_display, egl_config,
                                  egl_context);
        });
    if (bound)
        state.bound_output = bound;
}

void liyue_request_frame(LiyueState &state) {
    overlay_panel_request_frame(state.base);
}

void liyue_toggle(LiyueState &state, WaylandState &app, bool by_widget) {
    if (app.compositor_backend != WaylandState::CompositorBackend::Hyprland)
        return;
    if (!state.base.layer_surface || state.base.egl_surface == EGL_NO_SURFACE)
        return;

    bool opening = !state.base.open;
    if (opening) {
        hypr_refresh(app.hypr);
        state.opened_by_widget = by_widget;
        std::string monitor_name;
        for (auto &mon : app.outputs)
            if (mon->output.wl == state.bound_output)
                monitor_name = mon->output.name;
        int active_id = active_workspace_id(app.hypr, monitor_name);
        state.selected_workspace = active_id;
        state.workspace_group = (active_id - 1) / workspaces_shown();
        state.slide_y = static_cast<float>(state.base.height);
    } else {
        state.dragging = false;
    }
    overlay_panel_toggle(state.base);
    state.base.animations.animate(
        state.slide_y, opening ? 0.0f : static_cast<float>(state.base.height),
        kLiyueAnimFastMs, Easing::EaseOutCubic,
        [&state](float v) { state.slide_y = v; }, {}, kLiyueSlideOwner);
    liyue_request_frame(state);
}

std::vector<IpcHandler> liyue_ipc_handlers(LiyueState &liyue,
                                           WaylandState &state) {
    return {
        {"liyue",
         [&liyue, &state] {
             if (!liyue.base.open) {
                 MonitorOutput *target =
                     app_detail::active_target_monitor(state);
                 if (target && (target->output.wl != liyue.bound_output ||
                                !liyue.base.layer_surface))
                     liyue_retarget(liyue, state.compositor, state.layer_shell,
                                    state.display, state.renderer,
                                    state.egl_display, state.egl_config,
                                    state.egl_context, target->output.wl,
                                    target->output.name.c_str());
             }
             liyue_toggle(liyue, state);
         },
         "toggle the overview (Hyprland only)"},
    };
}

void liyue_handle_click(LiyueState &state, WaylandState &app, double px,
                        double py) {
    for (const LiyueWindowTile &tile : state.tiles) {
        auto it = state.tile_anim.find(tile.address);
        const Rect &drawn =
            it != state.tile_anim.end() ? it->second.current : tile.rect;
        if (!point_in_rect(px, py, drawn))
            continue;
        state.dragging = true;
        state.drag_address = tile.address;
        state.drag_from_workspace = tile.workspace_id;
        state.drag_target_workspace = -1;
        state.drag_offset_x = px - drawn.x;
        state.drag_offset_y = py - drawn.y;
        state.drag_pointer_x = px;
        state.drag_pointer_y = py;
        return;
    }

    std::string monitor_name;
    for (auto &mon : app.outputs)
        if (mon->output.wl == state.bound_output)
            monitor_name = mon->output.name;
    const HyprMonitor *target = find_monitor_by_name(app.hypr, monitor_name);
    if (!target) {
        liyue_toggle(state, app);
        return;
    }
    GridLayout g = compute_grid_layout(*target, state.base.width,
                                       state.base.height, state.slide_y);
    for (int row = 0; row < kLiyueRows; ++row) {
        for (int col = 0; col < kLiyueColumns; ++col) {
            Rect cell = cell_rect(g, row, col);
            if (!point_in_rect(px, py, cell))
                continue;
            int ws = workspace_id_at(state.workspace_group, row, col);
            state.selected_workspace = ws;
            hypr_tile_focus_workspace(app.hypr, ws);
            return;
        }
    }

    if (!point_in_rect(px, py, g.root))
        liyue_toggle(state, app);
}

bool liyue_point_is_clickable(LiyueState &state, WaylandState &app, double px,
                              double py) {
    if (!state.base.open)
        return false;
    for (const LiyueWindowTile &tile : state.tiles) {
        auto it = state.tile_anim.find(tile.address);
        const Rect &drawn =
            it != state.tile_anim.end() ? it->second.current : tile.rect;
        if (point_in_rect(px, py, drawn))
            return true;
    }
    std::string monitor_name;
    for (auto &mon : app.outputs)
        if (mon->output.wl == state.bound_output)
            monitor_name = mon->output.name;
    const HyprMonitor *target = find_monitor_by_name(app.hypr, monitor_name);
    if (!target)
        return false;
    GridLayout g = compute_grid_layout(*target, state.base.width,
                                       state.base.height, state.slide_y);
    for (int row = 0; row < kLiyueRows; ++row)
        for (int col = 0; col < kLiyueColumns; ++col)
            if (point_in_rect(px, py, cell_rect(g, row, col)))
                return true;
    return false;
}

void liyue_handle_pointer_move(LiyueState &state, WaylandState &app, double px,
                               double py) {
    if (!state.dragging)
        return;
    state.drag_pointer_x = px;
    state.drag_pointer_y = py;

    std::string monitor_name;
    for (auto &mon : app.outputs)
        if (mon->output.wl == state.bound_output)
            monitor_name = mon->output.name;
    const HyprMonitor *target = find_monitor_by_name(app.hypr, monitor_name);
    state.drag_target_workspace = -1;
    if (!target)
        return;
    GridLayout g = compute_grid_layout(*target, state.base.width,
                                       state.base.height, state.slide_y);
    for (int row = 0; row < kLiyueRows; ++row) {
        for (int col = 0; col < kLiyueColumns; ++col) {
            Rect cell = cell_rect(g, row, col);
            if (point_in_rect(px, py, cell)) {
                state.drag_target_workspace =
                    workspace_id_at(state.workspace_group, row, col);
                return;
            }
        }
    }
}

void liyue_handle_pointer_release(LiyueState &state, WaylandState &app) {
    if (!state.dragging)
        return;
    state.dragging = false;

    std::string monitor_name;
    for (auto &mon : app.outputs)
        if (mon->output.wl == state.bound_output)
            monitor_name = mon->output.name;
    const HyprMonitor *target = find_monitor_by_name(app.hypr, monitor_name);
    if (!target)
        return;
    GridLayout g = compute_grid_layout(*target, state.base.width,
                                       state.base.height, state.slide_y);
    for (int row = 0; row < kLiyueRows; ++row) {
        for (int col = 0; col < kLiyueColumns; ++col) {
            Rect cell = cell_rect(g, row, col);
            if (!point_in_rect(state.drag_pointer_x, state.drag_pointer_y,
                               cell))
                continue;
            int target_ws = workspace_id_at(state.workspace_group, row, col);
            if (target_ws != state.drag_from_workspace) {
                hypr_tile_move_window(app.hypr, target_ws, false,
                                      state.drag_address);
            } else {
                hypr_tile_focus_workspace(app.hypr, target_ws);
            }
            return;
        }
    }
}

void liyue_handle_key_event(LiyueState &state, WaylandState &app,
                            const KeyEvent &event) {
    if (!state.base.open)
        return;
    int shown = workspaces_shown();

    auto switch_to = [&](int ws, bool shift, bool alt) {
        state.selected_workspace = ws;
        state.workspace_group = (ws - 1) / shown;
        if (shift)
            hypr_tile_swap_workspace(app.hypr, ws);
        else if (alt)
            hypr_tile_move_workspace_in(app.hypr, ws);
        else
            hypr_tile_focus_workspace(app.hypr, ws);
    };

    switch (event.kind) {
    case KeyKind::Left:
    case KeyKind::Right:
    case KeyKind::Up:
    case KeyKind::Down: {
        int current = (state.selected_workspace - 1) % shown;
        if (current < 0)
            current += shown;
        int col = current % kLiyueColumns;
        int row = current / kLiyueColumns;
        if (event.kind == KeyKind::Left)
            col = (col - 1 + kLiyueColumns) % kLiyueColumns;
        else if (event.kind == KeyKind::Right)
            col = (col + 1) % kLiyueColumns;
        else if (event.kind == KeyKind::Up)
            row = (row - 1 + kLiyueRows) % kLiyueRows;
        else
            row = (row + 1) % kLiyueRows;
        switch_to(workspace_id_at(state.workspace_group, row, col), event.shift,
                  event.alt);
        break;
    }
    case KeyKind::Escape:
        liyue_toggle(state, app);
        break;
    case KeyKind::Text:
        if (event.text.size() == 1 && event.text[0] >= '0' &&
            event.text[0] <= '9') {
            int position = event.text[0] == '0' ? 10 : event.text[0] - '0';
            if (position <= shown)
                switch_to(state.workspace_group * shown + position, event.shift,
                          event.alt);
        } else if (event.ctrl && (event.text == "d" || event.text == "D")) {
            hypr_tile_close_workspace(app.hypr, HyprCloseScope::All);
        } else if (event.text == "D") {
            std::string monitor_name;
            for (auto &mon : app.outputs)
                if (mon->output.wl == state.bound_output)
                    monitor_name = mon->output.name;
            const HyprMonitor *target =
                find_monitor_by_name(app.hypr, monitor_name);
            if (target)
                hypr_tile_close_workspace(app.hypr, HyprCloseScope::Monitor,
                                          target->id);
        } else if (event.text == "d") {
            hypr_tile_close_workspace(app.hypr, HyprCloseScope::Workspace,
                                      state.selected_workspace);
        }
        break;
    default:
        break;
    }
    liyue_request_frame(state);
}

void liyue_paint(LiyueState &state, WaylandState &app) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    gl_make_current(state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
    state.renderer->begin_frame(state.base.width, state.base.height,
                                state.base.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    if (state.base.open &&
        app.compositor_backend == WaylandState::CompositorBackend::Hyprland) {
        std::string monitor_name;
        for (auto &mon : app.outputs)
            if (mon->output.wl == state.bound_output)
                monitor_name = mon->output.name;
        const HyprMonitor *target =
            find_monitor_by_name(app.hypr, monitor_name);

        if (target) {
            GridLayout g = compute_grid_layout(
                *target, state.base.width, state.base.height, state.slide_y);
            rebuild_tiles(state, app, g, *target);

            Node *bg = state.scene.root.claim_child();
            bg->kind = NodeKind::RoundedRect;
            bg->x = g.background.x;
            bg->y = g.background.y;
            bg->w = g.background.w;
            bg->h = g.background.h;
            bg->radius =
                kLiyueScreenRounding * kLiyueScale + kLiyueBackgroundPadding;
            bg->border_width = kLiyueBackgroundBorderWidth;
            static const float bg_fill[4] = {
                palette::field_bg.r, palette::field_bg.g, palette::field_bg.b,
                palette::field_bg.a * kLiyueBackgroundOpacity};
            static const float bg_border[4] = {
                palette::accent.r, palette::accent.g, palette::accent.b,
                palette::accent.a};
            bg->fill = bg_fill;
            bg->border = bg_border;

            int active_id = active_workspace_id(app.hypr, monitor_name);
            int active_local = (active_id - 1) % workspaces_shown();
            int active_row = active_local / kLiyueColumns;
            int active_col = active_local % kLiyueColumns;
            bool active_in_page =
                (active_id - 1) / workspaces_shown() == state.workspace_group;
            if (!active_in_page)
                state.indicator_tracking = false;

            for (int row = 0; row < kLiyueRows; ++row) {
                for (int col = 0; col < kLiyueColumns; ++col) {
                    Rect cell = cell_rect(g, row, col);
                    int ws = workspace_id_at(state.workspace_group, row, col);
                    bool hovered_while_dragging =
                        state.dragging && state.drag_target_workspace == ws;

                    Node *cellnode = state.scene.root.claim_child();
                    cellnode->kind = NodeKind::RoundedRect;
                    cellnode->x = cell.x;
                    cellnode->y = cell.y;
                    cellnode->w = cell.w;
                    cellnode->h = cell.h;
                    cellnode->radius = kLiyueScreenRounding * kLiyueScale;
                    cellnode->border_width = kLiyueWorkspaceBorderWidth;
                    static const float cell_fill[4] = {
                        palette::field_bg.r, palette::field_bg.g,
                        palette::field_bg.b, palette::field_bg.a};
                    static const float cell_border[4] = {
                        palette::text.r, palette::text.g, palette::text.b,
                        0.18f};
                    static const float cell_border_hover[4] = {
                        palette::text.r, palette::text.g, palette::text.b,
                        0.08f};
                    cellnode->fill = cell_fill;
                    cellnode->border = hovered_while_dragging
                                           ? cell_border_hover
                                           : cell_border;

                    Texture &num_tex = state.workspace_number_tex[ws];
                    if (!num_tex.id) {
                        RasterizedText num = rasterize_text_large(
                            std::to_string(ws), state.base.output_scale.scale);
                        if (num.width > 0)
                            num_tex = make_texture_from_raster(num);
                    }
                    if (num_tex.id) {
                        static const float num_tint[4] = {
                            palette::text.r, palette::text.g, palette::text.b,
                            1.0f - kLiyueWorkspaceNumberTextFade};
                        Node *label = state.scene.root.claim_child();
                        label->kind = NodeKind::Texture;
                        label->x = cell.x + (cell.w - num_tex.width) / 2.0f;
                        label->y = cell.y + (cell.h - num_tex.height) / 2.0f;
                        label->w = static_cast<float>(num_tex.width);
                        label->h = static_cast<float>(num_tex.height);
                        label->tex = &num_tex;
                        label->tint = num_tint;
                    }
                }
            }

            for (const LiyueWindowTile &tile : state.tiles) {
                toplevel_export_request(state.capture,
                                        app.toplevel_export_manager, app.shm,
                                        tile.address, kLiyueCaptureIntervalMs);
                const Texture *tex =
                    toplevel_export_texture(state.capture, tile.address);

                auto anim_it = state.tile_anim.find(tile.address);
                Rect r = anim_it != state.tile_anim.end()
                             ? anim_it->second.current
                             : tile.rect;
                if (state.dragging && state.drag_address == tile.address) {
                    r.x = static_cast<float>(state.drag_pointer_x -
                                             state.drag_offset_x);
                    r.y = static_cast<float>(state.drag_pointer_y -
                                             state.drag_offset_y);
                }

                if (tex && tex->id) {
                    Node *n = state.scene.root.claim_child();
                    n->kind = NodeKind::RoundedTexture;
                    n->x = r.x;
                    n->y = r.y;
                    n->w = r.w;
                    n->h = r.h;
                    n->radius = kLiyueWindowRounding * kLiyueScale;
                    n->tex = tex;
                    static const float white[4] = {1, 1, 1, 1};
                    n->tint = white;
                } else {
                    Node *n = state.scene.root.claim_child();
                    n->kind = NodeKind::RoundedRect;
                    n->x = r.x;
                    n->y = r.y;
                    n->w = r.w;
                    n->h = r.h;
                    n->radius = kLiyueWindowRounding * kLiyueScale;
                    n->border_width = kLiyueWindowPreviewBorderWidth;
                    static const float fill[4] = {
                        palette::field_bg.r, palette::field_bg.g,
                        palette::field_bg.b, palette::field_bg.a};
                    static const float border[4] = {
                        palette::accent.r, palette::accent.g, palette::accent.b,
                        palette::accent.a};
                    n->fill = fill;
                    n->border = border;
                }
            }

            std::vector<std::string> live_addresses;
            for (const HyprClient &c : app.hypr.clients)
                live_addresses.push_back(c.address);
            toplevel_export_prune(state.capture, live_addresses);

            if (active_in_page && state.slide_y == 0.0f) {
                Rect cell = cell_rect(g, active_row, active_col);
                if (!state.indicator_tracking ||
                    state.indicator_page != state.workspace_group) {
                    state.indicator_anim = cell;
                    state.indicator_target = cell;
                    state.indicator_tracking = true;
                    state.indicator_page = state.workspace_group;
                } else if (state.indicator_target.x != cell.x ||
                           state.indicator_target.y != cell.y) {
                    state.base.animations.animate(
                        state.indicator_anim.x, cell.x, kLiyueAnimFastMs,
                        Easing::EaseOutCubic,
                        [&state](float v) { state.indicator_anim.x = v; }, {},
                        kLiyueIndicatorXOwner);
                    state.base.animations.animate(
                        state.indicator_anim.y, cell.y, kLiyueAnimFastMs,
                        Easing::EaseOutCubic,
                        [&state](float v) { state.indicator_anim.y = v; }, {},
                        kLiyueIndicatorYOwner);
                    state.indicator_target = cell;
                }

                Node *indicator = state.scene.root.claim_child();
                indicator->kind = NodeKind::RoundedRect;
                indicator->x = state.indicator_anim.x;
                indicator->y = state.indicator_anim.y;
                indicator->w = cell.w;
                indicator->h = cell.h;
                indicator->radius = kLiyueScreenRounding * kLiyueScale;
                indicator->border_width = kLiyueFocusedIndicatorBorderWidth;
                static const float transparent[4] = {0, 0, 0, 0};
                static const float indicator_border[4] = {
                    palette::accent_alt.r, palette::accent_alt.g,
                    palette::accent_alt.b, palette::accent_alt.a};
                indicator->fill = transparent;
                indicator->border = indicator_border;
            }
        }
    }

    state.renderer->set_opacity(state.base.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive() || state.dragging)
        overlay_panel_request_frame(state.base);
}
