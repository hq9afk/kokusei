#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>

#include "app/wayland_state.h"

#include "config/expanse_config.h"

#include "core/deferred_call.h"
#include "core/log.h"

#include "modules/expanse.h"

#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/renderer.h"

#include "service/expanse_service.h"

namespace {

void expanse_paint(ExpanseState &wp);

AnimateFit to_fit(FillMode mode) {
    return mode == FillMode::Fit ? AnimateFit::Fit : AnimateFit::Crop;
}

void column_make_current(const ExpanseColumnGl &gl) {
    if (gl.surface == EGL_NO_SURFACE)
        return;
    auto t0 = std::chrono::steady_clock::now();
    gl_make_current(gl.display, gl.surface, gl.context);
    float ms = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
    if (ms > 5.0f)
        klog("expanse: column eglMakeCurrent %.1fms", ms);
}

void expanse_column_draw(const ExpanseColumn &col, Node *parent, float x,
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

void expanse_column_upload_pending(ExpanseColumn &col,
                                   const ExpanseColumnGl &gl) {
    if (!col.pending_pixels || gl.surface == EGL_NO_SURFACE)
        return;
    auto t0 = std::chrono::steady_clock::now();
    column_make_current(gl);
    bool animated = col.decode.stop_flag != nullptr;
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
        klog("expanse: cpu upload %.1fms", ms);
}

void expanse_column_clear(ExpanseColumn &col, const ExpanseColumnGl &gl) {
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
    if ((col.video_tex.tex || col.tex.id) && gl.surface != EGL_NO_SURFACE)
        column_make_current(gl);
    col.video_tex.reset();
    col.tex.reset();
    col.zero_copy = false;
    delete[] col.pending_pixels;
    col.pending_pixels = nullptr;
    ++col.generation;
    if (gl.request_frame)
        gl.request_frame();
}

void expanse_column_set_static(ExpanseColumn &col, const ExpanseColumnGl &gl,
                               const std::string &path, int target_w,
                               int target_h, FillMode mode) {
    col.path = path;
    col.mode = mode;
    col.target_w = target_w;
    col.target_h = target_h;
    uint64_t gen = ++col.generation;
    std::thread([&col, &gl, path, gen, target_w, target_h] {
        int w = 0, h = 0;
        unsigned char *data =
            animate_decode_scaled(path, target_w, target_h, w, h);
        DeferredCall::call_later([&col, &gl, data, w, h, gen] {
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
            expanse_column_upload_pending(col, gl);
        });
    }).detach();
}

void expanse_column_set_animated(ExpanseColumn &col, const ExpanseColumnGl &gl,
                                 const std::string &path, int target_w,
                                 int target_h, FillMode mode) {
    expanse_column_clear(col, gl);
    col.path = path;
    col.mode = mode;
    col.target_w = target_w;
    col.target_h = target_h;
    if (target_w <= 0 || target_h <= 0)
        return;
    uint64_t gen = ++col.generation;

    klog("expanse: animated column start '%s' zero_copy_supported=%d "
         "surface=%d",
         path.c_str(), video_texture_import_supported() ? 1 : 0,
         gl.surface != EGL_NO_SURFACE ? 1 : 0);

    std::string filter = animate_scale_filter(target_w, target_h, to_fit(mode));

    col.decode = media_decode_stream(
        path, filter, kAnimateExpanseFps, texture_row_length_supported(),
        [&col, &gl, gen](unsigned char *rgba, int w, int h, int stride_px) {
            DeferredCall::call_later([&col, &gl, rgba, w, h, stride_px, gen] {
                if (gen != col.generation) {
                    delete[] rgba;
                    return;
                }
                delete[] col.pending_pixels;
                col.pending_pixels = rgba;
                col.pending_width = w;
                col.pending_height = h;
                col.pending_stride = stride_px;
                expanse_column_upload_pending(col, gl);
            });
        },
        video_texture_import_supported()
            ? MediaDecodeDrmFrameCallback([&col, &gl,
                                           gen](MediaDrmFrame frame) {
                  DeferredCall::call_later([&col, &gl, frame, gen] {
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
                          klog("expanse: zero-copy import %.1fms", ms);
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
                          klog("expanse: video_texture_import failed "
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

MediaDecodeStatus expanse_column_decode_status(const ExpanseColumn &col) {
    return media_decode_status(col.decode);
}

void expanse_layer_surface_configure(void *data,
                                     zwlr_layer_surface_v1 *layer_surface,
                                     uint32_t serial, uint32_t width,
                                     uint32_t height) {
    auto *wp = static_cast<ExpanseState *>(data);
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

void expanse_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener expanse_layer_surface_listener = {
    .configure = expanse_layer_surface_configure,
    .closed = expanse_layer_surface_closed,
};

void expanse_paint(ExpanseState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE)
        return;
    if (wp.app && wp.app->session_locked) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            klog("expanse: paint suspended while session locked");
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
    expanse_draw_columns(wp, &wp.scene.root, wp.width, wp.height);
    wp.scene.draw(*wp.renderer);
    gl_check("expanse_paint");

    auto sw0 = std::chrono::steady_clock::now();
    if (!eglSwapBuffers(wp.egl_display, wp.egl_surface))
        klog("expanse: eglSwapBuffers failed, egl error 0x%04x", eglGetError());
    float sw = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - sw0)
                   .count();
    if (sw > 5.0f)
        klog("expanse: eglSwapBuffers %.1fms", sw);

    bool zero_copy = false;
    for (auto &c : wp.columns)
        if (c && c->zero_copy)
            zero_copy = true;
    if (++wp.dbg_frame % 60 == 0)
        klog("expanse: '%s' paint #%d zero_copy=%d", wp.output_name.c_str(),
             wp.dbg_frame, zero_copy);
}

} // namespace

void expanse_draw_columns(const ExpanseState &wp, Node *parent, int32_t width,
                          int32_t height) {
    size_t columns = std::max<size_t>(wp.columns.size(), 1);
    float column_w = static_cast<float>(width) / static_cast<float>(columns);
    for (size_t i = 0; i < wp.columns.size(); ++i)
        if (wp.columns[i])
            expanse_column_draw(*wp.columns[i], parent,
                                static_cast<float>(i) * column_w, column_w,
                                static_cast<float>(height));
}

bool expanse_create_surface(ExpanseState &wp, wl_compositor *compositor,
                            zwlr_layer_shell_v1 *layer_shell,
                            wl_output *output) {
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        .name_space = kExpanseLayerNamespace,
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
    };
    wp.layer_surface =
        layer_surface_create(wp.surface, compositor, layer_shell, cfg,
                             &expanse_layer_surface_listener, &wp, output);
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

bool expanse_init_egl(ExpanseState &wp, Renderer &renderer, EGLDisplay display,
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
    wp.gl.request_frame = [&wp] { expanse_request_frame(wp); };
    wp.frame_clock.surface = wp.surface;
    wp.frame_clock.draw = [&wp] { expanse_paint(wp); };
    return true;
}

void expanse_request_frame(ExpanseState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE || (wp.app && wp.app->session_locked))
        return;
    request_frame(wp.frame_clock);
}

void expanse_wake(ExpanseState &wp) {
    if (wp.egl_surface == EGL_NO_SURFACE)
        return;
    frame_clock_drop_callback(wp.frame_clock);
    request_frame(wp.frame_clock);
}

void expanse_columns_stop_all(ExpanseState &wp) {
    for (auto &col : wp.columns)
        if (col && col->decode.stop_flag)
            col->decode.stop_flag->store(true);
    for (auto &col : wp.columns)
        if (col)
            expanse_column_clear(*col, wp.gl);
}

void expanse_columns_pause_all(ExpanseState &wp) {
    for (auto &col : wp.columns)
        if (col)
            media_decode_pause(col->decode);
}

void expanse_columns_resume_all(ExpanseState &wp) {
    for (auto &col : wp.columns)
        if (col)
            media_decode_resume(col->decode);
}

MediaDecodeStatus expanse_column_status(const ExpanseState &wp,
                                        int column_index) {
    if (column_index < 0 ||
        static_cast<size_t>(column_index) >= wp.columns.size() ||
        !wp.columns[static_cast<size_t>(column_index)])
        return MediaDecodeStatus::Blink;
    return expanse_column_decode_status(
        *wp.columns[static_cast<size_t>(column_index)]);
}

void expanse_sync_from_config(ExpanseState &wp, const Config &cfg,
                              const std::string &monitor_name, bool animated) {
    int count = expanse_service_column_count(cfg, monitor_name, animated);

    if (!animated) {
        bool any_animated = false;
        for (auto &col : wp.columns)
            if (col && col->decode.stop_flag)
                any_animated = true;
        if (any_animated)
            expanse_columns_stop_all(wp);
    }

    for (size_t i = static_cast<size_t>(count); i < wp.columns.size(); ++i)
        if (wp.columns[i])
            expanse_column_clear(*wp.columns[i], wp.gl);
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
            slot = std::make_unique<ExpanseColumn>();
        ExpanseColumn &col = *slot;

        std::string path =
            expanse_service_column_path(cfg, monitor_name, i, animated);
        FillMode mode =
            expanse_service_fill_mode(cfg, monitor_name, i, animated) == "fit"
                ? FillMode::Fit
                : FillMode::Crop;

        if (path.empty()) {
            expanse_column_clear(col, wp.gl);
            continue;
        }
        if (animated) {
            if (col.decode.stop_flag && col.path == path && col.mode == mode)
                continue;
            expanse_column_set_animated(col, wp.gl, path, sz.w, sz.h, mode);
        } else {
            expanse_column_set_static(col, wp.gl, path, sz.w, sz.h, mode);
        }
    }
}
