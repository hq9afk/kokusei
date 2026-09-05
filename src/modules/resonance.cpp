#include <algorithm>
#include <chrono>
#include <vector>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "config/resonance_config.h"

#include "core/log.h"

#include "modules/resonance.h"
#include "modules/resonance/audio_stages.h"
#include "modules/resonance/blob_pipeline.h"
#include "modules/resonance/fft.h"

#include "render/gl.h"
#include "render/overlay_panel.h"
#include "render/palette.h"

namespace {

void clear_backbuffer(ResonanceState &state, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(palette::window_backdrop.r, palette::window_backdrop.g,
                 palette::window_backdrop.b, palette::window_backdrop.a);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!eglSwapBuffers(state.base.egl_display, state.base.egl_surface))
        klog("resonance: eglSwapBuffers failed 0x%x", eglGetError());
}

void render_thread_main(ResonanceState *state) {
    if (!gl_make_current(state->base.egl_display, state->base.egl_surface,
                         state->render_context)) {
        klog("resonance: render thread eglMakeCurrent failed, eglGetError=0x%x",
             eglGetError());
        return;
    }
    glEnable(GL_BLEND);
    klog("resonance: render thread current %dx%d scale %d", state->base.width,
         state->base.height, state->base.output_scale.scale);

    auto stages = std::make_unique<ResonanceAudioStages>();
    auto blob = std::make_unique<ResonanceBlobPipeline>();
    bool init_ok = stages->init() && blob->init();
    klog("resonance: pipeline init %s",
         init_ok ? "ok" : "FAILED, showing cleared window");

    std::vector<float> audio_l;
    std::vector<float> audio_r;
    std::vector<float> fft_l;
    std::vector<float> fft_r;
    int audio_size = 0;
    int tick = 0;
    int logged_frames = 0;
    bool first_frame_done = false;
    auto last_heartbeat = std::chrono::steady_clock::now();
    int heartbeat_frames = 0;
    float heartbeat_blob_ms = 0.0f;

    auto next = std::chrono::steady_clock::now();

    ResonanceRenderThreadState &ts = *state->thread_state;
    for (;;) {
        ResonanceParams params;
        {
            std::unique_lock<std::mutex> lock(ts.mutex);
            if (ts.cv.wait_until(lock, next, [&] { return ts.shutdown; }))
                break;
            params = ts.params;
        }
        auto now = std::chrono::steady_clock::now();
        int fps = std::clamp(params.fps, kResonanceFpsMin, kResonanceFpsMax);
        next += std::chrono::nanoseconds(1'000'000'000 / fps);
        if (next < now)
            next = now;

        float fade = 0.0f;
        if (first_frame_done) {
            float ft = std::chrono::duration<float, std::milli>(
                           now - state->fade_start)
                           .count() /
                       kOverlayFadeMs;
            ft = ft < 0.0f ? 0.0f : (ft > 1.0f ? 1.0f : ft);
            fade = applyEasing(Easing::EaseOutCubic, ft);
        }

        int width = state->base.width;
        int height = state->base.height;

        if (!init_ok) {
            clear_backbuffer(*state, width, height);
            continue;
        }

        bool modified = false;
        state->capture.take(audio_l, audio_r, modified);
        if (modified &&
            static_cast<int>(audio_l.size()) >= kResonanceFragmentSize &&
            static_cast<int>(audio_r.size()) >= kResonanceFragmentSize) {
            fft_l = audio_l;
            fft_r = audio_r;
            resonance_fft(fft_l.data(), kResonanceFragmentSize,
                          kResonanceFftScale, kResonanceFftCutOff);
            resonance_fft(fft_r.data(), kResonanceFragmentSize,
                          kResonanceFftScale, kResonanceFftCutOff);
            if (audio_size == 0)
                klog("resonance: first audio at frame %d", tick);
            audio_size = kResonanceFragmentSize;
        }

        tick++;
        bool trace = logged_frames < 40;
        auto t0 = std::chrono::steady_clock::now();
        if (audio_size > 0)
            stages->run(audio_size, fft_l, fft_r, fps);
        auto t1 = std::chrono::steady_clock::now();

        GLuint al = stages->ready() ? stages->smooth_l() : 0;
        GLuint ar = stages->ready() ? stages->smooth_r() : 0;
        blob->render(width, height, tick, fade, al, ar, stages->size(), params);
        glFinish();
        auto t2 = std::chrono::steady_clock::now();

        float render_ms =
            std::chrono::duration<float, std::milli>(t2 - t0).count();

        if (!eglSwapBuffers(state->base.egl_display, state->base.egl_surface))
            klog("resonance: eglSwapBuffers failed 0x%x", eglGetError());
        auto t3 = std::chrono::steady_clock::now();

        if (!first_frame_done) {
            first_frame_done = true;
            state->fade_start = t3;
            klog("resonance: first frame presented at %d (%.1fms)", tick,
                 render_ms);
        }

        if (trace) {
            gl_check("resonance render");
            klog("resonance: frame %d stages=%.1fms blob=%.1fms swap=%.1fms",
                 tick,
                 std::chrono::duration<float, std::milli>(t1 - t0).count(),
                 std::chrono::duration<float, std::milli>(t2 - t1).count(),
                 std::chrono::duration<float, std::milli>(t3 - t2).count());
            ++logged_frames;
        }

        ++heartbeat_frames;
        if (render_ms > heartbeat_blob_ms)
            heartbeat_blob_ms = render_ms;
        if (t3 - last_heartbeat >= std::chrono::seconds(1)) {
            klog("resonance: heartbeat tick=%d frames=%d fps=%.1f "
                 "worst=%.1fms fade=%.2f %dx%d",
                 tick, heartbeat_frames,
                 static_cast<float>(heartbeat_frames) /
                     std::chrono::duration<float>(t3 - last_heartbeat).count(),
                 heartbeat_blob_ms, fade, width, height);
            last_heartbeat = t3;
            heartbeat_frames = 0;
            heartbeat_blob_ms = 0.0f;
        }
    }

    stages->destroy();
    blob->destroy();
    stages.reset();
    blob.reset();
    gl_make_current(state->base.egl_display, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void resonance_render_thread_start(ResonanceState &state,
                                   const ResonanceParams &params) {
    static const EGLint kContextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    state.render_context =
        eglCreateContext(state.base.egl_display, state.egl_config,
                         state.base.egl_context, kContextAttribs);
    if (state.render_context == EGL_NO_CONTEXT) {
        klog("resonance: eglCreateContext failed 0x%x", eglGetError());
        return;
    }
    state.thread_state = std::make_unique<ResonanceRenderThreadState>();
    state.thread_state->params = params;
    state.render_thread = std::thread(render_thread_main, &state);
}

} // namespace

void resonance_apply_params(ResonanceState &state,
                            const ResonanceParams &params) {
    if (!state.thread_state)
        return;
    {
        std::lock_guard<std::mutex> lock(state.thread_state->mutex);
        state.thread_state->params = params;
    }
    state.thread_state->cv.notify_one();
}

void resonance_shutdown(ResonanceState &state) {
    state.capture.stop();

    if (state.thread_state) {
        {
            std::lock_guard<std::mutex> lock(state.thread_state->mutex);
            state.thread_state->shutdown = true;
        }
        state.thread_state->cv.notify_one();
    }
    if (state.render_thread.joinable())
        state.render_thread.join();
    if (state.render_context != EGL_NO_CONTEXT) {
        eglDestroyContext(state.base.egl_display, state.render_context);
        state.render_context = EGL_NO_CONTEXT;
    }
    state.thread_state.reset();
}

void resonance_toggle(ResonanceState &state, WaylandState &app) {
    if (state.base.egl_surface == EGL_NO_SURFACE) {
        if (!toplevel_window_create_surface(
                state.base, app.compositor, app.wm_base, "Visualizer",
                "kokusei-resonance", kResonanceDefaultWindow,
                kResonanceDefaultWindow))
            return;
        while (!state.base.configured)
            wl_display_dispatch(app.display);
        if (!toplevel_window_init_egl(state.base, app.egl_display,
                                      app.egl_config, app.egl_context)) {
            toplevel_window_destroy_surface(state.base);
            return;
        }
        state.base.frame_clock.surface = nullptr;
        state.base.on_close_request = [&state, &app] {
            resonance_toggle(state, app);
        };
        app_detail::rest_egl_current(app);

        state.egl_config = app.egl_config;
        state.base.open = true;
        state.fade_start = std::chrono::steady_clock::now();
        state.capture.start();
        resonance_render_thread_start(state, app.cfg.resonance);
        return;
    }

    state.base.open = false;
    resonance_shutdown(state);
    toplevel_window_destroy_surface(state.base);
    app_detail::rest_egl_current(app);
}

void resonance_handle_key_event(ResonanceState &state, WaylandState &app,
                                const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        resonance_toggle(state, app);
}

std::vector<IpcHandler> resonance_ipc_handlers(ResonanceState &resonance,
                                               WaylandState &state) {
    return {
        {"resonance",
         [&resonance, &state] { resonance_toggle(resonance, state); },
         "toggle the audio visualizer overlay"},
    };
}
