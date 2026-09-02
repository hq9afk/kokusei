#include "modules/qixing/widget/starward_widget.h"
#include "modules/qixing.h"

namespace qixing_detail {

Pill starward_pill(MonitorOutput &mon) {
    return Pill{PillId::Starward, &qixing_state(mon).starward_texture,
                "Starward", nullptr, [&mon] {
                    close_other_overlays(mon, PillId::Starward);
                    if (Module *starward =
                            find_overlay_by_name(*mon.app, "starward"))
                        starward->toggle_from_widget(*mon.app);
                }};
}

} // namespace qixing_detail
