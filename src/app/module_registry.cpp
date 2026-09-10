#include <chrono>
#include <filesystem>

#include "app/module_registry.h"
#include "app/monitor_output.h"
#include "app/text_input_client.h"
#include "app/wayland_state.h"

#include "config/bar_config.h"

#include "modules/wallpaper.h"
#include "modules/notification.h"
#include "modules/overview.h"
#include "modules/launcher.h"
#include "modules/lock.h"
#include "modules/bar.h"
#include "modules/rain.h"
#include "modules/visualizer.h"
#include "modules/osd.h"
#include "modules/logout.h"
#include "modules/settings.h"
#include "modules/dashboard.h"

#include "render/animated_image.h"
#include "render/image.h"
#include "render/layer_surface.h"
#include "render/palette.h"

#include "service/mpris_service.h"
#include "service/telemetry_service.h"

namespace {

class LauncherModule final : public Module, public TextInputClient {
  public:
    const char *name() const override { return "launcher"; }
    bool is_open() const override { return state_.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = launcher_create_surface(state_, app.compositor, app.layer_shell,
                                        output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!launcher_init_egl(state_, app.renderer, app.egl_display,
                               app.egl_config, app.egl_context))
            return false;
        state_.bound_output = output_;
        state_.sync_text_input_focus = [this, &app](bool focused) {
            if (focused)
                app.text_input.set_focused_client(state_.surface, this);
            else
                app.text_input.clear_focused_client(this);
        };
        request_frame();
        return true;
    }

    TextInputState text_input_state() const override {
        return launcher_text_input_state(state_);
    }
    void text_input_apply_edit(const TextInputEdit &edit) override {
        launcher_text_input_apply_edit(state_, edit);
        request_frame();
    }
    void text_input_reset_preedit() override {
        state_.search.preedit.clear();
        request_frame();
    }
    void text_input_activated(TextInputService &) override {}
    void text_input_deactivated(TextInputService &) override {
        state_.search.preedit.clear();
    }

    bool configured() const override { return !want_ || state_.configured; }
    wl_surface *surface() const override { return state_.surface; }
    void request_frame() override { launcher_request_frame(state_); }

    bool tick() override {
        launcher_search_start_pending(state_);
        return launcher_tick(state_);
    }
    int poll_timeout_ms() const override {
        return launcher_poll_timeout_ms(state_);
    }
    bool timer_tick(WaylandState &) override {
        if (!state_.open)
            return false;
        text_field_idle_toggle(state_.search);
        request_frame();
        return true;
    }

    void handle_click(WaylandState &, double x, double y) override {
        launcher_handle_click(state_, x, y);
    }
    void handle_pointer_move(WaylandState &, wl_surface *focused_surface,
                             double x, double y) override {
        launcher_handle_pointer_move(state_, focused_surface, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return state_.open && state_.hovered_index >= 0;
    }
    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        launcher_handle_key_event(state_, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        auto toggle_retargeted = [this, &app](bool global) {
            if (!state_.open) {
                MonitorOutput *target = app_detail::active_target_monitor(app);
                if (target && (target->output.wl != state_.bound_output ||
                               !state_.layer_surface))
                    launcher_retarget(state_, app.compositor, app.layer_shell,
                                      app.display, app.renderer,
                                      app.egl_display, app.egl_config,
                                      app.egl_context, target->output.wl,
                                      target->output.name.c_str());
            }
            launcher_toggle(state_, global);
        };
        return {
            {"launcher", [toggle_retargeted] { toggle_retargeted(false); },
             "toggle the launcher, searching from $HOME"},
            {"launcher global",
             [toggle_retargeted] { toggle_retargeted(true); },
             "toggle the launcher, searching from /"},
        };
    }

    std::vector<std::pair<int, std::function<void()>>>
    extra_poll_sources(WaylandState &app) override {
        std::vector<std::pair<int, std::function<void()>>> sources;
        auto dispatch = [this, &app] {
            if (launcher_search_poll(state_)) {
                request_frame();
                app_detail::rest_egl_current(app);
            }
        };
        if (state_.search_dirs_proc.wake_fd >= 0)
            sources.push_back({state_.search_dirs_proc.wake_fd, dispatch});
        if (state_.search_files_proc.wake_fd >= 0)
            sources.push_back({state_.search_files_proc.wake_fd, dispatch});
        return sources;
    }

  private:
    LauncherState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
};

class LogoutModule final : public Module {
  public:
    const char *name() const override { return "logout"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = logout_create_surface(state_, app.compositor, app.layer_shell,
                                        output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!logout_init_egl(state_, app.renderer, app.egl_display,
                               app.egl_config, app.egl_context))
            return false;
        state_.bound_output = output_;
        request_frame();

        logout_apply_logo_config(state_, app.cfg.logout_animated_logo);
        return true;
    }

