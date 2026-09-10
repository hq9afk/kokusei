#include <GLES3/gl32.h>
#include <cmath>

#include "modules/dock.h"

#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"

namespace {

int32_t dock_current_height(const DockState &state) {
    if (state.autohide.enabled && state.autohide.collapsed)
        return kDockPeekHeight;
    if (state.autohide.enabled)
        return kDockCapsuleHeight + kDockMarginBottom;
    return kDockCapsuleHeight;
}

void dock_apply_geometry(DockState &state) {
    if (!state.layer_surface)
        return;
    int32_t height = dock_current_height(state);
    int32_t margin_bottom = state.autohide.enabled ? 0 : kDockMarginBottom;
    int32_t zone = 0;
    if (!state.autohide.enabled)
        zone =
            state.entries.empty() ? 0 : kDockCapsuleHeight + kDockMarginBottom;
    zwlr_layer_surface_v1_set_size(state.layer_surface, 0, height);
    zwlr_layer_surface_v1_set_margin(state.layer_surface, 0, 0, margin_bottom,
                                     0);
    zwlr_layer_surface_v1_set_exclusive_zone(state.layer_surface, zone);
    state.last_exclusive_zone = zone;
    wl_surface_commit(state.surface);
    int32_t scale = state.output_scale.scale;
    if (state.egl_window)
        wl_egl_window_resize(state.egl_window, state.width * scale,
                             height * scale, 0, 0);
}

void dock_layer_surface_configure(void *data,
                                  zwlr_layer_surface_v1 *layer_surface,
                                  uint32_t serial, uint32_t width, uint32_t) {
    auto *state = static_cast<DockState *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    bool changed = state->width != static_cast<int32_t>(width);
    state->width = static_cast<int32_t>(width);
    if (changed && state->egl_window) {
        int32_t scale = state->output_scale.scale;
        wl_egl_window_resize(state->egl_window, state->width * scale,
                             dock_current_height(*state) * scale, 0, 0);
        if (state->frame_clock.surface)
            request_frame(state->frame_clock);
    }
    state->configured = true;
}

void dock_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener dock_layer_surface_listener = {
    .configure = dock_layer_surface_configure,
    .closed = dock_layer_surface_closed,
};

void dock_update_autohide(DockState &state) {
    if (!state.autohide.enabled)
        return;
    bool want_shown =
        state.pointer && state.pointer->focused_surface == state.surface;
    if (want_shown != state.autohide.hidden)
        return;
    state.autohide.hidden = !want_shown;
    if (want_shown && state.autohide.collapsed) {
        state.autohide.collapsed = false;
        dock_apply_geometry(state);
    }
    float target = want_shown ? 1.0f : 0.0f;
    float duration = want_shown ? kDockAutoHideRevealMs : kDockAutoHideHideMs;
    state.animations.animate(
        state.autohide.opacity, target, duration, Easing::EaseOutCubic,
        [&state](float v) { state.autohide.opacity = v; },
        [&state] {
            if (state.autohide.hidden && !state.autohide.collapsed) {
                state.autohide.collapsed = true;
                dock_apply_geometry(state);
            }
        },
        kDockAutoHideAnimOwner);
}

void dock_paint(DockState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;

    gl_make_current(state.egl_display, state.egl_surface, state.egl_context);

    state.animations.tick(std::chrono::steady_clock::now());
    dock_update_autohide(state);

    int32_t height = dock_current_height(state);
    state.renderer->begin_frame(state.width, height, state.output_scale.scale);
    state.renderer->set_opacity(state.autohide.enabled ? state.autohide.opacity
                                                       : 1.0f);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();
    if (!state.entries.empty()) {
        float row_w = dock_row_width(state.entries);
        float capsule_w = row_w + kDockPaddingH * 2.0f;
        float capsule_x =
            std::round((static_cast<float>(state.width) - capsule_w) / 2.0f);
        node_add_rrect(&state.scene.root, capsule_x, 0.0f, capsule_w,
                       kDockCapsuleHeight, metrics::radius_md,
                       metrics::border_thin, rgba(palette::overlay),
                       rgba(palette::accent));
        draw_dock_row(&state.scene.root, state.icons, state.row,
                      state.animations, capsule_x + kDockPaddingH,
                      kDockCapsuleHeight / 2.0f, state.entries,
                      kDockAnimOwnerBase);
    }
    state.scene.draw(*state.renderer);
    eglSwapBuffers(state.egl_display, state.egl_surface);

    if (state.animations.hasActive())
        request_frame(state.frame_clock);
}

} // namespace

bool dock_create_surface(DockState &state, wl_compositor *compositor,
                         zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    state.compositor = compositor;
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        .name_space = "kokusei-dock",
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        .height = kDockCapsuleHeight,
        .margin_bottom = kDockMarginBottom,
        .exclusive_zone = 0,
        .empty_input_region = true,
    };
    state.layer_surface =
        layer_surface_create(state.surface, compositor, layer_shell, cfg,
                             &dock_layer_surface_listener, &state, output);
    if (!state.layer_surface)
        return false;
    state.output_scale.on_change = [&state](int32_t scale) {
        if (state.egl_window)
            wl_egl_window_resize(state.egl_window, state.width * scale,
                                 dock_current_height(state) * scale, 0, 0);
        if (state.frame_clock.surface)
            request_frame(state.frame_clock);
    };
    output_scale_watch(state.output_scale, state.surface);
    wl_surface_commit(state.surface);
    return true;
}

bool dock_init_egl(DockState &state, Renderer &renderer, EGLDisplay display,
                   EGLConfig config, EGLContext context) {
    state.egl_display = display;
    state.egl_context = context;
    state.renderer = &renderer;
    int32_t scale = state.output_scale.scale;
    state.egl_window = wl_egl_window_create(state.surface, state.width * scale,
                                            dock_current_height(state) * scale);
    state.egl_surface = eglCreateWindowSurface(
        display, config,
        reinterpret_cast<EGLNativeWindowType>(state.egl_window), nullptr);
    if (state.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, state.egl_surface, context))
        return false;
    state.frame_clock.surface = state.surface;
    state.frame_clock.draw = [&state] { dock_paint(state); };
    return true;
}

void dock_request_frame(DockState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(state.frame_clock);
}

void dock_refresh(DockState &state) {
    if (!state.hypr)
        return;
    state.entries = dock_entries_for_monitor(*state.hypr, state.output_name);
    if (!state.autohide.enabled) {
        int32_t zone =
            state.entries.empty() ? 0 : kDockCapsuleHeight + kDockMarginBottom;
        if (zone != state.last_exclusive_zone && state.layer_surface) {
            zwlr_layer_surface_v1_set_exclusive_zone(state.layer_surface, zone);
            state.last_exclusive_zone = zone;
        }
    }
    dock_request_frame(state);
}

void dock_apply_autohide(DockState &state, bool enabled) {
    state.autohide.enabled = enabled;
    state.autohide.hidden = false;
    state.autohide.collapsed = false;
    state.autohide.opacity = 1.0f;
    if (state.surface && state.compositor) {
        if (enabled) {
            wl_surface_set_input_region(state.surface, nullptr);
        } else {
            wl_region *empty = wl_compositor_create_region(state.compositor);
            wl_surface_set_input_region(state.surface, empty);
            wl_region_destroy(empty);
        }
    }
    dock_apply_geometry(state);
    dock_request_frame(state);
}
