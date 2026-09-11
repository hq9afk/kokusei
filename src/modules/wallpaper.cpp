#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <random>
#include <thread>

#include "app/wayland_state.h"

#include "config/wallpaper_config.h"

#include "core/deferred_call.h"
#include "core/log.h"

#include "modules/wallpaper.h"

#include "render/animation.h"
#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/renderer.h"

#include "service/wallpaper_service.h"

namespace {

void wallpaper_paint(WallpaperState &wp);

AnimateFit to_fit(FillMode mode) {
    return mode == FillMode::Fit ? AnimateFit::Fit : AnimateFit::Crop;
}

void column_make_current(const WallpaperColumnGl &gl) {
    if (gl.surface == EGL_NO_SURFACE)
        return;
    auto t0 = std::chrono::steady_clock::now();
    gl_make_current(gl.display, gl.surface, gl.context);
    float ms = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
    if (ms > 5.0f)
        klog("wallpaper: column eglMakeCurrent %.1fms", ms);
}

void wallpaper_column_draw(const WallpaperColumn &col, Node *parent, float x,
                         float column_w, float height) {
    bool zero_copy = col.zero_copy && col.video_tex.tex;
    const Texture *tex = col.tex.id ? &col.tex : nullptr;
    if (!zero_copy && !tex)
        return;
    int tex_w = zero_copy ? col.video_tex.width : tex->width;
    int tex_h = zero_copy ? col.video_tex.height : tex->height;

    float scale = col.mode == FillMode::Fit
                      ? std::min(column_w / tex_w, height / tex_h)
                      : std::max(column_w / tex_w, height / tex_h);
    float draw_w = tex_w * scale;
    float draw_h = tex_h * scale;

    Node *clip = node_add_group(parent, x, 0.0f, column_w, height, true);
    Node *img = clip->claim_child();
    img->x = (column_w - draw_w) / 2.0f;
    img->y = (height - draw_h) / 2.0f;
    img->w = draw_w;
    img->h = draw_h;
    if (zero_copy) {
        img->kind = NodeKind::VideoTexture;
        img->video_tex = &col.video_tex;
    } else {
        img->kind = NodeKind::Texture;
        img->tex = tex;
    }
}

GLuint g_transition_prog[8] = {0};
bool g_transition_tried = false;

GLuint transition_program(WallpaperTransition kind) {
    if (!g_transition_tried) {
        g_transition_tried = true;
        struct {
            WallpaperTransition k;
            const char *frag;
            const char *tag;
        } defs[] = {
            {WallpaperTransition::Fade, "wallpaper/fade.frag", "wallpaper_fade"},
            {WallpaperTransition::Wipe, "wallpaper/wipe.frag", "wallpaper_wipe"},
            {WallpaperTransition::Disc, "wallpaper/disc.frag", "wallpaper_disc"},
            {WallpaperTransition::Stripes, "wallpaper/stripes.frag",
             "wallpaper_stripes"},
            {WallpaperTransition::Zoom, "wallpaper/zoom.frag", "wallpaper_zoom"},
            {WallpaperTransition::Honeycomb, "wallpaper/honeycomb.frag",
             "wallpaper_honeycomb"},
        };
        for (auto &d : defs)
            g_transition_prog[static_cast<int>(d.k)] = gl_compile_program_files(
                "renderer/quad.vert", d.frag, d.tag);
    }
    int i = static_cast<int>(kind);
    if (i < 0 || i >= 8)
        return 0;
    return g_transition_prog[i];
}

float rand_range(float lo, float hi) {
    static std::mt19937 rng{std::random_device{}()};
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

WallpaperTransition resolve_transition(WallpaperTransition kind) {
    if (kind != WallpaperTransition::Random)
        return kind;
    int pick = std::min(5, static_cast<int>(rand_range(0.0f, 6.0f)));
    return static_cast<WallpaperTransition>(
        static_cast<int>(WallpaperTransition::Fade) + pick);
}

void transition_begin(WallpaperColumn &col, WallpaperTransition kind) {
    col.transition_kind = resolve_transition(kind);
    col.transition_start = std::chrono::steady_clock::now();
    col.transitioning = true;
    switch (col.transition_kind) {
    case WallpaperTransition::Wipe:
        col.tr_direction = std::floor(rand_range(0.0f, 4.0f));
        break;
    case WallpaperTransition::Disc:
        col.tr_center_x = rand_range(0.2f, 0.8f);
        col.tr_center_y = rand_range(0.2f, 0.8f);
        break;
    case WallpaperTransition::Stripes:
        col.tr_stripe_count = std::round(rand_range(4.0f, 24.0f));
        col.tr_angle = rand_range(0.0f, 360.0f);
        break;
    case WallpaperTransition::Honeycomb:
        col.tr_cell_size = rand_range(0.02f, 0.06f);
        col.tr_center_x = rand_range(0.2f, 0.8f);
        col.tr_center_y = rand_range(0.2f, 0.8f);
        break;
    default:
        break;
    }
}

void transition_uv(FillMode mode, float col_w, float col_h, int tw, int th,
                   float out[4]) {
    float scale = mode == FillMode::Fit
                      ? std::min(col_w / tw, col_h / th)
                      : std::max(col_w / tw, col_h / th);
    float draw_w = tw * scale;
    float draw_h = th * scale;
    out[0] = col_w / draw_w;
    out[1] = col_h / draw_h;
    out[2] = -((col_w - draw_w) / 2.0f) / draw_w;
    out[3] = -((col_h - draw_h) / 2.0f) / draw_h;
}

void transition_render(Renderer &r, WallpaperColumn &col, float x, float y,
                       float w, float h, float progress) {
    GLuint prog = transition_program(col.transition_kind);
    if (!prog || !col.tex.id || !col.tex_prev.id)
        return;
    float from_uv[4];
    float to_uv[4];
    transition_uv(col.mode, w, h, col.tex_prev.width, col.tex_prev.height,
                  from_uv);
    transition_uv(col.mode, w, h, col.tex.width, col.tex.height, to_uv);
    const Color &fill = palette::base;
    float aspect = h > 0.0f ? w / h : 1.0f;
    r.draw_custom(prog, x, y, w, h, [&](GLuint p) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, col.tex_prev.id);
        glUniform1i(glGetUniformLocation(p, "u_from"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, col.tex.id);
        glUniform1i(glGetUniformLocation(p, "u_to"), 1);
        glUniform4fv(glGetUniformLocation(p, "u_from_uv"), 1, from_uv);
        glUniform4fv(glGetUniformLocation(p, "u_to_uv"), 1, to_uv);
        glUniform4f(glGetUniformLocation(p, "u_fill"), fill.r, fill.g, fill.b,
                    fill.a);
        glUniform1f(glGetUniformLocation(p, "u_progress"), progress);
        glUniform1f(glGetUniformLocation(p, "u_smoothness"),
                    kWallpaperTransitionSmoothness);
        glUniform1f(glGetUniformLocation(p, "u_direction"), col.tr_direction);
        glUniform2f(glGetUniformLocation(p, "u_center"), col.tr_center_x,
                    col.tr_center_y);
        glUniform1f(glGetUniformLocation(p, "u_aspect"), aspect);
        glUniform1f(glGetUniformLocation(p, "u_stripe_count"),
                    col.tr_stripe_count);
        glUniform1f(glGetUniformLocation(p, "u_angle"), col.tr_angle);
        glUniform1f(glGetUniformLocation(p, "u_cell_size"), col.tr_cell_size);
        glActiveTexture(GL_TEXTURE0);
    });
}

void wallpaper_column_upload_pending(WallpaperColumn &col,
                                   const WallpaperColumnGl &gl) {
    if (!col.pending_pixels || gl.surface == EGL_NO_SURFACE)
        return;
    auto t0 = std::chrono::steady_clock::now();
    column_make_current(gl);
    bool animated = col.decode.stop_flag != nullptr;
    if (col.pending_transition != WallpaperTransition::None && col.tex.id) {
        Texture fresh = make_texture_rgba(col.pending_width, col.pending_height,
                                          col.pending_pixels, true,
                                          col.pending_stride);
        if (fresh.id) {
            col.tex_prev = std::move(col.tex);
            col.tex = std::move(fresh);
            transition_begin(col, col.pending_transition);
        }
        col.pending_transition = WallpaperTransition::None;
        delete[] col.pending_pixels;
        col.pending_pixels = nullptr;
        if (gl.request_frame)
            gl.request_frame();
        return;
    }
    update_texture_rgba(col.tex, col.pending_width, col.pending_height,
                        col.pending_pixels, !animated, col.pending_stride);
    delete[] col.pending_pixels;
    col.pending_pixels = nullptr;
    if (gl.request_frame)
        gl.request_frame();
    float ms = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
    if (ms > 5.0f)
        klog("wallpaper: cpu upload %.1fms", ms);
}

void wallpaper_column_clear(WallpaperColumn &col, const WallpaperColumnGl &gl) {
    if (col.decode.stop_flag)
        col.decode.stop_flag->store(true);
    media_decode_stop(col.decode);
    col.path.clear();
    if (col.pinned_frame) {
        media_decode_release_drm_frame(col.pinned_frame);
        col.pinned_frame = nullptr;
    }
    if (col.pinned_frame_prev) {
        media_decode_release_drm_frame(col.pinned_frame_prev);
        col.pinned_frame_prev = nullptr;
    }
    if ((col.video_tex.tex || col.tex.id || col.tex_prev.id) &&
        gl.surface != EGL_NO_SURFACE)
        column_make_current(gl);
    col.video_tex.reset();
    col.tex.reset();
    col.tex_prev.reset();
    col.transitioning = false;
    col.pending_transition = WallpaperTransition::None;
    col.zero_copy = false;
    delete[] col.pending_pixels;
    col.pending_pixels = nullptr;
    ++col.generation;
    if (gl.request_frame)
        gl.request_frame();
}

void wallpaper_column_set_static(WallpaperColumn &col, const WallpaperColumnGl &gl,
                               const std::string &path, int target_w,
                               int target_h, FillMode mode) {
    if (col.path == path && col.mode == mode && col.target_w == target_w &&
        col.target_h == target_h && col.tex.id)
        return;
    col.pending_transition =
        col.tex.id ? WallpaperTransition::Random : WallpaperTransition::None;
    col.path = path;
    col.mode = mode;
    col.target_w = target_w;
    col.target_h = target_h;
    uint64_t gen = ++col.generation;
    std::weak_ptr<int> life = col.life;
    std::thread([&col, &gl, path, gen, target_w, target_h, life] {
        int w = 0, h = 0;
        unsigned char *data =
            animate_decode_scaled(path, target_w, target_h, w, h);
        DeferredCall::call_later([&col, &gl, data, w, h, gen, life] {
            auto keep = life.lock();
            if (!keep) {
                delete[] data;
                return;
            }
            if (gen != col.generation) {
                delete[] data;
                return;
            }
            if (!data)
                return;
            delete[] col.pending_pixels;
            col.pending_pixels = data;
            col.pending_width = w;
            col.pending_height = h;
            col.pending_stride = 0;
            wallpaper_column_upload_pending(col, gl);
        });
    }).detach();
}

void wallpaper_column_set_animated(WallpaperColumn &col, const WallpaperColumnGl &gl,
                                 const std::string &path, int target_w,
                                 int target_h, FillMode mode) {
    wallpaper_column_clear(col, gl);
    col.path = path;
    col.mode = mode;
    col.target_w = target_w;
    col.target_h = target_h;
    if (target_w <= 0 || target_h <= 0)
        return;
    uint64_t gen = ++col.generation;
    std::weak_ptr<int> life = col.life;

    klog("wallpaper: animated column start '%s' zero_copy_supported=%d "
         "surface=%d",
         path.c_str(), video_texture_import_supported() ? 1 : 0,
         gl.surface != EGL_NO_SURFACE ? 1 : 0);

    std::string filter = animate_scale_filter(target_w, target_h, to_fit(mode));

    col.decode = media_decode_stream(
        path, filter, kAnimateWallpaperFps, texture_row_length_supported(),
        [&col, &gl, gen, life](unsigned char *rgba, int w, int h,
                               int stride_px) {
            DeferredCall::call_later([&col, &gl, rgba, w, h, stride_px, gen,
                                     life] {
                auto keep = life.lock();
                if (!keep) {
                    delete[] rgba;
                    return;
                }
                if (gen != col.generation) {
                    delete[] rgba;
                    return;
                }
                delete[] col.pending_pixels;
                col.pending_pixels = rgba;
                col.pending_width = w;
                col.pending_height = h;
                col.pending_stride = stride_px;
                wallpaper_column_upload_pending(col, gl);
            });
        },
        video_texture_import_supported()
            ? MediaDecodeDrmFrameCallback([&col, &gl, gen,
                                           life](MediaDrmFrame frame) {
                  DeferredCall::call_later([&col, &gl, frame, gen, life] {
                      auto keep = life.lock();
                      if (!keep) {
                          media_decode_release_drm_frame(frame.avframe_handle);
                          return;
                      }
                      if (gen != col.generation ||
                          gl.surface == EGL_NO_SURFACE) {
                          media_decode_release_drm_frame(frame.avframe_handle);
                          return;
                      }
                      DrmFrameImport import;
                      import.plane_count = frame.plane_count;
                      import.width = frame.width;
                      import.height = frame.height;
                      for (int i = 0; i < frame.plane_count; ++i)
                          import.planes[i] = {
                              frame.planes[i].fd, frame.planes[i].modifier,
                              frame.planes[i].offset, frame.planes[i].pitch};
                      auto t0 = std::chrono::steady_clock::now();
                      column_make_current(gl);
                      bool ok = video_texture_import(col.video_tex, gl.display,
                                                     import);
                      float ms = std::chrono::duration<float, std::milli>(
                                     std::chrono::steady_clock::now() - t0)
                                     .count();
                      if (ms > 5.0f)
                          klog("wallpaper: zero-copy import %.1fms", ms);
                      if (ok) {
                          col.zero_copy = true;
                          if (col.pinned_frame_prev)
                              media_decode_release_drm_frame(
                                  col.pinned_frame_prev);
                          col.pinned_frame_prev = col.pinned_frame;
                          col.pinned_frame = frame.avframe_handle;
                          if (gl.request_frame)
                              gl.request_frame();
                      } else {
                          klog("wallpaper: video_texture_import failed "
                               "(%dx%d planes=%d), falling back to CPU upload",
                               frame.width, frame.height, frame.plane_count);
                          if (col.decode.egl_import_failed)
                              col.decode.egl_import_failed->store(true);
                          media_decode_release_drm_frame(frame.avframe_handle);
                      }
                  });
              })
            : MediaDecodeDrmFrameCallback());
}

MediaDecodeStatus wallpaper_column_decode_status(const WallpaperColumn &col) {
    return media_decode_status(col.decode);
}

void wallpaper_layer_surface_configure(void *data,
                                     zwlr_layer_surface_v1 *layer_surface,
                                     uint32_t serial, uint32_t width,
                                     uint32_t height) {
    auto *wp = static_cast<WallpaperState *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    bool changed = wp->width != static_cast<int32_t>(width) ||
                   wp->height != static_cast<int32_t>(height);
    wp->width = static_cast<int32_t>(width);
    wp->height = static_cast<int32_t>(height);
    if (changed && wp->egl_window) {
        int32_t scale = wp->output_scale.scale;
        wl_egl_window_resize(wp->egl_window, wp->width * scale,
                             wp->height * scale, 0, 0);
        if (wp->frame_clock.surface)
            request_frame(wp->frame_clock);
    }
    wp->configured = true;
    if (changed && wp->on_resize)
        wp->on_resize();
}

void wallpaper_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener wallpaper_layer_surface_listener = {
    .configure = wallpaper_layer_surface_configure,
    .closed = wallpaper_layer_surface_closed,
};

void wallpaper_draw_transitions(WallpaperState &wp) {
    if (wp.columns.empty())
        return;
    auto now = std::chrono::steady_clock::now();
    size_t ncol = std::max<size_t>(wp.columns.size(), 1);
    float column_w = static_cast<float>(wp.width) / static_cast<float>(ncol);
    bool any = false;
    for (size_t i = 0; i < wp.columns.size(); ++i) {
        auto &slot = wp.columns[i];
        if (!slot || !slot->transitioning)
            continue;
        WallpaperColumn &col = *slot;
        float raw = std::chrono::duration<float, std::milli>(
                        now - col.transition_start)
                        .count() /
                    kWallpaperTransitionDurationMs;
        if (raw >= 1.0f || col.transition_kind == WallpaperTransition::None) {
            col.tex_prev.reset();
            col.transitioning = false;
            continue;
        }
        any = true;
        transition_render(*wp.renderer, col,
                          static_cast<float>(i) * column_w, 0.0f, column_w,
                          static_cast<float>(wp.height),
                          applyEasing(Easing::EaseInOutCubic, raw));
    }
    if (any)
        wallpaper_request_frame(wp);
}

void wallpaper_paint(WallpaperState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE)
        return;
    if (wp.app && wp.app->session_locked) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            klog("wallpaper: paint suspended while session locked");
        }
        frame_clock_drop_callback(wp.frame_clock);
        return;
    }

    if (!gl_make_current(wp.egl_display, wp.egl_surface, wp.egl_context))
        return;
    wp.renderer->begin_frame(wp.width, wp.height, wp.output_scale.scale);
    glClearColor(palette::base.r, palette::base.g, palette::base.b,
                 palette::base.a);
    glClear(GL_COLOR_BUFFER_BIT);

    wp.scene.rebuild();
    wallpaper_draw_columns(wp, &wp.scene.root, wp.width, wp.height);
    wp.scene.draw(*wp.renderer);
    wallpaper_draw_transitions(wp);
    gl_check("wallpaper_paint");

    auto sw0 = std::chrono::steady_clock::now();
    if (!eglSwapBuffers(wp.egl_display, wp.egl_surface))
        klog("wallpaper: eglSwapBuffers failed, egl error 0x%04x", eglGetError());
    float sw = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - sw0)
                   .count();
    if (sw > 5.0f)
        klog("wallpaper: eglSwapBuffers %.1fms", sw);

    bool zero_copy = false;
    for (auto &c : wp.columns)
        if (c && c->zero_copy)
            zero_copy = true;
    if (++wp.dbg_frame % 60 == 0)
        klog("wallpaper: '%s' paint #%d zero_copy=%d", wp.output_name.c_str(),
             wp.dbg_frame, zero_copy);
}

} // namespace

