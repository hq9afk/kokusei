#include "app/user_info.h"

#include "modules/bar.h"
#include "modules/bar/widget/dashboard_widget.h"

namespace bar_detail {

Pill dashboard_pill(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    return Pill{PillId::Dashboard, &bs.dashboard_texture, user_info::username(),
                nullptr, [&mon, &bs] {
                    close_other_overlays(mon, PillId::Dashboard);
                    update_pill_expand(bs.capsule, mon.animations,
                                       PillId::Dashboard, true, true);
                    bar_paint(mon);
                    if (Module *cc = find_overlay_by_name(*mon.app, "dashboard"))
                        cc->toggle_from_widget(*mon.app);
                }};
}

} // namespace bar_detail