    bool configured() const override {
        return !want_ || state_.base.configured;
    }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { logout_request_frame(state_); }

    bool timer_tick(WaylandState &) override { return false; }

    void handle_pointer_move(WaylandState &, wl_surface *focused_surface,
                             double x, double y) override {
        if (!state_.base.open)
            return;
        if (focused_surface == state_.base.surface)
            logout_handle_hover(state_, x, y);
        else
            logout_clear_hover(state_);
        request_frame();
    }

    void handle_click(WaylandState &, double x, double y) override {
        logout_handle_click(state_, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return state_.base.open && state_.hovered_index >= 0;
    }
    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        logout_handle_key_event(state_, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return logout_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    wl_output *bound_output() const override { return state_.bound_output; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                logout_retarget(state_, app.compositor, app.layer_shell,
                                  app.display, app.renderer, app.egl_display,
                                  app.egl_config, app.egl_context,
                                  target->output.wl,
                                  target->output.name.c_str());
        }
        logout_apply_logo_config(state_, app.cfg.logout_animated_logo);
        logout_toggle(state_, true);
    }

  private:
    LogoutState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
};

class DashboardModule final : public Module {
  public:
    const char *name() const override { return "dashboard"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = dashboard_create_surface(state_, app.compositor, app.layer_shell,
                                      output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!dashboard_init_egl(state_, app.renderer, app, app.egl_display,
                             app.egl_config, app.egl_context))
            return false;
        state_.bound_output = output_;
        request_frame();
        return true;
    }

    bool configured() const override {
        return !want_ || state_.base.configured;
    }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override {
        dashboard_request_frame(
            state_, static_cast<float>(bar_detail::kBarHeight),
            static_cast<float>(bar_detail::kBarTopMargin));
    }

    bool timer_tick(WaylandState &app) override {
        if (!state_.base.open)
            return false;
        ++poll_tick_;
        if (poll_tick_ % 2 == 0) {
            cpu_temp_poll(app.cpu_temp);
            system_stats_poll(app.system_stats);
        }
        if (poll_tick_ % 5 == 0 || app.gpu_temp.nvidia_smi_running)
            gpu_temp_poll(app.gpu_temp);
        mpris_poll_position(app.mpris);
        request_frame();
        return true;
    }

    void handle_pointer_move(WaylandState &app, wl_surface *, double x,
                             double y) override {
        hovering_clickable_ =
            state_.base.open &&
            app.pointer.focused_surface == state_.base.surface &&
            panel_region_hit(state_.click_regions, x, y);
        if (!state_.dragging)
            return;
        dashboard_handle_pointer_move(state_, app, x);
        request_frame();
    }
    bool wants_pointing_hand_cursor() const override {
        return hovering_clickable_;
    }

    void handle_pointer_release() override {
        if (state_.dragging)
            state_.dragging.reset();
    }

    void handle_click(WaylandState &app, double x, double y) override {
        dashboard_handle_click(state_, app, x, y);
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        dashboard_handle_key_event(state_, app, event);
    }
    void handle_scroll(WaylandState &, double dy) override {
        dashboard_handle_scroll(state_, dy);
        request_frame();
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return dashboard_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    wl_output *bound_output() const override { return state_.bound_output; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                dashboard_retarget(state_, app.compositor, app.layer_shell,
                                app.display, app.renderer, app, app.egl_display,
                                app.egl_config, app.egl_context,
                                target->output.wl, target->output.name.c_str());
            cpu_temp_poll(app.cpu_temp);
            system_stats_poll(app.system_stats);
            gpu_temp_poll(app.gpu_temp);
        }
        dashboard_toggle(state_, true);
    }

  private:
    DashboardState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
    int poll_tick_ = 0;
};

class OverviewModule final : public Module {
  public:
    const char *name() const override { return "overview"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = overview_create_surface(state_, app.compositor, app.layer_shell,
                                     output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!overview_init_egl(state_, app.renderer, app.egl_display,
                            app.egl_config, app.egl_context))
            return false;
        state_.bound_output = output_;
        state_.app_ptr = &app;
        return true;
    }

