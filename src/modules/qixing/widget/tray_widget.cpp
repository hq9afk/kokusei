#include "modules/qixing/widget/tray_widget.h"
#include "modules/qixing.h"
#include "modules/qixing/panel/tray_panel.h"

namespace qixing_detail {

Pill tray_pill(MonitorOutput &mon) {
    QixingPerMonitorState &bs = qixing_state(mon);
    return Pill{PillId::Tray, &bs.tray_texture, "Tray", nullptr, [&mon, &bs] {
                    close_other_overlays(mon, PillId::Tray);
                    tray_menu_close(bs.tray_menu);
                    if (!bs.tray_panel.base.open) {
                        update_pill_expand(bs.capsule, mon.animations,
                                           PillId::Tray, true, true);
                        qixing_paint(mon);
                        overlay_panel_ensure(
                            bs.tray_panel.base, mon.app->display,
                            [&] {
                                return tray_panel_create_surface(
                                    bs.tray_panel, mon.app->compositor,
                                    mon.app->layer_shell, mon.output.wl);
                            },
                            [&] {
                                return tray_panel_init_egl(
                                    bs.tray_panel, mon.app->renderer,
                                    mon.app->tray, mon.app->egl_display,
                                    mon.app->egl_config, mon.app->egl_context);
                            });
                        app_detail::rest_egl_current(*mon.app);
                    }
                    tray_panel_toggle(bs.tray_panel,
                                      pill_center_x(bs.capsule, PillId::Tray));
                }};
}

} // namespace qixing_detail
