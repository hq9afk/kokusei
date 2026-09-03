#include <GLES3/gl32.h>
#include <algorithm>
#include <cstring>

#include "app/module_registry.h"
#include "app/monitor_output.h"
#include "app/wayland_registry.h"
#include "app/wayland_state.h"

#include "core/log.h"

#include "modules/qixing.h"

namespace {

void layer_surface_configure(void *data, zwlr_layer_surface_v1 *layer_surface,
                             uint32_t serial, uint32_t width, uint32_t) {
    auto *mon = static_cast<MonitorOutput *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    mon->width = static_cast<int32_t>(width);
    if (mon->egl_window) {
        int32_t scale = mon->output_scale.scale;
        wl_egl_window_resize(mon->egl_window, mon->width * scale,
                             qixing_detail::qixing_current_height(*mon) * scale,
                             0, 0);
    }
    mon->configured = true;
}

void layer_surface_closed(void *data, zwlr_layer_surface_v1 *) {
    static_cast<MonitorOutput *>(data)->app->running = false;
}

namespace output_detail {
void geometry(void *, wl_output *, int32_t, int32_t, int32_t, int32_t, int32_t,
              const char *, const char *, int32_t) {}
void mode(void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {}
void scale_event(void *data, wl_output *, int32_t factor) {
    static_cast<MonitorOutput *>(data)->output.scale = factor;
}
void name_event(void *data, wl_output *, const char *name) {
    static_cast<MonitorOutput *>(data)->output.name = name;
}
void description(void *, wl_output *, const char *) {}
void done(void *data, wl_output *) {
    auto *mon = static_cast<MonitorOutput *>(data);
    bool first_done = !mon->output.done;
    mon->output.done = true;
    if (!mon->activated && mon->app->egl_context != EGL_NO_CONTEXT)
        monitor_output_activate(*mon->app, *mon);
    if (first_done)
        penance_notify_output_added(*mon->app, mon->output.wl,
                                    mon->output.name.c_str());
}

const wl_output_listener &listener() {
    static constexpr wl_output_listener l{
        .geometry = geometry,
        .mode = mode,
        .done = done,
        .scale = scale_event,
        .name = name_event,
        .description = description,
    };
    return l;
}
} // namespace output_detail

namespace xdg_wm_base_listener_detail {
void ping(void *, xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}
constexpr xdg_wm_base_listener listener{.ping = ping};
} // namespace xdg_wm_base_listener_detail

void registry_global(void *data, wl_registry *registry, uint32_t name,
                     const char *interface, uint32_t version) {
    auto *state = static_cast<WaylandState *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = static_cast<wl_compositor *>(wl_registry_bind(
            registry, name, &wl_compositor_interface, std::min(version, 6u)));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell =
            static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(
                registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(
            registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
        xdg_wm_base_add_listener(
            state->wm_base, &xdg_wm_base_listener_detail::listener, nullptr);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        auto mon = std::make_unique<MonitorOutput>();
        mon->app = state;
        mon->output.registry_name = name;
        mon->output.wl = static_cast<wl_output *>(wl_registry_bind(
            registry, name, &wl_output_interface, std::min(version, 4u)));
        wl_output_add_listener(mon->output.wl, &output_detail::listener(),
                               mon.get());
        state->outputs.push_back(std::move(mon));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, 3));
        state->seat_caps.keyboard = &state->keyboard;
        state->seat_caps.pointer = &state->pointer;
        keyboard_attach_seat(state->seat_caps, state->seat);
    } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
        state->blink.notifier =
            static_cast<ext_idle_notifier_v1 *>(wl_registry_bind(
                registry, name, &ext_idle_notifier_v1_interface, 1));
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) ==
               0) {
        state->pointer.cursor_shape_manager =
            static_cast<wp_cursor_shape_manager_v1 *>(wl_registry_bind(
                registry, name, &wp_cursor_shape_manager_v1_interface, 1));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = static_cast<wl_shm *>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface,
                      hyprland_toplevel_export_manager_v1_interface.name) ==
               0) {
        state->toplevel_export_manager =
            static_cast<hyprland_toplevel_export_manager_v1 *>(wl_registry_bind(
                registry, name, &hyprland_toplevel_export_manager_v1_interface,
                1));
    } else if (strcmp(interface, zwp_text_input_manager_v3_interface.name) ==
               0) {
        state->text_input_manager =
            static_cast<zwp_text_input_manager_v3 *>(wl_registry_bind(
                registry, name, &zwp_text_input_manager_v3_interface, 1));
    } else if (strcmp(interface, ext_session_lock_manager_v1_interface.name) ==
               0) {
        state->session_lock_manager =
            static_cast<ext_session_lock_manager_v1 *>(wl_registry_bind(
                registry, name, &ext_session_lock_manager_v1_interface, 1));
    }
}

void registry_global_remove(void *data, wl_registry *, uint32_t name) {
    auto *state = static_cast<WaylandState *>(data);
    auto it = std::find_if(state->outputs.begin(), state->outputs.end(),
                           [name](const std::unique_ptr<MonitorOutput> &m) {
                               return m->output.registry_name == name;
                           });
    if (it == state->outputs.end())
        return;
    klog("output: '%s' removed", (*it)->output.name.c_str());
    penance_notify_output_removed(*state, (*it)->output.wl);
    if (state->last_pointer_monitor == it->get())
        state->last_pointer_monitor = nullptr;
    monitor_output_destroy(**it);
    state->outputs.erase(it);
}

} // namespace

const wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

const zwlr_layer_surface_v1_listener qixing_layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

bool bootstrap_egl(WaylandState &state) {
    state.egl_display =
        eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(state.display));
    if (state.egl_display == EGL_NO_DISPLAY)
        return false;
    if (!eglInitialize(state.egl_display, nullptr, nullptr))
        return false;
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };
    EGLint num_configs = 0;
    if (!eglChooseConfig(state.egl_display, config_attribs, &state.egl_config,
                         1, &num_configs) ||
        num_configs == 0) {
        return false;
    }

    const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                      EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    state.egl_context = eglCreateContext(state.egl_display, state.egl_config,
                                         EGL_NO_CONTEXT, context_attribs);
    if (state.egl_context == EGL_NO_CONTEXT) {
        klog("egl: OpenGL ES 3.2 context creation failed, egl error 0x%04x",
             eglGetError());
        return false;
    }
    return true;
}

bool renderer_bootstrap_init(WaylandState &state) {
    const EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface pbuffer = eglCreatePbufferSurface(
        state.egl_display, state.egl_config, pbuffer_attribs);
    if (pbuffer == EGL_NO_SURFACE)
        return false;
    if (!eglMakeCurrent(state.egl_display, pbuffer, pbuffer,
                        state.egl_context)) {
        eglDestroySurface(state.egl_display, pbuffer);
        return false;
    }
    klog("gl: %s | GLSL %s",
         reinterpret_cast<const char *>(glGetString(GL_VERSION)),
         reinterpret_cast<const char *>(
             glGetString(GL_SHADING_LANGUAGE_VERSION)));
    bool ok = state.renderer.init();
    eglDestroySurface(state.egl_display, pbuffer);
    return ok;
}
