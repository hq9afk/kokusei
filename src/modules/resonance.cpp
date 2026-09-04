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

namespace {

void clear_backbuffer(ResonanceState &state, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(kResonanceWindowBackground.r, kResonanceWindowBackground.g,
                 kResonanceWindowBackground.b, kResonanceWindowBackground.a);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
}

void render_thread_main(ResonanceState *state) {
    if (!gl_make_current(state->base.egl_display, state->base.egl_surface,
                         state->render_context)) {
        klog("resonance: render thread eglMakeCurrent failed, eglGetError=0x%x",
             eglGetError());
        return;
    }
    glEnable(GL_BLEND);

    auto stages = std::make_unique<ResonanceAudioStages>();
    auto blob = std::make_unique<ResonanceBlobPipeline>();
    bool init_ok = stages->init() && blob->init();
    if (!init_ok)
        klog("resonance: pipeline init failed, showing cleared window");

    std::vector<float> fft_l;
    std::vector<float> fft_r;
    int audio_size = 0;

    ResonanceRenderThreadState &ts = *state->thread_state;
    for (;;) {
        ResonanceFrame frame;
        {
            std::unique_lock<std::mutex> lock(ts.mutex);
            ts.cv.wait(lock, [&] { return ts.have_frame || ts.shutdown; });
            if (ts.shutdown)
                break;
            frame = std::move(ts.pending);
            ts.have_frame = false;
        }

        if (!init_ok) {
            clear_backbuffer(*state, frame.width, frame.height);
            continue;
        }

        if (frame.step && frame.modified &&
            static_cast<int>(frame.l.size()) >= kResonanceFragmentSize &&
            static_cast<int>(frame.r.size()) >= kResonanceFragmentSize) {
            fft_l = frame.l;
            fft_r = frame.r;
            resonance_fft(fft_l.data(), kResonanceFragmentSize,
                          kResonanceFftScale, kResonanceFftCutOff);
            resonance_fft(fft_r.data(), kResonanceFragmentSize,
                          kResonanceFftScale, kResonanceFftCutOff);
            audio_size = kResonanceFragmentSize;
        }

        if (frame.step && audio_size > 0)
            stages->run(audio_size, fft_l, fft_r);

        GLuint al = stages->ready() ? stages->smooth_l() : 0;
        GLuint ar = stages->ready() ? stages->smooth_r() : 0;
        blob->render(frame.width, frame.height, frame.tick, frame.opacity, al,
                     ar, stages->size());

        eglSwapBuffers(state->base.egl_display, state->base.egl_surface);
    }

    stages->destroy();
    blob->destroy();
    stages.reset();
    blob.reset();
    gl_make_current(state->base.egl_display, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void resonance_render_thread_submit(ResonanceState &state, EGLConfig egl_config,
                                    bool step) {
    if (!state.thread_state) {
        static const EGLint kContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                                 EGL_CONTEXT_MINOR_VERSION, 2,
                                                 EGL_NONE};
        state.render_context =
            eglCreateContext(state.base.egl_display, egl_config,
                             state.base.egl_context, kContextAttribs);
        if (state.render_context == EGL_NO_CONTEXT)
            return;
        state.thread_state = std::make_unique<ResonanceRenderThreadState>();
        state.render_thread = std::thread(render_thread_main, &state);
    }

    ResonanceRenderThreadState &ts = *state.thread_state;
    std::lock_guard<std::mutex> lock(ts.mutex);
    ts.pending.width = state.base.width;
    ts.pending.height = state.base.height;
    ts.pending.scale = state.base.output_scale.scale;
    ts.pending.opacity = state.base.opacity;
    ts.pending.tick = state.tick;
    ts.pending.step = step;
    ts.pending.modified = state.pending_modified;
    if (state.pending_modified) {
        ts.pending.l = std::move(state.pending_l);
        ts.pending.r = std::move(state.pending_r);
    } else {
        ts.pending.l.clear();
        ts.pending.r.clear();
    }
    ts.have_frame = true;
    ts.cv.notify_one();
}

} // namespace

void resonance_request_frame(ResonanceState &state) {
    toplevel_window_request_frame(state.base);
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
    bool opening = !state.base.open;
    if (opening) {
        if (state.base.egl_surface == EGL_NO_SURFACE) {
            if (!toplevel_window_create_surface(
                    state.base, app.compositor, app.wm_base, "Visualizer",
                    "kokusei-resonance", kResonanceSphereCanvas,
                    kResonanceSphereCanvas))
                return;
            while (!state.base.configured)
                wl_display_dispatch(app.display);
            if (!toplevel_window_init_egl(state.base, app.egl_display,
                                          app.egl_config, app.egl_context)) {
                toplevel_window_destroy_surface(state.base);
                return;
            }
            state.base.frame_clock.draw = [&state, &app] {
                resonance_paint(state, app.egl_config);
            };
            state.base.on_close_request = [&state, &app] {
                resonance_toggle(state, app);
            };
            app_detail::rest_egl_current(app);
        }
        state.base.open = true;
        state.capture.start();
    }

    if (opening) {
        state.base.animations.animate(
            state.base.opacity, 1.0f, kOverlayFadeMs, Easing::EaseOutCubic,
            [&state](float v) { state.base.opacity = v; }, {},
            kOverlayFadeOwner);
        toplevel_window_request_frame(state.base);
    } else {
        state.base.animations.cancelForOwner(kOverlayFadeOwner);
        state.base.open = false;
        resonance_shutdown(state);
        toplevel_window_destroy_surface(state.base);
        app_detail::rest_egl_current(app);
    }
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

void resonance_paint(ResonanceState &state, EGLConfig egl_config) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);

    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;

    constexpr std::chrono::nanoseconds kStep{1'000'000'000 / kResonanceFps};
    if (state.last_step.time_since_epoch().count() == 0)
        state.last_step = now - kStep;
    bool step = now - state.last_step >= kStep;
    if (step) {
        state.last_step += kStep;
        if (now - state.last_step >= kStep)
            state.last_step = now;

        state.tick++;
        bool modified = false;
        state.capture.take(state.pending_l, state.pending_r, modified);
        state.pending_modified = modified;
    } else {
        state.pending_modified = false;
    }

    resonance_render_thread_submit(state, egl_config, step);

    if (state.base.open || state.base.animations.hasActive())
        toplevel_window_request_frame(state.base);
}
