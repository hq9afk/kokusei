#include "app/user_info.h"

#include "modules/qixing.h"
#include "modules/qixing/widget/yuheng_widget.h"

namespace qixing_detail {

Pill yuheng_pill(MonitorOutput &mon) {
    QixingPerMonitorState &bs = qixing_state(mon);
    return Pill{PillId::Yuheng, &bs.yuheng_texture, user_info::username(),
                nullptr, [&mon, &bs] {
                    close_other_overlays(mon, PillId::Yuheng);
                    update_pill_expand(bs.capsule, mon.animations,
                                       PillId::Yuheng, true, true);
                    qixing_paint(mon);
                    if (Module *cc = find_overlay_by_name(*mon.app, "yuheng"))
                        cc->toggle_from_widget(*mon.app);
                }};
}

} // namespace qixing_detail
