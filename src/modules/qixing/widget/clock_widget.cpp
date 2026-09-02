#include <ctime>
#include <utility>

#include "modules/qixing.h"
#include "modules/qixing/panel/clock_panel.h"
#include "modules/qixing/widget/clock_widget.h"

#include "render/text.h"

namespace qixing_detail {

void update_clock(Texture &clock_texture) {
    char buf[32];
    time_t now = time(nullptr);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", localtime(&now));
    Texture tex = make_text_texture(buf);
    if (tex.id)
        clock_texture = std::move(tex);
}

Rect draw_clock_pill(Node *root, float height, int32_t surface_width,
                     const Texture &clock_texture, const float tint[4],
                     const float pill_bg[4]) {
    if (!clock_texture.id)
        return {};
    float clock_pill_w = clock_texture.width + kPillPad * 2;
    float clock_x = (surface_width - clock_pill_w) / 2.0f;
    draw_static_pill_row(root, clock_x, height, {&clock_texture}, tint,
                         pill_bg);
    return {clock_x, 0.0f, clock_pill_w, height};
}

void clock_pill_clicked(MonitorOutput &mon) {
    QixingPerMonitorState &bs = qixing_state(mon);
    close_other_overlays(mon, PillId::None);
    if (!bs.clock_panel.base.open) {
        overlay_panel_ensure(
            bs.clock_panel.base, mon.app->display,
            [&] {
                return clock_panel_create_surface(
                    bs.clock_panel, mon.app->compositor, mon.app->layer_shell,
                    mon.output.wl);
            },
            [&] {
                return clock_panel_init_egl(
                    bs.clock_panel, mon.app->renderer, mon.app->egl_display,
                    mon.app->egl_config, mon.app->egl_context);
            });
        app_detail::rest_egl_current(*mon.app);
    }
    clock_panel_toggle(bs.clock_panel,
                       static_cast<float>(mon.width) / 2.0f + kPanelSideMargin);
}

} // namespace qixing_detail
