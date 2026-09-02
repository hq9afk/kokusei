#include "app/key_dispatch.h"
#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "modules/qixing.h"

void dispatch_key_events(WaylandState &state,
                         const std::vector<KeyEvent> &events) {
    if (events.empty())
        return;

    if (state.keyboard.focused_surface) {
        for (auto &m : state.overlays) {
            if (!m->owns_surface(state.keyboard.focused_surface))
                continue;
            for (const KeyEvent &event : events)
                m->handle_key_event(state, event);
            m->request_frame();
            app_detail::rest_egl_current(state);
            return;
        }
    }

    for (auto &mon : state.outputs) {
        for (auto &m : mon->modules) {
            if (!m->is_open())
                continue;
            for (const KeyEvent &event : events)
                m->handle_key_event(state, *mon, event);
            m->request_frame();
            app_detail::rest_egl_current(state);
            return;
        }
    }
}
