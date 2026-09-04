#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/ipc.h"

#include "modules/resonance/audio_capture.h"

#include "render/toplevel_window.h"

#include "service/input_service.h"

struct WaylandState;

struct ResonanceFrame {
    int width = 0;
    int height = 0;
    int32_t scale = 1;
    float opacity = 1.0f;
    int tick = 0;
    bool step = false;
    bool modified = false;
    std::vector<float> l;
    std::vector<float> r;
};

struct ResonanceRenderThreadState {
    std::mutex mutex;
    std::condition_variable cv;
    ResonanceFrame pending;
    bool have_frame = false;
    bool shutdown = false;
};

struct ResonanceState {
    ToplevelWindowBase base;
    ResonanceAudioCapture capture;
    int tick = 0;
    std::chrono::steady_clock::time_point last_step{};
    std::vector<float> pending_l;
    std::vector<float> pending_r;
    bool pending_modified = false;

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
