#pragma once

#include <chrono>
#include <vector>

#include "app/ipc.h"

#include "config/rain_config.h"

#include "modules/rain/matrix_rain.h"
#include "modules/rain/stiletto_rain.h"

#include "render/renderer.h"
#include "render/scene.h"
#include "render/toplevel_window.h"

#include "service/input_service.h"

struct WaylandState;

struct RainState {
    ToplevelWindowBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    MatrixRain matrix;
    StilettoRain stiletto;
    RainMode mode = RainMode::Matrix;
    bool async_speed = false;
    int built_width = 0;
    int built_height = 0;
    std::chrono::steady_clock::time_point last_tick;
};

void rain_request_frame(RainState &state);

void rain_toggle(RainState &state, WaylandState &app);

void rain_handle_key_event(RainState &state, WaylandState &app,
                           const KeyEvent &event);

void rain_apply_params(RainState &state, const RainParams &params);

std::vector<IpcHandler> rain_ipc_handlers(RainState &rain, WaylandState &state);

void rain_paint(RainState &state);