    bool configured() const override {
        return !want_ || state_.base.configured;
    }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { overview_request_frame(state_); }

    int poll_timeout_ms() const override {
        return state_.base.open ? kOverviewCaptureIntervalMs : -1;
    }

    bool tick() override {
        if (!state_.base.open)
            return false;
        auto now = std::chrono::steady_clock::now();
        if (now - last_capture_arm_ <
            std::chrono::milliseconds(kOverviewCaptureIntervalMs))
            return false;
        last_capture_arm_ = now;
        return true;
    }

    void handle_pointer_move(WaylandState &app, wl_surface *, double x,
                             double y) override {
        overview_handle_pointer_move(state_, app, x, y);
        hovering_clickable_ =
            app.pointer.focused_surface == state_.base.surface &&
            overview_point_is_clickable(state_, app, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return hovering_clickable_;
    }
    void handle_pointer_release() override {
        if (state_.app_ptr)
            overview_handle_pointer_release(state_, *state_.app_ptr);
    }

    void handle_click(WaylandState &app, double x, double y) override {
        overview_handle_click(state_, app, x, y);
        request_frame();
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        overview_handle_key_event(state_, app, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return overview_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                overview_retarget(state_, app.compositor, app.layer_shell,
                               app.display, app.renderer, app.egl_display,
                               app.egl_config, app.egl_context,
                               target->output.wl, target->output.name.c_str());
        }
        overview_toggle(state_, app, true);
    }

  private:
    OverviewState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
    std::chrono::steady_clock::time_point last_capture_arm_{};
};

class SettingsModule final : public Module, public TextInputClient {
  public:
    const char *name() const override { return "settings"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = settings_create_surface(state_, app.compositor, app.layer_shell,
                                      output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        SettingsEnv env = settings_env(app);
        if (!settings_init_egl(state_, app.cfg, app.renderer, app.egl_display,
                             app.egl_config, app.egl_context,
                             env.monitor_names_fn, env.focused_monitor_fn,
                             env.decode_status_fn))
            return false;
        app.settings_bound_output = output_;
        app.settings_enabled = true;
        state_.sync_text_input_focus = [this, &app](bool focused) {
            if (focused)
                app.text_input.set_focused_client(state_.base.surface, this);
            else
                app.text_input.clear_focused_client(this);
        };
        return true;
    }

    TextInputState text_input_state() const override {
        return settings_text_input_state(state_);
    }
    void text_input_apply_edit(const TextInputEdit &edit) override {
        settings_text_input_apply_edit(state_, edit);
        request_frame();
    }
    void text_input_reset_preedit() override {
        state_.field_buffer.preedit.clear();
        request_frame();
    }
    void text_input_activated(TextInputService &) override {}
    void text_input_deactivated(TextInputService &) override {
        state_.field_buffer.preedit.clear();
    }

    bool configured() const override {
        return !want_ || state_.base.configured;
    }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { settings_request_frame(state_); }

    bool timer_tick(WaylandState &) override {
        if (state_.focused_field == SettingsFieldId::None)
            return false;
        text_field_idle_toggle(state_.field_buffer);
        request_frame();
        return true;
    }

    void handle_click(WaylandState &app, double x, double y) override {
        settings_handle_click(
            state_, app.cfg,
            [&app](Config c) {
                app_detail::save_and_apply_config_update(app, c);
            },
            x, y);
    }
    void handle_pointer_move(WaylandState &, wl_surface *focused_surface,
                             double x, double y) override {
        hovering_clickable_ = state_.base.open &&
                              focused_surface == state_.base.surface &&
                              settings_point_is_clickable(state_, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return hovering_clickable_;
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        settings_handle_key_event(
            state_, app.cfg,
            [&app](Config c) {
                app_detail::save_and_apply_config_update(app, c);
            },
            event);
    }
    void handle_scroll(WaylandState &, double dy) override {
        settings_handle_scroll(state_, dy);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return settings_ipc_handlers(state_, app);
    }

  private:
    SettingsState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
};

class RainModule final : public Module {
  public:
    const char *name() const override { return "rain"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }
    bool init_egl(WaylandState &app) override {
        rain_apply_params(state_, app.cfg.rain);
        return true;
    }
    bool configured() const override { return true; }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { rain_request_frame(state_); }

    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        rain_handle_key_event(state_, app, event);
    }

    void apply_config(WaylandState &, const Config &cfg) override {
        rain_apply_params(state_, cfg.rain);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return rain_ipc_handlers(state_, app);
    }

  private:
    RainState state_;
};

class VisualizerModule final : public Module {
  public:
    ~VisualizerModule() override { visualizer_shutdown(state_); }

