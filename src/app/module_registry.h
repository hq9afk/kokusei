#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/module.h"
#include "app/per_monitor_module.h"

#include "modules/dock.h"
#include "modules/idle.h"
#include "modules/notification.h"
#include "modules/osd.h"
#include "modules/wallpaper.h"

struct Config;

class DockPerMonitorModule final : public PerMonitorModule {
  public:
    DockState &state() { return state_; }

    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void request_frame() override;

  private:
    DockState state_;
};

class OsdPerMonitorModule final : public PerMonitorModule {
  public:
    OsdState &state() { return state_; }

    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void tick(WaylandState &app, MonitorOutput &mon) override;

  private:
    OsdState state_;
};

class WallpaperPerMonitorModule final : public PerMonitorModule {
  public:
    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void request_frame() override;

    void resync(WaylandState &app, MonitorOutput &mon, const Config &new_cfg);

    void pause_animation();
    void resume_animation();
    MediaDecodeStatus decode_status(int column_index) const;
    const WallpaperState &wallpaper_state() const { return state_; }

  private:
    WallpaperState state_;
};

class IdlePerMonitorModule final : public PerMonitorModule {
  public:
    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void timer_tick(WaylandState &app, MonitorOutput &mon) override;

  private:
    IdleOverlayState state_;
    bool screensaver_was_active_ = false;
};

class NotificationViewPerMonitorModule final : public PerMonitorModule {
  public:
    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void request_frame() override;
    void handle_click(WaylandState &app, MonitorOutput &mon,
                      wl_surface *surface, int button, double x, double y,
                      uint32_t serial) override;
    void handle_pointer_move(WaylandState &app, MonitorOutput &mon, double x,
                             double y) override;
    bool wants_pointing_hand_cursor() const override;

    void resync(WaylandState &app, MonitorOutput &mon);

  private:
    NotificationView state_;
};

std::vector<std::unique_ptr<Module>> build_app_modules();
std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules();

struct WaylandState;

struct SettingsEnv {
    std::function<std::vector<std::string>()> monitor_names_fn;
    std::function<std::string()> focused_monitor_fn;
    std::function<MediaDecodeStatus(const std::string &, int)> decode_status_fn;
};

SettingsEnv settings_env(WaylandState &app);

void lock_notify_output_added(WaylandState &app, wl_output *output,
                                 const char *name);
void lock_notify_output_removed(WaylandState &app, wl_output *output);
bool lock_is_locked(WaylandState &app);
void lock_start(WaylandState &app);
