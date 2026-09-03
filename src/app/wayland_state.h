#pragma once

#include <EGL/egl.h>
#include <cstring>
#include <memory>
#include <vector>
#include <wayland-client.h>

#include "app/config.h"
#include "app/module.h"
#include "app/service.h"

#include "modules/blink.h"
#include "modules/herald.h"

#include "render/renderer.h"

#include "service/bluetooth_service.h"
#include "service/brightness_service.h"
#include "service/hyprland_service.h"
#include "service/input_service.h"
#include "service/mpris_service.h"
#include "service/network_service.h"
#include "service/notification_service.h"
#include "service/pipewire_service.h"
#include "service/telemetry_service.h"
#include "service/text_input_service.h"
#include "service/tray_service.h"
#include "service/upower_service.h"

#include "ext-session-lock-v1-client-protocol.h"
#include "hyprland-toplevel-export-v1-client-protocol.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct MonitorOutput;

struct WaylandState {
    wl_display *display = nullptr;
    wl_compositor *compositor = nullptr;
    zwlr_layer_shell_v1 *layer_shell = nullptr;
    xdg_wm_base *wm_base = nullptr;
    wl_seat *seat = nullptr;
    wl_shm *shm = nullptr;
    hyprland_toplevel_export_manager_v1 *toplevel_export_manager = nullptr;
    zwp_text_input_manager_v3 *text_input_manager = nullptr;
    ext_session_lock_manager_v1 *session_lock_manager = nullptr;
    TextInputService text_input;
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLConfig egl_config = nullptr;
    EGLContext egl_context = EGL_NO_CONTEXT;
    Config cfg;
    bool running = true;
    bool session_locked = false;
    Renderer renderer;
    BlinkState blink;
    HeraldService herald;
    NotificationService notifications;
    std::vector<std::unique_ptr<Module>> overlays;
    std::vector<std::unique_ptr<Service>> services;
    UpowerState upower;
    NetworkState network;
    BluetoothState bluetooth;
    TrayState tray;
    KeyboardState keyboard;
    PointerState pointer;
    SeatCapabilityState seat_caps;
    BrightnessBackend brightness;
    PipewireState pipewire;
    CpuTempState cpu_temp;
    GpuTempState gpu_temp;
    SystemStatsState system_stats;
    MprisState mpris;
    int brightness_watch_fd = -1;
    int config_watch_fd = -1;
    bool config_own_write_pending = false;
    MonitorOutput *last_pointer_monitor = nullptr;
    bool trulla_enabled = false;
    wl_output *trulla_bound_output = nullptr;
    enum class CompositorBackend { None, Hyprland };
    CompositorBackend compositor_backend = CompositorBackend::None;
    HyprlandState hypr;
    std::vector<std::unique_ptr<MonitorOutput>> outputs;
};

inline Module *find_overlay_by_name(WaylandState &app, const char *name) {
    for (auto &m : app.overlays)
        if (strcmp(m->name(), name) == 0)
            return m.get();
    return nullptr;
}
