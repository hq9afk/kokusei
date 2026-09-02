#include <algorithm>

#include "app/module_registry.h"
#include "app/monitor_output.h"

#include "core/log.h"

#include "modules/qixing.h"
#include "modules/trulla.h"

#include "render/overlay_panel.h"

#include "service/trulla_service.h"

void monitor_output_destroy(MonitorOutput &mon) {
    for (auto &m : mon.modules)
        m->destroy(*mon.app, mon);
    if (mon.output.wl)
        wl_output_release(mon.output.wl);
}

MonitorOutput *find_monitor_by_name_wl(WaylandState &app, wl_output *wl) {
    for (auto &mon : app.outputs)
        if (mon->output.wl == wl)
            return mon.get();
    return nullptr;
}

MonitorOutput *find_monitor_for_surface(WaylandState &app,
                                        wl_surface *surface) {
    if (!surface)
        return nullptr;
    for (auto &mon : app.outputs)
        for (auto &m : mon->modules)
            if (m->owns_surface(surface))
                return mon.get();
    return nullptr;
}

void monitor_output_create_surfaces(WaylandState &app, MonitorOutput &mon) {
    mon.modules = build_per_monitor_modules();
    for (auto &m : mon.modules)
        m->create_surface(app, mon, mon.output.wl);
}

void monitor_output_wait_configured(WaylandState &app, MonitorOutput &mon) {
    for (;;) {
        bool all_configured = true;
        for (auto &m : mon.modules)
            if (!m->configured())
                all_configured = false;
        if (all_configured)
            return;
        wl_display_dispatch(app.display);
    }
}

void monitor_output_finish_egl(WaylandState &app, MonitorOutput &mon) {
    for (auto &m : mon.modules)
        m->init_egl(app, mon);
}

void monitor_output_activate(WaylandState &app, MonitorOutput &mon) {
    if (mon.activated)
        return;
    klog("output: activating '%s'", mon.output.name.c_str());
    monitor_output_create_surfaces(app, mon);
    monitor_output_wait_configured(app, mon);
    monitor_output_finish_egl(app, mon);
    mon.activated = true;
}

void request_all_frames(MonitorOutput &mon) {
    for (auto &m : mon.modules)
        m->request_frame();
}

namespace app_detail {

void rest_egl_current(WaylandState &app) {
    if (!app.outputs.empty())
        eglMakeCurrent(app.egl_display, app.outputs.front()->egl_surface,
                       app.outputs.front()->egl_surface, app.egl_context);
}

const std::vector<Workspace> &monitor_workspaces(const MonitorOutput &mon) {
    static const std::vector<Workspace> empty;
    if (mon.app->compositor_backend !=
        WaylandState::CompositorBackend::Hyprland)
        return empty;
    auto it = mon.app->hypr.by_monitor.find(mon.output.name);
    return it != mon.app->hypr.by_monitor.end() ? it->second.workspaces : empty;
}

int monitor_active_workspace_id(const MonitorOutput &mon) {
    if (mon.app->compositor_backend !=
        WaylandState::CompositorBackend::Hyprland)
        return -1;
    auto it = mon.app->hypr.by_monitor.find(mon.output.name);
    return it != mon.app->hypr.by_monitor.end() ? it->second.active_id : -1;
}

void apply_config_update(WaylandState &app, Config new_cfg) {
    bool blink_changed =
        app.cfg.blink_management_enabled != new_cfg.blink_management_enabled ||
        app.cfg.ambient_enabled != new_cfg.ambient_enabled ||
        app.cfg.ambient_timeout_seconds != new_cfg.ambient_timeout_seconds ||
        app.cfg.screensaver_enabled != new_cfg.screensaver_enabled ||
        app.cfg.screensaver_timeout_seconds !=
            new_cfg.screensaver_timeout_seconds ||
        app.cfg.monitor_overrides != new_cfg.monitor_overrides;
    if (blink_changed) {
        std::vector<std::string> names;
        for (auto &mon : app.outputs)
            names.push_back(mon->output.name);
        blink_reset(app.blink, names);
    }

    for (auto &mon : app.outputs) {
        if (auto *wp = mon->module<ExpansePerMonitorModule>())
            wp->resync(app, *mon, new_cfg);

        bool new_autohide =
            autohide_effective_enabled(new_cfg, mon->output.name);
        if (new_autohide != mon->autohide.enabled)
            qixing_detail::monitor_autohide_apply(*mon, new_autohide);

        if (auto *nv = mon->module<HeraldViewPerMonitorModule>())
            nv->resync(app, *mon);
    }

    app.cfg = new_cfg;
    for (auto &mon : app.outputs)
        for (auto &m : mon->modules)
            m->request_frame();
}

void save_and_apply_config_update(WaylandState &app, Config new_cfg) {
    apply_config_update(app, new_cfg);
    trulla_service_save(app.cfg);
    app.config_own_write_pending = true;
}

MonitorOutput *active_target_monitor(WaylandState &app) {
    std::vector<Output *> outputs;
    for (auto &mon : app.outputs)
        outputs.push_back(&mon->output);
    std::string focused_name =
        app.compositor_backend == WaylandState::CompositorBackend::Hyprland
            ? app.hypr.focused_monitor
            : std::string();
    wl_output *pointer_hint = app.last_pointer_monitor
                                  ? app.last_pointer_monitor->output.wl
                                  : nullptr;
    wl_output *target =
        active_output_select(outputs, focused_name, pointer_hint);
    return target ? find_monitor_by_name_wl(app, target) : nullptr;
}

void trulla_retarget(WaylandState &app, TrullaState &trulla,
                     MonitorOutput &target) {
    TrullaState &s = trulla;
    TrullaEnv env = trulla_env(app);
    wl_output *bound = overlay_panel_retarget(
        s.base, app.display, app.trulla_bound_output, target.output.wl,
        target.output.name.c_str(),
        [&](wl_output *out) {
            return trulla_create_surface(s, app.compositor, app.layer_shell,
                                         out);
        },
        [&] {
            return trulla_init_egl(s, app.cfg, app.renderer, app.egl_display,
                                   app.egl_config, app.egl_context,
                                   env.monitor_names_fn, env.focused_monitor_fn,
                                   env.decode_status_fn);
        });
    if (bound)
        app.trulla_bound_output = bound;
    else
        app.trulla_enabled = false;

    if (!app.outputs.empty())
        eglMakeCurrent(app.egl_display, app.outputs.front()->egl_surface,
                       app.outputs.front()->egl_surface, app.egl_context);
}

} // namespace app_detail
