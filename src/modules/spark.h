#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "config/spark_config.h"

#include "render/animation.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture.h"

#include "service/frame_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

enum class SparkKind { Volume, Mic, Brightness };

struct SparkState {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    bool configured = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;

    SparkKind kind = SparkKind::Volume;
    float level = 0.0f;
    bool muted = false;
    Texture icon_texture;
    Texture label_texture;
    bool visible = false;
    std::chrono::steady_clock::time_point hide_at;

    AnimationManager animations;
    float opacity = 0.0f;
    float qixing_fill = 0.0f;
    float icon_color_t = 0.0f;
    std::chrono::steady_clock::time_point created_at =
        std::chrono::steady_clock::now();
};

bool spark_create_surface(SparkState &state, wl_compositor *compositor,
                          zwlr_layer_shell_v1 *layer_shell,
                          wl_output *output = nullptr);

bool spark_init_egl(SparkState &state, Renderer &renderer, EGLDisplay display,
                    EGLConfig config, EGLContext context);

void spark_request_frame(SparkState &state);

void spark_show(SparkState &state, SparkKind kind, float level, bool muted);

void spark_hide(SparkState &state);
