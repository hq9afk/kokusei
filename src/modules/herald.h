#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "config/herald_config.h"

#include "render/animation.h"
#include "render/palette.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture.h"

#include "service/frame_service.h"
#include "service/notification_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct HeraldEntry {
    uint32_t id = 0;
    std::string app_name;
    std::string summary;
    std::string body;
    bool content_built = false;
    Texture app_name_texture;
    Texture summary_texture;
    Texture body_texture;
    uint8_t urgency = 1;
    int32_t timeout_ms = 5000;
    float height = 0.0f;
    float opacity = 0.0f;
    float slide_offset = kHeraldSlideOffset;
    float progress = 1.0f;
    bool exiting = false;
};

struct HeraldService {
    std::vector<HeraldEntry> entries;
    AnimationManager animations;
};

struct HeraldView {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    wl_compositor *compositor = nullptr;
    Renderer *renderer = nullptr;
    bool configured = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    std::vector<std::pair<uint32_t, Rect>> close_hitboxes;
    uint32_t hovered_close_id = 0;
    AnimationManager local_animations;
    std::unordered_map<uint32_t, float> local_exit;
};

float herald_detail_texture_height(const Texture &tex);

const Color &herald_detail_urgency_color(uint8_t urgency);

bool herald_view_create_surface(HeraldView &view, wl_compositor *compositor,
                                zwlr_layer_shell_v1 *layer_shell,
                                wl_output *output = nullptr);

bool herald_view_init_egl(HeraldView &view, HeraldService &service,
                          Renderer &renderer, EGLDisplay display,
                          EGLConfig config, EGLContext context);

void herald_view_request_frame(HeraldView &view);

void herald_sync(HeraldService &service,
                 const NotificationService &notifications);

bool herald_view_handle_close_click(HeraldView &view, double x, double y);

bool herald_view_set_close_hover(HeraldView &view, double x, double y);

bool herald_view_clear_close_hover(HeraldView &view);

void herald_paint(HeraldView &view, HeraldService &service);
