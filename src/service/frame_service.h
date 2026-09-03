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

void frame_clock_drop_callback(FrameClock &clock);