void wallpaper_draw_columns(const WallpaperState &wp, Node *parent, int32_t width,
                          int32_t height) {
    size_t columns = std::max<size_t>(wp.columns.size(), 1);
    float column_w = static_cast<float>(width) / static_cast<float>(columns);
    for (size_t i = 0; i < wp.columns.size(); ++i)
        if (wp.columns[i])
            wallpaper_column_draw(*wp.columns[i], parent,
                                static_cast<float>(i) * column_w, column_w,
                                static_cast<float>(height));
}

bool wallpaper_create_surface(WallpaperState &wp, wl_compositor *compositor,
                            zwlr_layer_shell_v1 *layer_shell,
                            wl_output *output) {
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        .name_space = kWallpaperLayerNamespace,
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
    };
    wp.layer_surface =
        layer_surface_create(wp.surface, compositor, layer_shell, cfg,
                             &wallpaper_layer_surface_listener, &wp, output);
    if (!wp.layer_surface)
        return false;
    wp.output_scale.on_change = [&wp](int32_t scale) {
        if (wp.egl_window)
            wl_egl_window_resize(wp.egl_window, wp.width * scale,
                                 wp.height * scale, 0, 0);
        if (wp.frame_clock.surface)
            request_frame(wp.frame_clock);
        if (wp.on_resize)
            wp.on_resize();
    };
    output_scale_watch(wp.output_scale, wp.surface);
    wl_surface_commit(wp.surface);
    return true;
}

