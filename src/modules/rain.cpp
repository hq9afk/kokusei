#include <GLES3/gl32.h>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "modules/rain.h"

#include "render/gl.h"
#include "render/node.h"
#include "render/overlay_panel.h"
#include "render/palette.h"

namespace {

void rain_rebuild_active(RainState &state, int width, int height) {
    if (state.mode == RainMode::Matrix)
        state.matrix.rebuild(width, height, state.async_speed);
    else
        state.stiletto.rebuild(width, height, state.async_speed);
}

void rain_tick_active(RainState &state) {
    if (state.mode == RainMode::Matrix)
        state.matrix.tick();
    else
        state.stiletto.tick();
}

bool rain_active_ready(const RainState &state) {
    return state.mode == RainMode::Matrix ? state.matrix.ready()
                                          : state.stiletto.ready();
}

const Texture &rain_active_texture(const RainState &state) {
    return state.mode == RainMode::Matrix ? state.matrix.texture()
                                          : state.stiletto.texture();
}

} // namespace

void rain_request_frame(RainState &state) {
    toplevel_window_request_frame(state.base);
}

void rain_toggle(RainState &state, WaylandState &app) {
    bool opening = !state.base.open;
    if (opening) {
        if (state.base.egl_surface == EGL_NO_SURFACE) {
            if (!toplevel_window_create_surface(
                    state.base, app.compositor, app.wm_base, "Rain",
                    "kokusei-rain", kRainDefaultWindowWidth,
                    kRainDefaultWindowHeight))
                return;
            while (!state.base.configured)
                wl_display_dispatch(app.display);
            if (!toplevel_window_init_egl(state.base, app.egl_display,
                                          app.egl_config, app.egl_context)) {
                toplevel_window_destroy_surface(state.base);
                return;
            }
            state.renderer = &app.renderer;
            state.base.frame_clock.draw = [&state] { rain_paint(state); };
            state.base.on_close_request = [&state, &app] {
                rain_toggle(state, app);
            };
        }
        state.base.open = true;
        rain_rebuild_active(state, state.base.width, state.base.height);
        state.built_width = state.base.width;
        state.built_height = state.base.height;
        state.last_tick = std::chrono::steady_clock::now();
    }

    if (opening) {
        state.base.animations.animate(
            state.base.opacity, 1.0f, kOverlayFadeMs, Easing::EaseOutCubic,
            [&state](float v) { state.base.opacity = v; }, {},
            kOverlayFadeOwner);
        toplevel_window_request_frame(state.base);
    } else {
        state.base.animations.cancelForOwner(kOverlayFadeOwner);
        state.base.open = false;
        toplevel_window_destroy_surface(state.base);
        app_detail::rest_egl_current(app);
    }
}

void rain_handle_key_event(RainState &state, WaylandState &app,
                           const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        rain_toggle(state, app);
}

void rain_apply_params(RainState &state, const RainParams &params) {
    bool changed =
        params.mode != state.mode || params.async_speed != state.async_speed;
    state.mode = params.mode;
    state.async_speed = params.async_speed;
    if (changed && state.base.open) {
        rain_rebuild_active(state, state.base.width, state.base.height);
        state.built_width = state.base.width;
        state.built_height = state.base.height;
        state.last_tick = std::chrono::steady_clock::now();
        rain_request_frame(state);
    }
}

std::vector<IpcHandler> rain_ipc_handlers(RainState &rain,
                                          WaylandState &state) {
    return {
        {"rain", [&rain, &state] { rain_toggle(rain, state); },
         "toggle the rain overlay"},
    };
}

void rain_paint(RainState &state) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);

    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    gl_make_current(state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
    state.renderer->begin_frame(state.base.width, state.base.height,
                                state.base.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    float win_w = static_cast<float>(state.base.width);
    float win_h = static_cast<float>(state.base.height);

    node_add_rect(&state.scene.root, 0.0f, 0.0f, win_w, win_h,
                  rgba(palette::window_backdrop));

    if (state.base.width != state.built_width ||
        state.base.height != state.built_height) {
        rain_rebuild_active(state, state.base.width, state.base.height);
        state.built_width = state.base.width;
        state.built_height = state.base.height;
        state.last_tick = now;
    }

    float elapsed_ms =
        std::chrono::duration<float, std::milli>(now - state.last_tick).count();
    if (elapsed_ms >= kRainFallIntervalMs) {
        rain_tick_active(state);
        state.last_tick = now;
    }

    if (rain_active_ready(state)) {
        Node *tex = state.scene.root.claim_child();
        tex->kind = NodeKind::Texture;
        tex->x = 0.0f;
        tex->y = 0.0f;
        tex->w = win_w;
        tex->h = win_h;
        tex->tex = &rain_active_texture(state);
    }

    state.renderer->set_opacity(state.base.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.open || state.base.animations.hasActive())
        toplevel_window_request_frame(state.base);
}
