#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/module.h"
#include "app/per_monitor_module.h"

#include "modules/blink.h"
#include "modules/expanse.h"
#include "modules/herald.h"
#include "modules/spark.h"

struct Config;

class SparkPerMonitorModule final : public PerMonitorModule {
  public:
    SparkState &state() { return state_; }

    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void tick(WaylandState &app, MonitorOutput &mon) override;

  private:
    SparkState state_;
};

class ExpansePerMonitorModule final : public PerMonitorModule {
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
    const ExpanseState &expanse_state() const { return state_; }

  private:
    ExpanseState state_;
};

class BlinkPerMonitorModule final : public PerMonitorModule {
  public:
    bool create_surface(WaylandState &app, MonitorOutput &mon,
                        wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void timer_tick(WaylandState &app, MonitorOutput &mon) override;

  private:
    BlinkOverlayState state_;
    bool screensaver_was_active_ = false;
};

class HeraldViewPerMonitorModule final : public PerMonitorModule {
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
    HeraldView state_;
};

std::vector<std::unique_ptr<Module>> build_app_modules();
std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules();

struct WaylandState;

struct TrullaEnv {
    std::function<std::vector<std::string>()> monitor_names_fn;
    std::function<std::string()> focused_monitor_fn;
    std::function<MediaDecodeStatus(const std::string &, int)> decode_status_fn;
};

TrullaEnv trulla_env(WaylandState &app);

void penance_notify_output_added(WaylandState &app, wl_output *output,
                                 const char *name);
void penance_notify_output_removed(WaylandState &app, wl_output *output);
bool penance_is_locked(WaylandState &app);
