#include "service/frame_service.h"

namespace {

const wl_callback_listener &listener();

void arm_callback(FrameClock &clock) {
    clock.callback = wl_surface_frame(clock.surface);
    wl_callback_add_listener(clock.callback, &listener(), &clock);
}

void frame_done(void *data, wl_callback *cb, uint32_t) {
    auto *clock = static_cast<FrameClock *>(data);
    wl_callback_destroy(cb);
    clock->callback = nullptr;
    if (clock->redraw_requested) {
        clock->redraw_requested = false;
        arm_callback(*clock);
        clock->draw();
    }
}

const wl_callback_listener &listener() {
    static constexpr wl_callback_listener l{.done = frame_done};
    return l;
}

} // namespace

void frame_clock_drop_callback(FrameClock &clock) {
    if (clock.callback) {
        wl_callback_destroy(clock.callback);
        clock.callback = nullptr;
    }
    clock.redraw_requested = false;
}

void request_frame(FrameClock &clock) {
    if (clock.callback) {
        clock.redraw_requested = true;
        return;
    }
    if (!clock.mapped) {
        clock.mapped = true;
        arm_callback(clock);
        clock.draw();
        return;
    }
    clock.redraw_requested = true;
    arm_callback(clock);
    wl_surface_damage_buffer(clock.surface, 0, 0, 1, 1);
    wl_surface_commit(clock.surface);
}
