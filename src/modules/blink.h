#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "config/blink_config.h"

#include "ext-idle-notify-v1-client-protocol.h"

#include "render/animated_image.h"
#include "render/animation.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture.h"

#include "service/frame_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct BlinkState {
    ext_idle_notifier_v1 *notifier = nullptr;

    ext_idle_notification_v1 *recent_activity_herald = nullptr;
    bool recent_activity_idled = false;
    std::map<std::string, std::chrono::steady_clock::time_point> last_activity;
};

struct BlinkOverlayState {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    bool configured = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;

    AnimationManager animations;
    float ambient_opacity = 0.0f;
    float screensaver_opacity = 0.0f;
    bool ambient_active = false;
    bool screensaver_active = false;

    float logo_x = 40.0f;
    float logo_y = 40.0f;
    float logo_vel_x = kBlinkLogoSpeed;
    float logo_vel_y = kBlinkLogoSpeed * 0.75f;
    std::chrono::steady_clock::time_point last_tick =
        std::chrono::steady_clock::now();

    AnimatedImage logo;

    std::function<void(Node &root, float w, float h)> draw_ambient;
};

bool blink_init(BlinkState &state, wl_seat *seat);

void blink_tick(BlinkState &state, const std::string &focused_monitor);

void blink_reset(BlinkState &state,
                 const std::vector<std::string> &monitor_names);

bool is_blink(const BlinkState &state, const std::string &monitor,
              uint32_t timeout_seconds);

bool blink_overlay_create_surface(BlinkOverlayState &state,
                                  wl_compositor *compositor,
                                  zwlr_layer_shell_v1 *layer_shell,
                                  wl_output *output);

bool blink_overlay_init_egl(BlinkOverlayState &state, Renderer &renderer,
                            EGLDisplay display, EGLConfig config,
                            EGLContext context);

void blink_overlay_request_frame(BlinkOverlayState &state);

void blink_overlay_set_active(BlinkOverlayState &state, bool ambient_active,
                              bool screensaver_active);