    const char *name() const override { return "visualizer"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }
    bool init_egl(WaylandState &) override { return true; }
    bool configured() const override { return true; }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override {}

    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        visualizer_handle_key_event(state_, app, event);
    }

    void apply_config(WaylandState &, const Config &cfg) override {
        visualizer_apply_params(state_, cfg.visualizer);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return visualizer_ipc_handlers(state_, app);
    }

  private:
    VisualizerState state_;
};

class LockModule final : public Module {
  public:
    LockState &state() { return state_; }

    const char *name() const override { return "lock"; }
    bool is_open() const override { return state_.active; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }

    bool init_egl(WaylandState &app) override {
        state_.app = &app;
        state_.draw_wallpaper = [&app](const std::string &output_name, Node &root,
                                     int32_t w, int32_t h) {
            for (auto &mon : app.outputs) {
                if (mon->output.name != output_name)
                    continue;
                if (auto *wp = mon->module<WallpaperPerMonitorModule>())
                    wallpaper_draw_columns(wp->wallpaper_state(), &root, w, h);
                return;
            }
        };
        state_.panel_gated_for = [&app](const std::string &output_name) {
            return lock_effective_enabled(app.cfg, output_name);
        };
        const char *echo_candidates[] = {KOKUSEI_INPUT_ECHO,
                                         "assets/electro.png"};
        for (const char *c : echo_candidates) {
            if (std::filesystem::exists(c)) {
                state_.echo_glyph = load_image_texture(c);
                break;
            }
        }
        return true;
    }

    bool configured() const override { return true; }
    wl_surface *surface() const override {
        return lock_focused_surface(state_);
    }
    bool owns_surface(wl_surface *s) const override {
        return lock_owns_surface(state_, s);
    }
    void request_frame() override {}

    bool timer_tick(WaylandState &app) override {
        if (!state_.active)
            return false;
        ++poll_tick_;
        cpu_temp_poll(app.cpu_temp);
        system_stats_poll(app.system_stats);
        if (poll_tick_ % 5 == 0 || app.gpu_temp.nvidia_smi_running)
            gpu_temp_poll(app.gpu_temp);
        lock_timer_tick(state_);
        return true;
    }

    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        lock_handle_key(state_, event);
    }
    void handle_click(WaylandState &app, double x, double y) override {
        lock_handle_click(state_, app.pointer.focused_surface, x, y);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return {{"lock",
                 [this, &app] {
                     cpu_temp_poll(app.cpu_temp);
                     system_stats_poll(app.system_stats);
                     gpu_temp_poll(app.gpu_temp);
                     lock_request(state_, app);
                 },
                 "lock the session"}};
    }

