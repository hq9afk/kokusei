#include <algorithm>
#include <cmath>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "config/resonance_config.h"

#include "core/log.h"

#include "modules/resonance.h"

#include "render/color_ops.h"
#include "render/node.h"
#include "render/overlay_panel.h"

namespace {

constexpr Color kQixingColor =
    with_alpha(palette::accent, kResonanceBarOpacity);

int bar_visualizer_compute_qixing_count(int width) {
    return std::max(
        1, static_cast<int>((static_cast<float>(width) - kResonanceBarSpacing) /
                            (kResonanceBarWidth + kResonanceBarSpacing)));
}

void bar_visualizer_render(BarVisualizerState &state, int width, int height,
                           int32_t scale, float opacity, float elapsed_ms,
                           const std::vector<float> &spectrum) {
    if (!state.ready)
        state.ready = state.renderer.init();
    if (!state.ready)
        return;

    state.renderer.begin_frame(width, height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    float win_w = static_cast<float>(width);
    float win_h = static_cast<float>(height);

    node_add_rect(&state.scene.root, 0.0f, 0.0f, win_w, win_h,
                  rgba(kResonanceWindowBackground));

    float k = 1.0f - std::exp(-std::max(0.0f, elapsed_ms) /
                              kResonanceBarsAnimDurationMs);

    state.display_values.resize(spectrum.size(), 0.0f);
    int qixing_count = static_cast<int>(spectrum.size());

    float total_w = qixing_count * kResonanceBarWidth +
                    (qixing_count - 1) * kResonanceBarSpacing;
    float start_x = (win_w - total_w) / 2.0f;
    float baseline_y = win_h;

    for (int i = 0; i < qixing_count; ++i) {
        float &v = state.display_values[static_cast<size_t>(i)];
        v += (spectrum[static_cast<size_t>(i)] - v) * k;
        float qixing_h = std::max(1.0f, v * kResonanceBarHeightRatio * win_h);
        float qixing_x =
            start_x + i * (kResonanceBarWidth + kResonanceBarSpacing);
        node_add_rrect(&state.scene.root, qixing_x, baseline_y - qixing_h,
                       kResonanceBarWidth, qixing_h, kResonanceBarRadius, 0.0f,
                       rgba(kQixingColor), kNodeTransparent);
    }

    state.renderer.set_opacity(opacity);
    state.scene.draw(state.renderer);
    state.renderer.set_opacity(1.0f);
}

void bar_visualizer_destroy_gl(BarVisualizerState &state) {
    state.renderer.destroy();
    state = BarVisualizerState{};
}

void render_thread_main(ResonanceState *state) {
    if (!eglMakeCurrent(state->base.egl_display, state->base.egl_surface,
                        state->base.egl_surface, state->render_context)) {
        klog(
            "visualizer: render thread eglMakeCurrent failed, eglGetError=0x%x",
            eglGetError());
        return;
    }
    glEnable(GL_BLEND);

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

        bar_visualizer_render(state->qixing, frame.width, frame.height,
                              frame.scale, frame.opacity, frame.elapsed_ms,
                              frame.spectrum);
        eglSwapBuffers(state->base.egl_display, state->base.egl_surface);
    }

    bar_visualizer_destroy_gl(state->qixing);
    eglMakeCurrent(state->base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
}

void resonance_render_thread_submit(ResonanceState &state, EGLConfig egl_config,
                                    float elapsed_ms) {
    if (!state.thread_state) {
        static const EGLint kContextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
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
    ts.pending.elapsed_ms = elapsed_ms;
    ts.pending.spectrum = state.spectrum.values();
    ts.have_frame = true;
    ts.cv.notify_one();
}

void retarget_spectrum(ResonanceState &state, WaylandState &app) {
    uint32_t sink_id = app.pipewire.default_sink_id;
    auto it = app.pipewire.nodes.find(sink_id);
    std::string sink_name =
        it != app.pipewire.nodes.end() ? it->second.name : "";
    state.spectrum.setTargetNode(sink_id, sink_name);
}

} // namespace

void resonance_request_frame(ResonanceState &state) {
    toplevel_window_request_frame(state.base);
}

void resonance_shutdown(ResonanceState &state) {
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
                    "kokusei-resonance", kResonanceDefaultWindowWidth,
                    kResonanceDefaultWindowHeight))
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
            if (!state.spectrum_ready)
                state.spectrum_ready = state.spectrum.init();
            state.base.on_close_request = [&state, &app] {
                resonance_toggle(state, app);
            };
        }
        state.base.open = true;
        retarget_spectrum(state, app);
        state.last_frame = std::chrono::steady_clock::now();
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

    state.spectrum.setBarCount(
        bar_visualizer_compute_qixing_count(state.base.width));
    state.spectrum.processFrame();

    float elapsed_ms =
        std::chrono::duration<float, std::milli>(now - state.last_frame)
            .count();

    resonance_render_thread_submit(state, egl_config, elapsed_ms);

    state.last_frame = now;

    if (state.base.open || state.base.animations.hasActive())
        toplevel_window_request_frame(state.base);
}
