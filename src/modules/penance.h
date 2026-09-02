#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "config/penance_config.h"

#include "render/animated_image.h"
#include "render/animation.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/text_field.h"
#include "render/texture.h"
#include "render/texture_cache.h"

#include "service/frame_service.h"
#include "service/input_service.h"
#include "service/output_service.h"

#include "ext-session-lock-v1-client-protocol.h"

struct WaylandState;
struct PenanceState;

struct PenanceOutputSurface {
    PenanceState *owner = nullptr;
    wl_output *output = nullptr;
    std::string output_name;
    wl_surface *surface = nullptr;
    ext_session_lock_surface_v1 *penance_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    int32_t width = 0;
    int32_t height = 0;
    bool configured = false;
    bool panel_gated = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    AnimationManager animations;

    float panel_scale = kPenanceScaleHidden;
    float panel_rotation = 0.0f;
    float panel_w = 0.0f;
    float panel_h = 0.0f;
    float panel_w_target = 0.0f;
    float panel_h_target = 0.0f;
    float icon_alpha = 1.0f;
    float content_alpha = 0.0f;
    float content_scale = kPenanceScaleHidden;
    bool anim_started = false;

    Rect media_prev{};
    Rect media_play{};
    Rect media_next{};
    Rect pill_button{};

    PenanceOutputSurface() = default;
    PenanceOutputSurface(const PenanceOutputSurface &) = delete;
    PenanceOutputSurface &operator=(const PenanceOutputSurface &) = delete;
};

struct PenanceState {
    WaylandState *app = nullptr;
    ext_session_lock_v1 *penance = nullptr;
    bool active = false;
    bool locked = false;
    bool unlocking = false;
    std::chrono::steady_clock::time_point locked_at{};

    std::vector<std::unique_ptr<PenanceOutputSurface>> surfaces;

    TextFieldState password;
    bool failed = false;
    std::chrono::steady_clock::time_point fail_clear_at{};
    bool authenticating = false;
    uint64_t auth_generation = 0;

    std::string user;
    Texture echo_glyph;
    TextureCache tcache;
    std::unordered_map<std::string, Texture> art_cache;
    TextFieldTypeAnim pw_anim;
    TextFieldRowSlide pw_row_slide;

    AnimatedImage avatar;

    std::function<void(const std::string &output_name, Node &root, int32_t w,
                       int32_t h)>
        draw_expanse;
    std::function<bool(const std::string &output_name)> panel_gated_for;
};

bool penance_request(PenanceState &st, WaylandState &app);
void penance_teardown(PenanceState &st);
void penance_begin_unlock(PenanceState &st);
void penance_handle_key(PenanceState &st, const KeyEvent &ev);
void penance_handle_click(PenanceState &st, wl_surface *surf, double x,
                          double y);
void penance_timer_tick(PenanceState &st);
void penance_hotplug_add(PenanceState &st, wl_output *output, const char *name);
void penance_hotplug_remove(PenanceState &st, wl_output *output);
wl_surface *penance_focused_surface(const PenanceState &st);
bool penance_owns_surface(const PenanceState &st, wl_surface *s);