  private:
    LockState state_;
    int poll_tick_ = 0;
};

void wallpaper_sync_active_mode(WallpaperState &wp, const Config &cfg,
                              const std::string &monitor_name) {
    wallpaper_sync_from_config(wp, cfg, monitor_name,
                             cfg.wallpaper_animated_enabled);
}

} // namespace

SettingsEnv settings_env(WaylandState &app) {
    return {
        [&app] {
            std::vector<std::string> names;
            for (const auto &mon : app.outputs)
                names.push_back(mon->output.name);
            return names;
        },
        [&app] {
            return app.compositor_backend ==
                           WaylandState::CompositorBackend::Hyprland
                       ? app.hypr.focused_monitor
                       : std::string();
        },
        [&app](const std::string &name, int column) -> MediaDecodeStatus {
            for (auto &mon : app.outputs) {
                if (mon->output.name != name)
                    continue;
                if (auto *wp = mon->module<WallpaperPerMonitorModule>())
                    return wp->decode_status(column);
            }
            return MediaDecodeStatus::Idle;
        },
    };
}

bool OsdPerMonitorModule::create_surface(WaylandState &app,
                                           MonitorOutput &mon,
                                           wl_output *output) {
    if (!osd_create_surface(state_, app.compositor, app.layer_shell, output))
        klog("osd: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool OsdPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool OsdPerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (state_.layer_surface &&
        osd_init_egl(state_, app.renderer, app.egl_display, app.egl_config,
                       app.egl_context))
        eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                       app.egl_context);
    return true;
}

void OsdPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool OsdPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void OsdPerMonitorModule::tick(WaylandState &, MonitorOutput &) {
    if (state_.visible && std::chrono::steady_clock::now() >= state_.hide_at)
        osd_hide(state_);
}

bool WallpaperPerMonitorModule::create_surface(WaylandState &app,
                                             MonitorOutput &mon,
                                             wl_output *output) {
    if (!wallpaper_create_surface(state_, app.compositor, app.layer_shell,
                                output))
        klog("wallpaper: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool WallpaperPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool WallpaperPerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (!state_.layer_surface)
        return true;
    state_.app = &app;
    state_.output_name = mon.output.name;
    if (!wallpaper_init_egl(state_, app.renderer, app.egl_display, app.egl_config,
                          app.egl_context))
        return true;
    wallpaper_sync_active_mode(state_, app.cfg, mon.output.name);
    state_.on_resize = [&app, &mon, this] {
        if (!app.cfg.wallpaper_animated_enabled)
            return;
        wallpaper_columns_stop_all(state_);
        wallpaper_sync_from_config(state_, app.cfg, mon.output.name, true);
    };
    wallpaper_request_frame(state_);
    eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                   app.egl_context);
    return true;
}

void WallpaperPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    wallpaper_columns_stop_all(state_);
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool WallpaperPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void WallpaperPerMonitorModule::pause_animation() {
    wallpaper_columns_pause_all(state_);
}

void WallpaperPerMonitorModule::resume_animation() {
    wallpaper_columns_resume_all(state_);
}

void WallpaperPerMonitorModule::request_frame() { wallpaper_wake(state_); }

MediaDecodeStatus
WallpaperPerMonitorModule::decode_status(int column_index) const {
    return wallpaper_column_status(state_, column_index);
}

void WallpaperPerMonitorModule::resync(WaylandState &, MonitorOutput &mon,
                                     const Config &new_cfg) {
    wallpaper_sync_active_mode(state_, new_cfg, mon.output.name);
    wallpaper_request_frame(state_);
}

bool NotificationViewPerMonitorModule::create_surface(WaylandState &app,
                                                MonitorOutput &mon,
                                                wl_output *output) {
    if (notifications_effective_enabled(app.cfg, mon.output.name) &&
        !notification_view_create_surface(state_, app.compositor, app.layer_shell,
                                    output))
        klog("notification: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool NotificationViewPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool NotificationViewPerMonitorModule::init_egl(WaylandState &app,
                                          MonitorOutput &mon) {
    if (state_.layer_surface &&
        notification_view_init_egl(state_, app.notification, app.renderer, app.egl_display,
                             app.egl_config, app.egl_context))
        eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                       app.egl_context);
    return true;
}

void NotificationViewPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool NotificationViewPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void NotificationViewPerMonitorModule::request_frame() {
    notification_view_request_frame(state_);
}

void NotificationViewPerMonitorModule::handle_click(WaylandState &, MonitorOutput &,
                                              wl_surface *, int button,
                                              double x, double y, uint32_t) {
    if (button != BTN_LEFT)
        return;
    if (notification_view_handle_close_click(state_, x, y))
        notification_view_request_frame(state_);
}

void NotificationViewPerMonitorModule::handle_pointer_move(WaylandState &app,
                                                     MonitorOutput &, double x,
                                                     double y) {
    bool changed = app.pointer.focused_surface == state_.surface
                       ? notification_view_set_close_hover(state_, x, y)
                       : notification_view_clear_close_hover(state_);
    if (changed)
        notification_view_request_frame(state_);
}

bool NotificationViewPerMonitorModule::wants_pointing_hand_cursor() const {
    return state_.hovered_close_id != 0;
}

void NotificationViewPerMonitorModule::resync(WaylandState &app, MonitorOutput &mon) {
    bool want = notifications_effective_enabled(app.cfg, mon.output.name);
    bool have = state_.layer_surface != nullptr;
    if (want && !have) {
        if (notification_view_create_surface(state_, app.compositor, app.layer_shell,
                                       mon.output.wl)) {
            while (!state_.configured)
                wl_display_dispatch(app.display);
            if (notification_view_init_egl(state_, app.notification, app.renderer,
                                     app.egl_display, app.egl_config,
                                     app.egl_context))
                eglMakeCurrent(app.egl_display, mon.egl_surface,
                               mon.egl_surface, app.egl_context);
        }
    } else if (!want && have) {
        destroy_layer_surface(app.egl_display, state_.surface,
                              state_.layer_surface, state_.egl_window,
                              state_.egl_surface);
        state_.configured = false;
    }
}

bool IdlePerMonitorModule::create_surface(WaylandState &app,
                                           MonitorOutput &mon,
                                           wl_output *output) {
    if (mon.output.name == "HEADLESS")
        return true;
    if (!idle_overlay_create_surface(state_, app.compositor, app.layer_shell,
                                      output))
        klog("idle-overlay: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool IdlePerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool IdlePerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (!state_.layer_surface)
        return true;
    if (!idle_overlay_init_egl(state_, app.renderer, app.egl_display,
                                app.egl_config, app.egl_context))
        return true;
    state_.draw_ambient = [&mon](Node &root, float w, float h) {
        auto *wp = mon.module<WallpaperPerMonitorModule>();
        if (!wp)
            return;
        wallpaper_draw_columns(wp->wallpaper_state(), &root,
                             static_cast<int32_t>(w), static_cast<int32_t>(h));
    };
    eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                   app.egl_context);
    return true;
}

void IdlePerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool IdlePerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void IdlePerMonitorModule::timer_tick(WaylandState &app, MonitorOutput &mon) {
    if (mon.output.name == "HEADLESS")
        return;
    if (!app.idle.last_activity.count(mon.output.name))
        app.idle.last_activity[mon.output.name] =
            std::chrono::steady_clock::now();

    bool ambient_now =
        ambient_effective_enabled(app.cfg, mon.output.name) &&
        is_idle(app.idle, mon.output.name,
                 ambient_effective_timeout_seconds(app.cfg, mon.output.name));
    bool screensaver_now =
        screensaver_effective_enabled(app.cfg, mon.output.name) &&
        is_idle(
            app.idle, mon.output.name,
            screensaver_effective_timeout_seconds(app.cfg, mon.output.name));

    idle_overlay_set_active(state_, ambient_now, screensaver_now);

    if (screensaver_now != screensaver_was_active_) {
        screensaver_was_active_ = screensaver_now;
        if (auto *wp = mon.module<WallpaperPerMonitorModule>()) {
            if (screensaver_now)
                wp->pause_animation();
            else
                wp->resume_animation();
        }
    }
}

std::vector<std::unique_ptr<Module>> build_app_modules() {
    std::vector<std::unique_ptr<Module>> modules;
    modules.push_back(std::make_unique<LauncherModule>());
    modules.push_back(std::make_unique<LogoutModule>());
    modules.push_back(std::make_unique<DashboardModule>());
    modules.push_back(std::make_unique<OverviewModule>());
    modules.push_back(std::make_unique<SettingsModule>());
    modules.push_back(std::make_unique<RainModule>());
    modules.push_back(std::make_unique<VisualizerModule>());
    modules.push_back(std::make_unique<LockModule>());
    return modules;
}

namespace {

LockModule *find_lock_module(WaylandState &app) {
    for (auto &m : app.overlays)
        if (auto *lm = dynamic_cast<LockModule *>(m.get()))
            return lm;
    return nullptr;
}

} // namespace

void lock_notify_output_added(WaylandState &app, wl_output *output,
                                 const char *name) {
    if (auto *lm = find_lock_module(app))
        lock_hotplug_add(lm->state(), output, name);
}

void lock_notify_output_removed(WaylandState &app, wl_output *output) {
    if (auto *lm = find_lock_module(app))
        lock_hotplug_remove(lm->state(), output);
}

bool lock_is_locked(WaylandState &app) {
    auto *lm = find_lock_module(app);
    return lm && lm->state().locked;
}

void lock_start(WaylandState &app) {
    if (auto *lm = find_lock_module(app))
        lock_request(lm->state(), app);
}

std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules() {
    std::vector<std::unique_ptr<PerMonitorModule>> modules;
    modules.push_back(std::make_unique<BarPerMonitorModule>());
    modules.push_back(std::make_unique<WallpaperPerMonitorModule>());
    modules.push_back(std::make_unique<OsdPerMonitorModule>());
    modules.push_back(std::make_unique<NotificationViewPerMonitorModule>());
    modules.push_back(std::make_unique<IdlePerMonitorModule>());
    return modules;
}
