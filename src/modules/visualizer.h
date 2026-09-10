#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/ipc.h"

#include "config/visualizer_config.h"

#include "modules/visualizer/audio_capture.h"

#include "render/toplevel_window.h"

#include "service/input_service.h"

struct WaylandState;

struct VisualizerRenderThreadState {
    std::mutex mutex;
    std::condition_variable cv;
    bool shutdown = false;
    VisualizerParams params;
};

struct VisualizerState {
    ToplevelWindowBase base;
    VisualizerAudioCapture capture;
    std::chrono::steady_clock::time_point fade_start{};

    EGLConfig egl_config = nullptr;
    EGLContext render_context = EGL_NO_CONTEXT;
    std::thread render_thread;
    std::unique_ptr<VisualizerRenderThreadState> thread_state;
};

void visualizer_shutdown(VisualizerState &state);

void visualizer_apply_params(VisualizerState &state,
                            const VisualizerParams &params);

void visualizer_toggle(VisualizerState &state, WaylandState &app);

void visualizer_handle_key_event(VisualizerState &state, WaylandState &app,
                                const KeyEvent &event);

std::vector<IpcHandler> visualizer_ipc_handlers(VisualizerState &visualizer,
                                               WaylandState &state);