bool wallpaper_init_egl(WallpaperState &wp, Renderer &renderer, EGLDisplay display,
                      EGLConfig config, EGLContext context) {
    wp.egl_display = display;
    wp.egl_context = context;
    wp.renderer = &renderer;
    int32_t scale = wp.output_scale.scale;
    wp.egl_window =
        wl_egl_window_create(wp.surface, wp.width * scale, wp.height * scale);
    wp.egl_surface = eglCreateWindowSurface(
        display, config, reinterpret_cast<EGLNativeWindowType>(wp.egl_window),
        nullptr);
    if (wp.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, wp.egl_surface, context))
        return false;
    wp.gl.display = display;
    wp.gl.context = context;
    wp.gl.surface = wp.egl_surface;
    wp.gl.request_frame = [&wp] { wallpaper_request_frame(wp); };
    wp.frame_clock.surface = wp.surface;
    wp.frame_clock.draw = [&wp] { wallpaper_paint(wp); };
    return true;
}

void wallpaper_request_frame(WallpaperState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE || (wp.app && wp.app->session_locked))
        return;
    request_frame(wp.frame_clock);
}

void wallpaper_wake(WallpaperState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE)
        return;
    frame_clock_drop_callback(wp.frame_clock);
    request_frame(wp.frame_clock);
}

