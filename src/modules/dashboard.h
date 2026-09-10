#pragma once

#include <EGL/egl.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>

#include "app/ipc.h"

#include "config/dashboard_config.h"

#include "render/animated_image.h"
#include "render/marquee_scroll.h"
#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/brightness_service.h"
#include "service/input_service.h"
#include "service/pipewire_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct WaylandState;

struct DashboardState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;
    bool opened_by_widget = false;
    wl_output *bound_output = nullptr;
    std::optional<DraggedSlider> dragging;
    std::string selected_slider_tag;
    float brightness_level = 0.0f;

    float scroll_offset = 0.0f;
    float content_height = 0.0f;
    float visible_height = 0.0f;

    AnimatedImage profile_pic;
    std::unordered_map<std::string, Texture> art_cache;
    MarqueeTextState media_title_marquee;
    MarqueeTextState media_artist_marquee;

    float pending_bar_height = 0.0f;
    float pending_bar_top_margin = 0.0f;
};

bool dashboard_create_surface(DashboardState &state, wl_compositor *compositor,
                           zwlr_layer_shell_v1 *layer_shell,
                           wl_output *output = nullptr);

bool dashboard_init_egl(DashboardState &state, Renderer &renderer, WaylandState &app,
                     EGLDisplay display, EGLConfig config, EGLContext context);

void dashboard_retarget(DashboardState &state, wl_compositor *compositor,
                     zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                     Renderer &renderer, WaylandState &app,
                     EGLDisplay egl_display, EGLConfig egl_config,
                     EGLContext egl_context, wl_output *target_output,
                     const char *target_name);

void dashboard_request_frame(DashboardState &state, float bar_height,
                          float bar_top_margin);

void dashboard_toggle(DashboardState &state, bool by_widget = false);

std::vector<IpcHandler> dashboard_ipc_handlers(DashboardState &dashboard,
                                            WaylandState &state);

void dashboard_handle_click(DashboardState &state, WaylandState &app, double px,
                         double py);

void dashboard_handle_pointer_move(DashboardState &state, WaylandState &app,
                                double px);

void dashboard_handle_scroll(DashboardState &state, double dy);

void dashboard_handle_key_event(DashboardState &state, WaylandState &app,
                             const KeyEvent &event);

void dashboard_paint(DashboardState &state, WaylandState &app, float bar_height,
                  float bar_top_margin);
