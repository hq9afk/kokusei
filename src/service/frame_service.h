#pragma once

#include <functional>
#include <wayland-client.h>

struct FrameClock {
    wl_surface *surface = nullptr;
    wl_callback *callback = nullptr;
    bool redraw_requested = false;
    bool mapped = false;
    std::function<void()> draw;
};

void request_frame(FrameClock &clock);

// Drop a pending frame callback that will never fire (surface hidden by a
// session lock or a fullscreen overlay). Leaves `mapped` set so the next
// request_frame takes the damage+commit path that guarantees a frame_done.
void frame_clock_drop_callback(FrameClock &clock);