void wallpaper_columns_stop_all(WallpaperState &wp) {
    for (auto &col : wp.columns)
        if (col && col->decode.stop_flag)
            col->decode.stop_flag->store(true);
    for (auto &col : wp.columns)
        if (col)
            wallpaper_column_clear(*col, wp.gl);
}

void wallpaper_columns_pause_all(WallpaperState &wp) {
    for (auto &col : wp.columns)
        if (col)
            media_decode_pause(col->decode);
}

void wallpaper_columns_resume_all(WallpaperState &wp) {
    for (auto &col : wp.columns)
        if (col)
            media_decode_resume(col->decode);
}

MediaDecodeStatus wallpaper_column_status(const WallpaperState &wp,
                                        int column_index) {
    if (column_index < 0 ||
        static_cast<size_t>(column_index) >= wp.columns.size() ||
        !wp.columns[static_cast<size_t>(column_index)])
        return MediaDecodeStatus::Idle;
    return wallpaper_column_decode_status(
        *wp.columns[static_cast<size_t>(column_index)]);
}

void wallpaper_sync_from_config(WallpaperState &wp, const Config &cfg,
                              const std::string &monitor_name, bool animated) {
    int count = wallpaper_service_column_count(cfg, monitor_name, animated);

    if (!animated) {
        bool any_animated = false;
        for (auto &col : wp.columns)
            if (col && col->decode.stop_flag)
                any_animated = true;
        if (any_animated)
            wallpaper_columns_stop_all(wp);
    }

    for (size_t i = static_cast<size_t>(count); i < wp.columns.size(); ++i)
        if (wp.columns[i])
            wallpaper_column_clear(*wp.columns[i], wp.gl);
    wp.columns.resize(static_cast<size_t>(count));

    int target_w =
        (wp.width / std::max(1, count)) * std::max(1, wp.output_scale.scale);
    int target_h = wp.height * std::max(1, wp.output_scale.scale);
    AnimateSize sz =
        animated ? animate_decode_size(target_w, target_h, kAnimateMaxDecodeDim)
                 : AnimateSize{target_w, target_h};

    for (int i = 0; i < count; ++i) {
        auto &slot = wp.columns[static_cast<size_t>(i)];
        if (!slot)
            slot = std::make_unique<WallpaperColumn>();
        WallpaperColumn &col = *slot;

        std::string path =
            wallpaper_service_column_path(cfg, monitor_name, i, animated);
        FillMode mode =
            wallpaper_service_fill_mode(cfg, monitor_name, i, animated) == "fit"
                ? FillMode::Fit
                : FillMode::Crop;

        if (path.empty()) {
            wallpaper_column_clear(col, wp.gl);
            continue;
        }
        if (animated) {
            if (col.decode.stop_flag && col.path == path && col.mode == mode)
                continue;
            wallpaper_column_set_animated(col, wp.gl, path, sz.w, sz.h, mode);
        } else {
            wallpaper_column_set_static(col, wp.gl, path, sz.w, sz.h, mode);
        }
    }
}
