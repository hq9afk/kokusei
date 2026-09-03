#include "render/popup_window.h"
#include "render/gl.h"

namespace {

xdg_positioner *make_positioner(xdg_wm_base *wm_base, Rect anchor_rect,
                                int32_t w, int32_t h) {
    xdg_positioner *p = xdg_wm_base_create_positioner(wm_base);
    xdg_positioner_set_size(p, w, h);
    xdg_positioner_set_anchor_rect(
        p, static_cast<int32_t>(anchor_rect.x),
        static_cast<int32_t>(anchor_rect.y),
        static_cast<int32_t>(anchor_rect.w > 1.0f ? anchor_rect.w : 1.0f),
        static_cast<int32_t>(anchor_rect.h > 1.0f ? anchor_rect.h : 1.0f));
    xdg_positioner_set_anchor(p, XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
    xdg_positioner_set_gravity(p, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_constraint_adjustment(
        p, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    return p;
}

void xdg_surface_configure(void *data, xdg_surface *surface, uint32_t serial) {
    auto *base = static_cast<PopupWindowBase *>(data);
    xdg_surface_ack_configure(surface, serial);
    base->configured = true;
    int32_t scale = base->output_scale.scale;
    if (base->egl_window)
        wl_egl_window_resize(base->egl_window, base->width * scale,
                             base->height * scale, 0, 0);
}

const xdg_surface_listener xdg_surface_listener_impl = {
    .configure = xdg_surface_configure,
};

void popup_configure(void *data, xdg_popup *, int32_t, int32_t, int32_t width,
                     int32_t height) {
    auto *base = static_cast<PopupWindowBase *>(data);
    if (width > 0)
        base->width = width;
    if (height > 0)
        base->height = height;
}

void popup_done(void *data, xdg_popup *) {
    auto *base = static_cast<PopupWindowBase *>(data);
    base->done = true;
    if (base->on_done)
        base->on_done();
}

void popup_repositioned(void *, xdg_popup *, uint32_t) {}

const xdg_popup_listener xdg_popup_listener_impl = {
    .configure = popup_configure,
    .popup_done = popup_done,
    .repositioned = popup_repositioned,
};

} // namespace

bool popup_window_create(PopupWindowBase &base, wl_compositor *compositor,
                         xdg_wm_base *wm_base,
                         zwlr_layer_surface_v1 *parent_layer, Rect anchor_rect,
                         int32_t w, int32_t h, wl_seat *seat,
                         uint32_t grab_serial) {
    base.compositor = compositor;
    base.width = w;
    base.height = h;
    base.done = false;
    base.configured = false;

    base.surface = wl_compositor_create_surface(compositor);
    base.shell_surface = xdg_wm_base_get_xdg_surface(wm_base, base.surface);
    if (!base.shell_surface) {
        wl_surface_destroy(base.surface);
        base.surface = nullptr;
        return false;
    }
    xdg_surface_add_listener(base.shell_surface, &xdg_surface_listener_impl,
                             &base);

    xdg_positioner *positioner = make_positioner(wm_base, anchor_rect, w, h);
    base.popup = xdg_surface_get_popup(base.shell_surface, nullptr, positioner);
    xdg_positioner_destroy(positioner);
    if (!base.popup) {
        xdg_surface_destroy(base.shell_surface);
        wl_surface_destroy(base.surface);
        base.shell_surface = nullptr;
        base.surface = nullptr;
        return false;
    }
    xdg_popup_add_listener(base.popup, &xdg_popup_listener_impl, &base);
    zwlr_layer_surface_v1_get_popup(parent_layer, base.popup);
    if (seat)
        xdg_popup_grab(base.popup, seat, grab_serial);

    xdg_surface_set_window_geometry(base.shell_surface, 0, 0, w, h);

    base.output_scale.on_change = [&base](int32_t scale) {
        if (base.egl_window)
            wl_egl_window_resize(base.egl_window, base.width * scale,
                                 base.height * scale, 0, 0);
        if (base.frame_clock.surface)
            request_frame(base.frame_clock);
    };
    output_scale_watch(base.output_scale, base.surface);
    wl_surface_commit(base.surface);
    return true;
}

bool popup_window_init_egl(PopupWindowBase &base, wl_display *display,
                           EGLDisplay egl_display, EGLConfig config,
                           EGLContext context) {
    while (!base.configured)
        wl_display_dispatch(display);

    base.egl_display = egl_display;
    base.egl_context = context;
    int32_t scale = base.output_scale.scale;
    base.egl_window = wl_egl_window_create(base.surface, base.width * scale,
                                           base.height * scale);
    base.egl_surface = eglCreateWindowSurface(
        egl_display, config,
        reinterpret_cast<EGLNativeWindowType>(base.egl_window), nullptr);
    if (base.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(egl_display, base.egl_surface, context))
        return false;
    base.frame_clock.surface = base.surface;
    return true;
}

void popup_window_reposition(PopupWindowBase &base, xdg_wm_base *wm_base,
                             Rect anchor_rect, int32_t w, int32_t h) {
    if (!base.popup)
        return;
    xdg_positioner *positioner = make_positioner(wm_base, anchor_rect, w, h);
    xdg_popup_reposition(base.popup, positioner, ++base.reposition_token);
    xdg_positioner_destroy(positioner);
    xdg_surface_set_window_geometry(base.shell_surface, 0, 0, w, h);
    base.configured = false;
}

void popup_window_request_frame(PopupWindowBase &base) {
    if (base.egl_surface == EGL_NO_SURFACE || base.done)
        return;
    request_frame(base.frame_clock);
}

void popup_window_destroy(PopupWindowBase &base) {
    if (base.frame_clock.callback) {
        wl_callback_destroy(base.frame_clock.callback);
        base.frame_clock.callback = nullptr;
    }
    base.frame_clock.surface = nullptr;
    base.frame_clock.redraw_requested = false;
    base.frame_clock.mapped = false;

    if (base.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(base.egl_display, base.egl_surface);
        base.egl_surface = EGL_NO_SURFACE;
    }
    if (base.egl_window) {
        wl_egl_window_destroy(base.egl_window);
        base.egl_window = nullptr;
    }
    if (base.popup) {
        xdg_popup_destroy(base.popup);
        base.popup = nullptr;
    }
    if (base.shell_surface) {
        xdg_surface_destroy(base.shell_surface);
        base.shell_surface = nullptr;
    }
    if (base.surface) {
        wl_surface_destroy(base.surface);
        base.surface = nullptr;
    }
    base.configured = false;
    base.done = false;
}
