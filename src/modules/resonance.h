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

struct ResonanceRenderThreadState {
    std::mutex mutex;
    std::condition_variable cv;
    bool shutdown = false;
};

struct ResonanceState {
    ToplevelWindowBase base;
    ResonanceAudioCapture capture;
    std::chrono::steady_clock::time_point fade_start{};

    EGLConfig egl_config = nullptr;
    EGLContext render_context = EGL_NO_CONTEXT;
    std::thread render_thread;
    std::unique_ptr<ResonanceRenderThreadState> thread_state;
};

void resonance_shutdown(ResonanceState &state);

void resonance_toggle(ResonanceState &state, WaylandState &app);

void resonance_handle_key_event(ResonanceState &state, WaylandState &app,
                                const KeyEvent &event);

std::vector<IpcHandler> resonance_ipc_handlers(ResonanceState &resonance,
                                               WaylandState &state);
