#include <chrono>
#include <filesystem>

#include "app/module_registry.h"
#include "app/monitor_output.h"
#include "app/text_input_client.h"
#include "app/wayland_state.h"

#include "config/qixing_config.h"

#include "modules/expanse.h"
#include "modules/herald.h"
#include "modules/liyue.h"
#include "modules/overseer.h"
#include "modules/penance.h"
#include "modules/qixing.h"
#include "modules/resonance.h"
#include "modules/spark.h"
#include "modules/starward.h"
#include "modules/stiletto.h"
#include "modules/trulla.h"
#include "modules/yuheng.h"

#include "render/animated_image.h"
#include "render/image.h"
#include "render/layer_surface.h"
#include "render/palette.h"

#include "service/mpris_service.h"
#include "service/telemetry_service.h"

namespace {

class OverseerModule final : public Module, public TextInputClient {
  public:
    const char *name() const override { return "overseer"; }
    bool is_open() const override { return state_.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = overseer_create_surface(state_, app.compositor, app.layer_shell,
                                        output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!overseer_init_egl(state_, app.renderer, app.egl_display,
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
        return overseer_text_input_state(state_);
    }
    void text_input_apply_edit(const TextInputEdit &edit) override {
        overseer_text_input_apply_edit(state_, edit);
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
    void request_frame() override { overseer_request_frame(state_); }

    bool tick() override {
        overseer_search_start_pending(state_);
        return overseer_tick(state_);
    }
    int poll_timeout_ms() const override {
        return overseer_poll_timeout_ms(state_);
    }
    bool timer_tick(WaylandState &) override {
        if (!state_.open)
            return false;
        text_field_blink_toggle(state_.search);
        request_frame();
        return true;
    }

    void handle_click(WaylandState &, double x, double y) override {
        overseer_handle_click(state_, x, y);
    }
    void handle_pointer_move(WaylandState &, wl_surface *focused_surface,
                             double x, double y) override {
        overseer_handle_pointer_move(state_, focused_surface, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return state_.open && state_.hovered_index >= 0;
    }
    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        overseer_handle_key_event(state_, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        auto toggle_retargeted = [this, &app](bool global) {
            if (!state_.open) {
                MonitorOutput *target = app_detail::active_target_monitor(app);
                if (target && (target->output.wl != state_.bound_output ||
                               !state_.layer_surface))
                    overseer_retarget(state_, app.compositor, app.layer_shell,
                                      app.display, app.renderer,
                                      app.egl_display, app.egl_config,
                                      app.egl_context, target->output.wl,
                                      target->output.name.c_str());
            }
            overseer_toggle(state_, global);
        };
        return {
            {"overseer", [toggle_retargeted] { toggle_retargeted(false); },
             "toggle the overseer, searching from $HOME"},
            {"overseer global",
             [toggle_retargeted] { toggle_retargeted(true); },
             "toggle the overseer, searching from /"},
        };
    }

    std::vector<std::pair<int, std::function<void()>>>
    extra_poll_sources(WaylandState &app) override {
        std::vector<std::pair<int, std::function<void()>>> sources;
        auto dispatch = [this, &app] {
            if (overseer_search_poll(state_)) {
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
    OverseerState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
};

class StarwardModule final : public Module {
  public:
    const char *name() const override { return "starward"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = starward_create_surface(state_, app.compositor, app.layer_shell,
                                        output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!starward_init_egl(state_, app.renderer, app.egl_display,
                               app.egl_config, app.egl_context))
            return false;
        state_.bound_output = output_;
        request_frame();

        starward_apply_logo_config(state_, app.cfg.starward_animated_logo);
        return true;
    }

    bool configured() const override {
        return !want_ || state_.base.configured;
    }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { starward_request_frame(state_); }

    bool timer_tick(WaylandState &) override { return false; }

    void handle_pointer_move(WaylandState &, wl_surface *focused_surface,
                             double x, double y) override {
        if (!state_.base.open)
            return;
        if (focused_surface == state_.base.surface)
            starward_handle_hover(state_, x, y);
        else
            starward_clear_hover(state_);
        request_frame();
    }

    void handle_click(WaylandState &, double x, double y) override {
        starward_handle_click(state_, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return state_.base.open && state_.hovered_index >= 0;
    }
    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        starward_handle_key_event(state_, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return starward_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    wl_output *bound_output() const override { return state_.bound_output; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                starward_retarget(state_, app.compositor, app.layer_shell,
                                  app.display, app.renderer, app.egl_display,
                                  app.egl_config, app.egl_context,
                                  target->output.wl,
                                  target->output.name.c_str());
        }
        starward_apply_logo_config(state_, app.cfg.starward_animated_logo);
        starward_toggle(state_, true);
    }

  private:
    StarwardState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
};

class YuhengModule final : public Module {
  public:
    const char *name() const override { return "yuheng"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = yuheng_create_surface(state_, app.compositor, app.layer_shell,
                                      output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!yuheng_init_egl(state_, app.renderer, app, app.egl_display,
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
        yuheng_request_frame(
            state_, static_cast<float>(qixing_detail::kQixingHeight),
            static_cast<float>(qixing_detail::kQixingTopMargin));
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
        yuheng_handle_pointer_move(state_, app, x);
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
        yuheng_handle_click(state_, app, x, y);
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        yuheng_handle_key_event(state_, app, event);
    }
    void handle_scroll(WaylandState &, double dy) override {
        yuheng_handle_scroll(state_, dy);
        request_frame();
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return yuheng_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    wl_output *bound_output() const override { return state_.bound_output; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                yuheng_retarget(state_, app.compositor, app.layer_shell,
                                app.display, app.renderer, app, app.egl_display,
                                app.egl_config, app.egl_context,
                                target->output.wl, target->output.name.c_str());
            cpu_temp_poll(app.cpu_temp);
            system_stats_poll(app.system_stats);
            gpu_temp_poll(app.gpu_temp);
        }
        yuheng_toggle(state_, true);
    }

  private:
    YuhengState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
    int poll_tick_ = 0;
};

class LiyueModule final : public Module {
  public:
    const char *name() const override { return "liyue"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = liyue_create_surface(state_, app.compositor, app.layer_shell,
                                     output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        if (!liyue_init_egl(state_, app.renderer, app.egl_display,
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
    void request_frame() override { liyue_request_frame(state_); }

    int poll_timeout_ms() const override {
        return state_.base.open ? kLiyueCaptureIntervalMs : -1;
    }

    bool tick() override {
        if (!state_.base.open)
            return false;
        auto now = std::chrono::steady_clock::now();
        if (now - last_capture_arm_ <
            std::chrono::milliseconds(kLiyueCaptureIntervalMs))
            return false;
        last_capture_arm_ = now;
        return true;
    }

    void handle_pointer_move(WaylandState &app, wl_surface *, double x,
                             double y) override {
        liyue_handle_pointer_move(state_, app, x, y);
        hovering_clickable_ =
            app.pointer.focused_surface == state_.base.surface &&
            liyue_point_is_clickable(state_, app, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return hovering_clickable_;
    }
    void handle_pointer_release() override {
        if (state_.app_ptr)
            liyue_handle_pointer_release(state_, *state_.app_ptr);
    }

    void handle_click(WaylandState &app, double x, double y) override {
        liyue_handle_click(state_, app, x, y);
        request_frame();
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        liyue_handle_key_event(state_, app, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return liyue_ipc_handlers(state_, app);
    }

    bool opened_by_widget() const override { return state_.opened_by_widget; }
    void toggle_from_widget(WaylandState &app) override {
        if (!state_.base.open) {
            MonitorOutput *target = app_detail::active_target_monitor(app);
            if (target && (target->output.wl != state_.bound_output ||
                           !state_.base.layer_surface))
                liyue_retarget(state_, app.compositor, app.layer_shell,
                               app.display, app.renderer, app.egl_display,
                               app.egl_config, app.egl_context,
                               target->output.wl, target->output.name.c_str());
        }
        liyue_toggle(state_, app, true);
    }

  private:
    LiyueState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
    std::chrono::steady_clock::time_point last_capture_arm_{};
};

class TrullaModule final : public Module, public TextInputClient {
  public:
    const char *name() const override { return "trulla"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &app, wl_output *output) override {
        output_ = output;
        want_ = trulla_create_surface(state_, app.compositor, app.layer_shell,
                                      output);
        return want_;
    }

    bool init_egl(WaylandState &app) override {
        TrullaEnv env = trulla_env(app);
        if (!trulla_init_egl(state_, app.cfg, app.renderer, app.egl_display,
                             app.egl_config, app.egl_context,
                             env.monitor_names_fn, env.focused_monitor_fn,
                             env.decode_status_fn))
            return false;
        app.trulla_bound_output = output_;
        app.trulla_enabled = true;
        state_.sync_text_input_focus = [this, &app](bool focused) {
            if (focused)
                app.text_input.set_focused_client(state_.base.surface, this);
            else
                app.text_input.clear_focused_client(this);
        };
        return true;
    }

    TextInputState text_input_state() const override {
        return trulla_text_input_state(state_);
    }
    void text_input_apply_edit(const TextInputEdit &edit) override {
        trulla_text_input_apply_edit(state_, edit);
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
    void request_frame() override { trulla_request_frame(state_); }

    bool timer_tick(WaylandState &) override {
        if (state_.focused_field == TrullaFieldId::None)
            return false;
        text_field_blink_toggle(state_.field_buffer);
        request_frame();
        return true;
    }

    void handle_click(WaylandState &app, double x, double y) override {
        trulla_handle_click(
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
                              trulla_point_is_clickable(state_, x, y);
    }
    bool wants_pointing_hand_cursor() const override {
        return hovering_clickable_;
    }
    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        trulla_handle_key_event(
            state_, app.cfg,
            [&app](Config c) {
                app_detail::save_and_apply_config_update(app, c);
            },
            event);
    }
    void handle_scroll(WaylandState &, double dy) override {
        trulla_handle_scroll(state_, dy);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return trulla_ipc_handlers(state_, app);
    }

  private:
    TrullaState state_;
    wl_output *output_ = nullptr;
    bool want_ = false;
    bool hovering_clickable_ = false;
};

class StilettoModule final : public Module {
  public:
    const char *name() const override { return "stiletto"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }
    bool init_egl(WaylandState &) override { return true; }
    bool configured() const override { return true; }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { stiletto_request_frame(state_); }

    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        stiletto_handle_key_event(state_, app, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return stiletto_ipc_handlers(state_, app);
    }

  private:
    StilettoState state_;
};

class ResonanceModule final : public Module {
  public:
    ~ResonanceModule() override { resonance_shutdown(state_); }

    const char *name() const override { return "resonance"; }
    bool is_open() const override { return state_.base.open; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }
    bool init_egl(WaylandState &) override { return true; }
    bool configured() const override { return true; }
    wl_surface *surface() const override { return state_.base.surface; }
    void request_frame() override { resonance_request_frame(state_); }

    void handle_key_event(WaylandState &app, const KeyEvent &event) override {
        resonance_handle_key_event(state_, app, event);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return resonance_ipc_handlers(state_, app);
    }

  private:
    ResonanceState state_;
};

class PenanceModule final : public Module {
  public:
    PenanceState &state() { return state_; }

    const char *name() const override { return "penance"; }
    bool is_open() const override { return state_.active; }

    bool create_surface(WaylandState &, wl_output *) override { return true; }

    bool init_egl(WaylandState &app) override {
        state_.app = &app;
        state_.draw_expanse = [&app](const std::string &output_name, Node &root,
                                     int32_t w, int32_t h) {
            for (auto &mon : app.outputs) {
                if (mon->output.name != output_name)
                    continue;
                if (auto *wp = mon->module<ExpansePerMonitorModule>())
                    expanse_draw_columns(wp->expanse_state(), &root, w, h);
                return;
            }
        };
        state_.panel_gated_for = [&app](const std::string &output_name) {
            return penance_effective_enabled(app.cfg, output_name);
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
        return penance_focused_surface(state_);
    }
    bool owns_surface(wl_surface *s) const override {
        return penance_owns_surface(state_, s);
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
        penance_timer_tick(state_);
        return true;
    }

    void handle_key_event(WaylandState &, const KeyEvent &event) override {
        penance_handle_key(state_, event);
    }
    void handle_click(WaylandState &app, double x, double y) override {
        penance_handle_click(state_, app.pointer.focused_surface, x, y);
    }

    std::vector<IpcHandler> ipc_handlers(WaylandState &app) override {
        return {{"penance",
                 [this, &app] {
                     cpu_temp_poll(app.cpu_temp);
                     system_stats_poll(app.system_stats);
                     gpu_temp_poll(app.gpu_temp);
                     penance_request(state_, app);
                 },
                 "lock the session"}};
    }

  private:
    PenanceState state_;
    int poll_tick_ = 0;
};

void expanse_sync_active_mode(ExpanseState &wp, const Config &cfg,
                              const std::string &monitor_name) {
    expanse_sync_from_config(wp, cfg, monitor_name,
                             cfg.expanse_animated_enabled);
}

} // namespace

TrullaEnv trulla_env(WaylandState &app) {
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
                if (auto *wp = mon->module<ExpansePerMonitorModule>())
                    return wp->decode_status(column);
            }
            return MediaDecodeStatus::Blink;
        },
    };
}

bool SparkPerMonitorModule::create_surface(WaylandState &app,
                                           MonitorOutput &mon,
                                           wl_output *output) {
    if (!spark_create_surface(state_, app.compositor, app.layer_shell, output))
        klog("spark: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool SparkPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool SparkPerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (state_.layer_surface &&
        spark_init_egl(state_, app.renderer, app.egl_display, app.egl_config,
                       app.egl_context))
        eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                       app.egl_context);
    return true;
}

void SparkPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool SparkPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void SparkPerMonitorModule::tick(WaylandState &, MonitorOutput &) {
    if (state_.visible && std::chrono::steady_clock::now() >= state_.hide_at)
        spark_hide(state_);
}

bool ExpansePerMonitorModule::create_surface(WaylandState &app,
                                             MonitorOutput &mon,
                                             wl_output *output) {
    if (!expanse_create_surface(state_, app.compositor, app.layer_shell,
                                output))
        klog("expanse: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool ExpansePerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool ExpansePerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (!state_.layer_surface)
        return true;
    if (!expanse_init_egl(state_, app.renderer, app.egl_display, app.egl_config,
                          app.egl_context))
        return true;
    expanse_sync_active_mode(state_, app.cfg, mon.output.name);
    state_.on_resize = [&app, &mon, this] {
        if (!app.cfg.expanse_animated_enabled)
            return;
        expanse_columns_stop_all(state_);
        expanse_sync_from_config(state_, app.cfg, mon.output.name, true);
    };
    expanse_request_frame(state_);
    eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                   app.egl_context);
    return true;
}

void ExpansePerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    expanse_columns_stop_all(state_);
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool ExpansePerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void ExpansePerMonitorModule::pause_animation() {
    expanse_columns_pause_all(state_);
}

void ExpansePerMonitorModule::resume_animation() {
    expanse_columns_resume_all(state_);
}

MediaDecodeStatus
ExpansePerMonitorModule::decode_status(int column_index) const {
    return expanse_column_status(state_, column_index);
}

void ExpansePerMonitorModule::resync(WaylandState &, MonitorOutput &mon,
                                     const Config &new_cfg) {
    expanse_sync_active_mode(state_, new_cfg, mon.output.name);
    expanse_request_frame(state_);
}

bool HeraldViewPerMonitorModule::create_surface(WaylandState &app,
                                                MonitorOutput &mon,
                                                wl_output *output) {
    if (heralds_effective_enabled(app.cfg, mon.output.name) &&
        !herald_view_create_surface(state_, app.compositor, app.layer_shell,
                                    output))
        klog("herald: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool HeraldViewPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool HeraldViewPerMonitorModule::init_egl(WaylandState &app,
                                          MonitorOutput &mon) {
    if (state_.layer_surface &&
        herald_view_init_egl(state_, app.herald, app.renderer, app.egl_display,
                             app.egl_config, app.egl_context))
        eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                       app.egl_context);
    return true;
}

void HeraldViewPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool HeraldViewPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void HeraldViewPerMonitorModule::request_frame() {
    herald_view_request_frame(state_);
}

void HeraldViewPerMonitorModule::handle_click(WaylandState &, MonitorOutput &,
                                              wl_surface *, int button,
                                              double x, double y, uint32_t) {
    if (button != BTN_LEFT)
        return;
    if (herald_view_handle_close_click(state_, x, y))
        herald_view_request_frame(state_);
}

void HeraldViewPerMonitorModule::handle_pointer_move(WaylandState &app,
                                                     MonitorOutput &, double x,
                                                     double y) {
    bool changed = app.pointer.focused_surface == state_.surface
                       ? herald_view_set_close_hover(state_, x, y)
                       : herald_view_clear_close_hover(state_);
    if (changed)
        herald_view_request_frame(state_);
}

bool HeraldViewPerMonitorModule::wants_pointing_hand_cursor() const {
    return state_.hovered_close_id != 0;
}

void HeraldViewPerMonitorModule::resync(WaylandState &app, MonitorOutput &mon) {
    bool want = heralds_effective_enabled(app.cfg, mon.output.name);
    bool have = state_.layer_surface != nullptr;
    if (want && !have) {
        if (herald_view_create_surface(state_, app.compositor, app.layer_shell,
                                       mon.output.wl)) {
            while (!state_.configured)
                wl_display_dispatch(app.display);
            if (herald_view_init_egl(state_, app.herald, app.renderer,
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

bool BlinkPerMonitorModule::create_surface(WaylandState &app,
                                           MonitorOutput &mon,
                                           wl_output *output) {
    if (mon.output.name == "HEADLESS")
        return true;
    if (!blink_overlay_create_surface(state_, app.compositor, app.layer_shell,
                                      output))
        klog("blink-overlay: failed to create layer surface on '%s'",
             mon.output.name.c_str());
    return true;
}

bool BlinkPerMonitorModule::configured() const {
    return !state_.layer_surface || state_.configured;
}

bool BlinkPerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (!state_.layer_surface)
        return true;
    if (!blink_overlay_init_egl(state_, app.renderer, app.egl_display,
                                app.egl_config, app.egl_context))
        return true;
    state_.draw_ambient = [&mon](Node &root, float w, float h) {
        auto *wp = mon.module<ExpansePerMonitorModule>();
        if (!wp)
            return;
        expanse_draw_columns(wp->expanse_state(), &root,
                             static_cast<int32_t>(w), static_cast<int32_t>(h));
    };
    eglMakeCurrent(app.egl_display, mon.egl_surface, mon.egl_surface,
                   app.egl_context);
    return true;
}

void BlinkPerMonitorModule::destroy(WaylandState &app, MonitorOutput &) {
    destroy_layer_surface(app.egl_display, state_.surface, state_.layer_surface,
                          state_.egl_window, state_.egl_surface);
}

bool BlinkPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == state_.surface;
}

void BlinkPerMonitorModule::timer_tick(WaylandState &app, MonitorOutput &mon) {
    if (mon.output.name == "HEADLESS")
        return;
    if (!app.blink.last_activity.count(mon.output.name))
        app.blink.last_activity[mon.output.name] =
            std::chrono::steady_clock::now();

    bool ambient_now =
        ambient_effective_enabled(app.cfg, mon.output.name) &&
        is_blink(app.blink, mon.output.name,
                 ambient_effective_timeout_seconds(app.cfg, mon.output.name));
    bool screensaver_now =
        screensaver_effective_enabled(app.cfg, mon.output.name) &&
        is_blink(
            app.blink, mon.output.name,
            screensaver_effective_timeout_seconds(app.cfg, mon.output.name));

    blink_overlay_set_active(state_, ambient_now, screensaver_now);

    if (screensaver_now != screensaver_was_active_) {
        screensaver_was_active_ = screensaver_now;
        if (auto *wp = mon.module<ExpansePerMonitorModule>()) {
            if (screensaver_now)
                wp->pause_animation();
            else
                wp->resume_animation();
        }
    }
}

std::vector<std::unique_ptr<Module>> build_app_modules() {
    std::vector<std::unique_ptr<Module>> modules;
    modules.push_back(std::make_unique<OverseerModule>());
    modules.push_back(std::make_unique<StarwardModule>());
    modules.push_back(std::make_unique<YuhengModule>());
    modules.push_back(std::make_unique<LiyueModule>());
    modules.push_back(std::make_unique<TrullaModule>());
    modules.push_back(std::make_unique<StilettoModule>());
    modules.push_back(std::make_unique<ResonanceModule>());
    modules.push_back(std::make_unique<PenanceModule>());
    return modules;
}

namespace {

PenanceModule *find_penance_module(WaylandState &app) {
    for (auto &m : app.overlays)
        if (auto *lm = dynamic_cast<PenanceModule *>(m.get()))
            return lm;
    return nullptr;
}

} // namespace

void penance_notify_output_added(WaylandState &app, wl_output *output,
                                 const char *name) {
    if (auto *lm = find_penance_module(app))
        penance_hotplug_add(lm->state(), output, name);
}

void penance_notify_output_removed(WaylandState &app, wl_output *output) {
    if (auto *lm = find_penance_module(app))
        penance_hotplug_remove(lm->state(), output);
}

bool penance_is_locked(WaylandState &app) {
    auto *lm = find_penance_module(app);
    return lm && lm->state().locked;
}

std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules() {
    std::vector<std::unique_ptr<PerMonitorModule>> modules;
    modules.push_back(std::make_unique<QixingPerMonitorModule>());
    modules.push_back(std::make_unique<ExpansePerMonitorModule>());
    modules.push_back(std::make_unique<SparkPerMonitorModule>());
    modules.push_back(std::make_unique<HeraldViewPerMonitorModule>());
    modules.push_back(std::make_unique<BlinkPerMonitorModule>());
    return modules;
}
