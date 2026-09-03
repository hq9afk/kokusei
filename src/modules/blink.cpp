#include <GLES2/gl2.h>
#include <filesystem>

#include "core/log.h"

#include "modules/blink.h"

#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"

namespace {

void blink_recent_activity_idled(void *data, ext_idle_notification_v1 *) {
    static_cast<BlinkState *>(data)->recent_activity_idled = true;
}

void blink_recent_activity_resumed(void *data, ext_idle_notification_v1 *) {
    static_cast<BlinkState *>(data)->recent_activity_idled = false;
}

constexpr ext_idle_notification_v1_listener blink_recent_activity_listener = {
    .idled = blink_recent_activity_idled,
    .resumed = blink_recent_activity_resumed,
};

} // namespace

bool blink_init(BlinkState &state, wl_seat *seat) {
    if (!state.notifier || !seat) {
        klog("blink: compositor is missing ext_idle_notifier_v1 or wl_seat, "
             "skipping");
        return false;
    }
    state.recent_activity_herald = ext_idle_notifier_v1_get_idle_notification(
        state.notifier, kBlinkRecentActivityPulseSeconds * 1000, seat);
    if (state.recent_activity_herald)
        ext_idle_notification_v1_add_listener(state.recent_activity_herald,
                                              &blink_recent_activity_listener,
                                              &state);
    else
        klog("blink: recent-activity notification failed, ambient/screensaver "
             "clock disabled");
    return state.recent_activity_herald != nullptr;
}

void blink_reset(BlinkState &state,
                 const std::vector<std::string> &monitor_names) {
    auto now = std::chrono::steady_clock::now();
    for (const auto &name : monitor_names)
        state.last_activity[name] = now;
}

void blink_tick(BlinkState &state, const std::string &focused_monitor) {
    if (!state.recent_activity_idled && !focused_monitor.empty())
        state.last_activity[focused_monitor] = std::chrono::steady_clock::now();
}

bool is_blink(const BlinkState &state, const std::string &monitor,
              uint32_t timeout_seconds) {
    auto it = state.last_activity.find(monitor);
    if (it == state.last_activity.end())
        return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - it->second)
                       .count();
    return elapsed > static_cast<int64_t>(timeout_seconds);
}

namespace {

void blink_overlay_layer_surface_configure(void *data,
                                           zwlr_layer_surface_v1 *layer_surface,
                                           uint32_t serial, uint32_t width,
                                           uint32_t height) {
    auto *state = static_cast<BlinkOverlayState *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    bool changed = state->width != static_cast<int32_t>(width) ||
                   state->height != static_cast<int32_t>(height);
    state->width = static_cast<int32_t>(width);
    state->height = static_cast<int32_t>(height);
    if (changed && state->egl_window) {
        int32_t scale = state->output_scale.scale;
        wl_egl_window_resize(state->egl_window, state->width * scale,
                             state->height * scale, 0, 0);
        if (state->frame_clock.surface)
            request_frame(state->frame_clock);
    }
    state->configured = true;
}

void blink_overlay_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener blink_overlay_layer_surface_listener =
    {
        .configure = blink_overlay_layer_surface_configure,
        .closed = blink_overlay_layer_surface_closed,
};

void blink_overlay_bounce(BlinkOverlayState &state, float dt) {
    if (dt <= 0.0f || dt > 0.5f || state.logo.frames.empty())
        return;
    float logo_w = kBlinkLogoSize;
    float logo_h = kBlinkLogoSize;
    state.logo_x += state.logo_vel_x * dt;
    state.logo_y += state.logo_vel_y * dt;
    float max_x = static_cast<float>(state.width) - logo_w;
    float max_y = static_cast<float>(state.height) - logo_h;
    if (state.logo_x < 0.0f) {
        state.logo_x = 0.0f;
        state.logo_vel_x = -state.logo_vel_x;
    } else if (state.logo_x > max_x) {
        state.logo_x = max_x;
        state.logo_vel_x = -state.logo_vel_x;
    }
    if (state.logo_y < 0.0f) {
        state.logo_y = 0.0f;
        state.logo_vel_y = -state.logo_vel_y;
    } else if (state.logo_y > max_y) {
        state.logo_y = max_y;
        state.logo_vel_y = -state.logo_vel_y;
    }
}

void blink_overlay_paint(BlinkOverlayState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;

    gl_make_current(state.egl_display, state.egl_surface, state.egl_context);
    state.renderer->begin_frame(state.width, state.height,
                                state.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    auto now = std::chrono::steady_clock::now();
    state.animations.tick(now);
    float dt = std::chrono::duration<float>(now - state.last_tick).count();
    state.last_tick = now;
    if (state.screensaver_active) {
        animated_image_tick(state.logo, now);
        blink_overlay_bounce(state, dt);
    } else if (state.screensaver_opacity <= 0.0f) {
        animated_image_hide(state.logo);
    }

    if (state.ambient_opacity > 0.0f && state.draw_ambient) {
        state.scene.rebuild();
        state.draw_ambient(state.scene.root, static_cast<float>(state.width),
                           static_cast<float>(state.height));
        state.renderer->set_opacity(state.ambient_opacity);
        state.scene.draw(*state.renderer);
    }

    if (state.screensaver_opacity > 0.0f) {
        state.scene.rebuild();
        Color black{0.0f, 0.0f, 0.0f, state.screensaver_opacity};
        node_add_rect(&state.scene.root, 0, 0, static_cast<float>(state.width),
                      static_cast<float>(state.height), rgba(black));
        animated_image_draw(state.logo, &state.scene.root, state.logo_x,
                            state.logo_y, kBlinkLogoSize, kBlinkLogoSize,
                            state.screensaver_opacity);
        state.renderer->set_opacity(1.0f);
        state.scene.draw(*state.renderer);
    }

    eglSwapBuffers(state.egl_display, state.egl_surface);

    if (state.animations.hasActive() || state.screensaver_active)
        request_frame(state.frame_clock);
}

} // namespace

bool blink_overlay_create_surface(BlinkOverlayState &state,
                                  wl_compositor *compositor,
                                  zwlr_layer_shell_v1 *layer_shell,
                                  wl_output *output) {
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        .name_space = kBlinkOverlayLayerNamespace,
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        .empty_input_region = true,
    };
    state.layer_surface = layer_surface_create(
        state.surface, compositor, layer_shell, cfg,
        &blink_overlay_layer_surface_listener, &state, output);
    if (!state.layer_surface)
        return false;
    state.output_scale.on_change = [&state](int32_t scale) {
        if (state.egl_window)
            wl_egl_window_resize(state.egl_window, state.width * scale,
                                 state.height * scale, 0, 0);
        if (state.frame_clock.surface)
            request_frame(state.frame_clock);
    };
    output_scale_watch(state.output_scale, state.surface);
    wl_surface_commit(state.surface);
    return true;
}

