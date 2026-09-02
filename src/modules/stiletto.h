#pragma once

#include <chrono>
#include <vector>

#include "app/ipc.h"

#include "config/stiletto_config.h"

#include "render/renderer.h"
#include "render/scene.h"
#include "render/stiletto_grid.h"
#include "render/toplevel_window.h"

#include "service/input_service.h"

struct WaylandState;

struct StilettoState {
    ToplevelWindowBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    StilettoGrid grid;
    int grid_width = 0;
    int grid_height = 0;
    std::chrono::steady_clock::time_point last_tick;
};

void stiletto_request_frame(StilettoState &state);

void stiletto_toggle(StilettoState &state, WaylandState &app);

void stiletto_handle_key_event(StilettoState &state, WaylandState &app,
                               const KeyEvent &event);

std::vector<IpcHandler> stiletto_ipc_handlers(StilettoState &stiletto,
                                              WaylandState &state);

void stiletto_paint(StilettoState &state);
