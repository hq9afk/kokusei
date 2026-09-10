#include <algorithm>
#include <chrono>
#include <vector>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "config/visualizer_config.h"

#include "core/log.h"

#include "modules/visualizer.h"
#include "modules/visualizer/audio_stages.h"
#include "modules/visualizer/bar_visualizer.h"
#include "modules/visualizer/fft.h"
#include "modules/visualizer/sphere_visualizer.h"

#include "render/gl.h"
#include "render/overlay_panel.h"
#include "render/palette.h"

namespace {

void clear_backbuffer(VisualizerState &state, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(palette::window_backdrop.r, palette::window_backdrop.g,
                 palette::window_backdrop.b, palette::window_backdrop.a);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!eglSwapBuffers(state.base.egl_display, state.base.egl_surface))
        klog("visualizer: eglSwapBuffers failed 0x%x", eglGetError());
}

void render_thread_main(VisualizerState *state) {
    if (!gl_make_current(state->base.egl_display, state->base.egl_surface,
                         state->render_context)) {
        klog("visualizer: render thread eglMakeCurrent failed, eglGetError=0x%x",
             eglGetError());
        return;
    }
    glEnable(GL_BLEND);
    klog("visualizer: render thread current %dx%d scale %d", state->base.width,
         state->base.height, state->base.output_scale.scale);

    auto stages = std::make_unique<VisualizerAudioStages>();
    auto sphere = std::make_unique<SphereVisualizer>();
    auto bar = std::make_unique<BarVisualizer>();
    bool stages_ok = stages->init();
    bool sphere_ok = false;
    bool bar_ok = false;
    bool sphere_tried = false;
    bool bar_tried = false;
    klog("visualizer: audio stages init %s",
         stages_ok ? "ok" : "FAILED, showing cleared window");

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
    float heartbeat_draw_ms = 0.0f;

    auto next = std::chrono::steady_clock::now();

    VisualizerRenderThreadState &ts = *state->thread_state;
    for (;;) {
        VisualizerParams params;
        {
            std::unique_lock<std::mutex> lock(ts.mutex);
            if (ts.cv.wait_until(lock, next, [&] { return ts.shutdown; }))
                break;
            params = ts.params;
        }
        auto now = std::chrono::steady_clock::now();
        bool want_sphere =
            params.visualizer_shape == VisualizerShape::Sphere;
        int fps = std::clamp(want_sphere ? params.fps : kVisualizerBarFps,
                             kVisualizerFpsMin, kVisualizerFpsMax);
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

        if (want_sphere && !sphere_tried) {
            sphere_tried = true;
            sphere_ok = sphere->init();
            klog("visualizer: sphere renderer init %s",
                 sphere_ok ? "ok" : "FAILED");
        } else if (!want_sphere && !bar_tried) {
            bar_tried = true;
            bar_ok = bar->init();
            klog("visualizer: bar renderer init %s", bar_ok ? "ok" : "FAILED");
        }
        if (!stages_ok || (want_sphere ? !sphere_ok : !bar_ok)) {
            clear_backbuffer(*state, width, height);
            continue;
        }

        bool modified = false;
        state->capture.take(audio_l, audio_r, modified);
        if (modified &&
            static_cast<int>(audio_l.size()) >= kVisualizerFragmentSize &&
            static_cast<int>(audio_r.size()) >= kVisualizerFragmentSize) {
            fft_l = audio_l;
            fft_r = audio_r;
            visualizer_fft(fft_l.data(), kVisualizerFragmentSize,
                          kVisualizerFftScale, kVisualizerFftCutOff);
            visualizer_fft(fft_r.data(), kVisualizerFragmentSize,
                          kVisualizerFftScale, kVisualizerFftCutOff);
            if (audio_size == 0)
                klog("visualizer: first audio at frame %d", tick);
            audio_size = kVisualizerFragmentSize;
        }

        tick++;
        bool trace = logged_frames < 40;
        auto t0 = std::chrono::steady_clock::now();
        if (audio_size > 0)
            stages->run(audio_size, fft_l, fft_r, fps);
        auto t1 = std::chrono::steady_clock::now();

        GLuint al = stages->ready() ? stages->smooth_l() : 0;
        GLuint ar = stages->ready() ? stages->smooth_r() : 0;
        if (want_sphere)
            sphere->render(width, height, tick, fade, al, ar, stages->size(),
                           params);
        else
            bar->render(width, height, tick, fade, al, ar, stages->size(),
                        params);
        glFinish();
        auto t2 = std::chrono::steady_clock::now();

        float render_ms =
            std::chrono::duration<float, std::milli>(t2 - t0).count();

        if (!eglSwapBuffers(state->base.egl_display, state->base.egl_surface))
            klog("visualizer: eglSwapBuffers failed 0x%x", eglGetError());
        auto t3 = std::chrono::steady_clock::now();

        if (!first_frame_done) {
            first_frame_done = true;
            state->fade_start = t3;
            klog("visualizer: first frame presented at %d (%.1fms)", tick,
                 render_ms);
        }

        if (trace) {
            gl_check("visualizer render");
            klog("visualizer: frame %d stages=%.1fms draw=%.1fms swap=%.1fms",
                 tick,
                 std::chrono::duration<float, std::milli>(t1 - t0).count(),
                 std::chrono::duration<float, std::milli>(t2 - t1).count(),
                 std::chrono::duration<float, std::milli>(t3 - t2).count());
            ++logged_frames;
        }

        ++heartbeat_frames;
        if (render_ms > heartbeat_draw_ms)
            heartbeat_draw_ms = render_ms;
        if (t3 - last_heartbeat >= std::chrono::seconds(1)) {
            klog("visualizer: heartbeat tick=%d frames=%d fps=%.1f "
                 "worst=%.1fms fade=%.2f %dx%d",
                 tick, heartbeat_frames,
                 static_cast<float>(heartbeat_frames) /
                     std::chrono::duration<float>(t3 - last_heartbeat).count(),
                 heartbeat_draw_ms, fade, width, height);
            last_heartbeat = t3;
            heartbeat_frames = 0;
            heartbeat_draw_ms = 0.0f;
        }
    }

    stages->destroy();
    sphere->destroy();
    bar->destroy();
    stages.reset();
    sphere.reset();
    bar.reset();
    gl_make_current(state->base.egl_display, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void visualizer_render_thread_start(VisualizerState &state,
                                   const VisualizerParams &params) {
    static const EGLint kContextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    state.render_context =
        eglCreateContext(state.base.egl_display, state.egl_config,
                         state.base.egl_context, kContextAttribs);
    if (state.render_context == EGL_NO_CONTEXT) {
        klog("visualizer: eglCreateContext failed 0x%x", eglGetError());
        return;
    }
    state.thread_state = std::make_unique<VisualizerRenderThreadState>();
    state.thread_state->params = params;
    state.render_thread = std::thread(render_thread_main, &state);
}

} // namespace

void visualizer_apply_params(VisualizerState &state,
                            const VisualizerParams &params) {
    if (!state.thread_state)
        return;
    {
        std::lock_guard<std::mutex> lock(state.thread_state->mutex);
        state.thread_state->params = params;
    }
    state.thread_state->cv.notify_one();
}

void visualizer_shutdown(VisualizerState &state) {
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

void visualizer_toggle(VisualizerState &state, WaylandState &app) {
    if (state.base.egl_surface == EGL_NO_SURFACE) {
        if (!toplevel_window_create_surface(
                state.base, app.compositor, app.wm_base, "Visualizer",
                "kokusei-visualizer", kVisualizerDefaultWindow,
                kVisualizerDefaultWindow))
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
            visualizer_toggle(state, app);
        };
        app_detail::rest_egl_current(app);

        state.egl_config = app.egl_config;
        state.base.open = true;
        state.fade_start = std::chrono::steady_clock::now();
        state.capture.start();
        visualizer_render_thread_start(state, app.cfg.visualizer);
        return;
    }

    state.base.open = false;
    visualizer_shutdown(state);
    toplevel_window_destroy_surface(state.base);
    app_detail::rest_egl_current(app);
}

void visualizer_handle_key_event(VisualizerState &state, WaylandState &app,
                                const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        visualizer_toggle(state, app);
}

std::vector<IpcHandler> visualizer_ipc_handlers(VisualizerState &visualizer,
                                               WaylandState &state) {
    return {
        {"visualizer",
         [&visualizer, &state] { visualizer_toggle(visualizer, state); },
         "toggle the audio visualizer overlay"},
    };
}
