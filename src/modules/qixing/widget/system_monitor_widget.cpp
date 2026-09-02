#include "modules/qixing/widget/system_monitor_widget.h"
#include "modules/qixing.h"
#include "modules/qixing/panel/system_monitor_panel.h"

namespace qixing_detail {

Pill cpu_pill(MonitorOutput &mon) {
    QixingPerMonitorState &bs = qixing_state(mon);
    return Pill{
        PillId::Cpu, &bs.cpu_texture, "System Monitor", nullptr, [&mon, &bs] {
            close_other_overlays(mon, PillId::Cpu);
            if (!bs.system_monitor_panel.base.open) {
                update_pill_expand(bs.capsule, mon.animations, PillId::Cpu,
                                   true, true);
                qixing_paint(mon);
                overlay_panel_ensure(
                    bs.system_monitor_panel.base, mon.app->display,
                    [&] {
                        return system_monitor_panel_create_surface(
                            bs.system_monitor_panel, mon.app->compositor,
                            mon.app->layer_shell, mon.output.wl);
                    },
                    [&] {
                        return system_monitor_panel_init_egl(
                            bs.system_monitor_panel, mon.app->renderer,
                            mon.app->cpu_temp, mon.app->gpu_temp,
                            mon.app->system_stats, mon.app->egl_display,
                            mon.app->egl_config, mon.app->egl_context);
                    });
                app_detail::rest_egl_current(*mon.app);
            }
            system_monitor_panel_toggle(bs.system_monitor_panel,
                                        pill_center_x(bs.capsule, PillId::Cpu));
            if (bs.system_monitor_panel.base.open) {
                cpu_temp_poll(mon.app->cpu_temp);
                gpu_temp_poll(mon.app->gpu_temp);
                system_stats_poll(mon.app->system_stats);
            }
        }};
}

} // namespace qixing_detail
