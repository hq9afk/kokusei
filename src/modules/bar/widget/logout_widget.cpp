#include "modules/bar/widget/logout_widget.h"
#include "modules/bar.h"

namespace bar_detail {

Pill logout_pill(MonitorOutput &mon) {
    return Pill{PillId::Logout, &bar_state(mon).logout_texture,
                "Logout", nullptr, [&mon] {
                    close_other_overlays(mon, PillId::Logout);
                    if (Module *logout =
                            find_overlay_by_name(*mon.app, "logout"))
                        logout->toggle_from_widget(*mon.app);
                }};
}

} // namespace bar_detail
