#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/ipc.h"

#include "render/renderer.h"
#include "render/scene.h"
#include "render/toplevel_window.h"

#include "service/input_service.h"
#include "service/spectrum_service.h"

struct WaylandState;

struct ResonanceFrame {
    int width = 0;
    int height = 0;
    int32_t scale = 1;
    float opacity = 1.0f;
    float elapsed_ms = 0.0f;
    std::vector<float> spectrum;
};

struct ResonanceRenderThreadState {
    std::mutex mutex;
    std::condition_variable cv;
    ResonanceFrame pending;
    bool have_frame = false;
    bool shutdown = false;
};

struct BarVisualizerState {
    Renderer renderer;
    Scene scene;
    std::vector<float> display_values;
    bool ready = false;
};

struct ResonanceState {
    ToplevelWindowBase base;
    AudioSpectrum spectrum;
    BarVisualizerState qixing;
    std::chrono::steady_clock::time_point last_frame;
    bool spectrum_ready = false;

    EGLContext render_context = EGL_NO_CONTEXT;
    std::thread render_thread;
    std::unique_ptr<ResonanceRenderThreadState> thread_state;
};

void resonance_request_frame(ResonanceState &state);

void resonance_shutdown(ResonanceState &state);

void resonance_toggle(ResonanceState &state, WaylandState &app);

void resonance_handle_key_event(ResonanceState &state, WaylandState &app,
                                const KeyEvent &event);

std::vector<IpcHandler> resonance_ipc_handlers(ResonanceState &resonance,
                                               WaylandState &state);

void resonance_paint(ResonanceState &state, EGLConfig egl_config);