bool blink_overlay_init_egl(BlinkOverlayState &state, Renderer &renderer,
                            EGLDisplay display, EGLConfig config,
                            EGLContext context) {
    state.egl_display = display;
    state.egl_context = context;
    state.renderer = &renderer;
    int32_t scale = state.output_scale.scale;
    state.egl_window = wl_egl_window_create(state.surface, state.width * scale,
                                            state.height * scale);
    state.egl_surface = eglCreateWindowSurface(
        display, config,
        reinterpret_cast<EGLNativeWindowType>(state.egl_window), nullptr);
    if (state.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, state.egl_surface, context))
        return false;
    state.frame_clock.surface = state.surface;
    state.frame_clock.draw = [&state] { blink_overlay_paint(state); };

    const char *logo_candidates[] = {KOKUSEI_IDLE_LOGO,
                                     "assets/default_wp.svg"};
    std::string logo_path = logo_candidates[1];
    for (const char *candidate : logo_candidates) {
        if (std::filesystem::exists(candidate)) {
            logo_path = candidate;
            break;
        }
    }
    AnimatedImageStyle logo_style;
    logo_style.size = kBlinkLogoSize;
    logo_style.decode = {30, static_cast<int>(kBlinkLogoSize)};
    animated_image_set_source(state.logo, logo_path, logo_style);
    return true;
}

void blink_overlay_request_frame(BlinkOverlayState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(state.frame_clock);
}

void blink_overlay_set_active(BlinkOverlayState &state, bool ambient_active,
                              bool screensaver_active) {
    bool changed = false;
    bool ambient_shown = ambient_active && !screensaver_active;
    if (ambient_shown != state.ambient_active) {
        state.ambient_active = ambient_shown;
        state.animations.animate(
            state.ambient_opacity, ambient_shown ? 1.0f : 0.0f,
            kBlinkOverlayFadeMs, Easing::EaseOutCubic,
            [&state](float v) { state.ambient_opacity = v; }, {},
            kBlinkAmbientFadeOwner);
        changed = true;
    }
    if (screensaver_active != state.screensaver_active) {
        state.screensaver_active = screensaver_active;
        if (screensaver_active)
            animated_image_show(
                state.logo, [&state] { blink_overlay_request_frame(state); });
        state.animations.animate(
            state.screensaver_opacity, screensaver_active ? 1.0f : 0.0f,
            kBlinkOverlayFadeMs, Easing::EaseOutCubic,
            [&state](float v) { state.screensaver_opacity = v; }, {},
            kBlinkScreensaverFadeOwner);
        changed = true;
    }
    if (changed)
        blink_overlay_request_frame(state);
}
