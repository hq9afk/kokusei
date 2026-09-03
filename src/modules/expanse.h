#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "app/config.h"

#include "render/scene.h"
#include "render/texture.h"
#include "render/video_texture.h"

#include "service/frame_service.h"
#include "service/media_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

class Renderer;
struct Node;
struct WaylandState;

enum class FillMode { Crop, Fit };

struct ExpanseColumnGl {
    EGLDisplay display = nullptr;
    EGLContext context = nullptr;
    EGLSurface surface = EGL_NO_SURFACE;
    std::function<void()> request_frame;
};

struct ExpanseColumn {
    Texture tex;
    uint64_t generation = 0;

    unsigned char *pending_pixels = nullptr;
    int pending_width = 0;
    int pending_height = 0;
    int pending_stride = 0;

    MediaDecodePlayback decode;
    std::string path;
    FillMode mode = FillMode::Crop;
    VideoTexture video_tex;
    void *pinned_frame = nullptr;
    void *pinned_frame_prev = nullptr;
    bool zero_copy = false;

    int target_w = 0;
    int target_h = 0;

    ExpanseColumn() = default;
    ExpanseColumn(const ExpanseColumn &) = delete;
    ExpanseColumn &operator=(const ExpanseColumn &) = delete;
    ~ExpanseColumn() { delete[] pending_pixels; }
};

struct ExpanseState {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    WaylandState *app = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    bool configured = false;
    std::string output_name;
    int dbg_frame = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;

    ExpanseColumnGl gl;
    std::vector<std::unique_ptr<ExpanseColumn>> columns;

    std::function<void()> on_resize;
};

bool expanse_create_surface(ExpanseState &wp, wl_compositor *compositor,
                            zwlr_layer_shell_v1 *layer_shell,
                            wl_output *output = nullptr);

bool expanse_init_egl(ExpanseState &wp, Renderer &renderer, EGLDisplay display,
                      EGLConfig config, EGLContext context);

void expanse_request_frame(ExpanseState &wp);

void expanse_wake(ExpanseState &wp);

void expanse_draw_columns(const ExpanseState &wp, Node *parent, int32_t width,
                          int32_t height);

void expanse_sync_from_config(ExpanseState &wp, const Config &cfg,
                              const std::string &monitor_name, bool animated);

void expanse_columns_stop_all(ExpanseState &wp);
void expanse_columns_pause_all(ExpanseState &wp);
void expanse_columns_resume_all(ExpanseState &wp);

MediaDecodeStatus expanse_column_status(const ExpanseState &wp,
                                        int column_index);
