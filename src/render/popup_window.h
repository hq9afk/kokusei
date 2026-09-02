#pragma once

#include <EGL/egl.h>
#include <functional>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "render/rect.h"

#include "service/frame_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct PopupWindowBase {
    wl_compositor *compositor = nullptr;
    wl_surface *surface = nullptr;
    xdg_surface *shell_surface = nullptr;
    xdg_popup *popup = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    bool configured = false, done = false;
    int32_t width = 0, height = 0;
    uint32_t reposition_token = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    std::function<void()> on_done;
};

bool popup_window_create(PopupWindowBase &base, wl_compositor *compositor,
                         xdg_wm_base *wm_base,
                         zwlr_layer_surface_v1 *parent_layer, Rect anchor_rect,
                         int32_t w, int32_t h, wl_seat *seat,
                         uint32_t grab_serial);

bool popup_window_init_egl(PopupWindowBase &base, wl_display *display,
                           EGLDisplay egl_display, EGLConfig config,
                           EGLContext context);

void popup_window_reposition(PopupWindowBase &base, xdg_wm_base *wm_base,
                             Rect anchor_rect, int32_t w, int32_t h);

void popup_window_request_frame(PopupWindowBase &base);

void popup_window_destroy(PopupWindowBase &base);
