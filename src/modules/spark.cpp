#include <GLES2/gl2.h>
#include <algorithm>
#include <cmath>

#include "modules/spark.h"

#include "render/color_ops.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/progress_bar.h"
#include "render/text.h"

namespace {

void spark_layer_surface_configure(void *data,
                                   zwlr_layer_surface_v1 *layer_surface,
                                   uint32_t serial, uint32_t, uint32_t) {
    auto *state = static_cast<SparkState *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    state->configured = true;
}

void spark_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener spark_layer_surface_listener = {
    .configure = spark_layer_surface_configure,
    .closed = spark_layer_surface_closed,
};

void spark_paint(SparkState &state) {
    eglMakeCurrent(state.egl_display, state.egl_surface, state.egl_surface,
                   state.egl_context);
    int32_t scale = state.output_scale.scale;
    state.renderer->begin_frame(kSparkSurfaceWidth, kSparkSurfaceHeight, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.animations.tick(std::chrono::steady_clock::now());

    Color icon_color;

    state.scene.rebuild();
    if (state.opacity > 0.0f) {
        Node *bg = state.scene.root.claim_child();
        bg->kind = NodeKind::RoundedRect;
        bg->w = kSparkSurfaceWidth;
        bg->h = kSparkSurfaceHeight;
        bg->radius = kSparkSurfaceHeight / 2.0f;
        bg->border_width = metrics::border_thin;
        bg->fill = rgba(palette::overlay);
        bg->border = rgba(palette::electro);

        float icon_y = (kSparkSurfaceHeight - KOKUSEI_ICON_PX) / 2.0f;
        if (state.icon_texture.id) {
            icon_color = lerp_color(palette::text, palette::text_muted,
                                    state.icon_color_t);
            Node *icon = state.scene.root.claim_child();
            icon->kind = NodeKind::Texture;
            icon->x = kSparkContentMargin;
            icon->y = icon_y;
            icon->w = static_cast<float>(state.icon_texture.width) /
                      static_cast<float>(state.icon_texture.scale);
            icon->h = static_cast<float>(state.icon_texture.height) /
                      static_cast<float>(state.icon_texture.scale);
            icon->tex = &state.icon_texture;
            icon->tint = rgba(icon_color);
        }

        float qixing_x =
            kSparkContentMargin + (state.icon_texture.id
                                       ? KOKUSEI_ICON_PX + kSparkQixingMargin
                                       : 0.0f);
        float qixing_w = kSparkSurfaceWidth - qixing_x - kSparkQixingMargin -
                         kSparkLabelWidth - kSparkContentMargin;
        float qixing_y = kSparkSurfaceHeight / 2.0f - 3;

        const Color &fill_color =
            state.muted ? palette::text_muted : palette::accent;
        draw_flat_bar(&state.scene.root, qixing_x, qixing_y, qixing_w, 6.0f,
                      3.0f, state.qixing_fill, 0.0f,
                      rgba(palette::text_alpha11), rgba(fill_color));

        if (state.label_texture.id) {
            float label_h = static_cast<float>(state.label_texture.height) /
                            static_cast<float>(state.label_texture.scale);
            float label_w = static_cast<float>(state.label_texture.width) /
                            static_cast<float>(state.label_texture.scale);
            Node *label = state.scene.root.claim_child();
            label->kind = NodeKind::Texture;
            label->x = kSparkSurfaceWidth - kSparkContentMargin - label_w;
            label->y = (kSparkSurfaceHeight - label_h) / 2.0f;
            label->w = label_w;
            label->h = label_h;
            label->tex = &state.label_texture;
            label->tint = rgba(palette::text);
        }
    }

    state.renderer->set_opacity(state.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.egl_display, state.egl_surface);

    if (state.animations.hasActive())
        request_frame(state.frame_clock);
}

PangoFontDescription *spark_label_font() {
    static PangoFontDescription *d =
        pango_font_description_from_string("ComicShannsMono Nerd Font 15");
    return d;
}

} // namespace

bool spark_create_surface(SparkState &state, wl_compositor *compositor,
                          zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        .name_space = "kokusei-spark",

        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
        .width = kSparkSurfaceWidth,
        .height = kSparkSurfaceHeight,
        .margin_bottom = 30,
        .empty_input_region = true,
    };
    state.layer_surface =
        layer_surface_create(state.surface, compositor, layer_shell, cfg,
                             &spark_layer_surface_listener, &state, output);
    if (!state.layer_surface)
        return false;
    state.output_scale.on_change = [&state](int32_t scale) {
        if (state.egl_window)
            wl_egl_window_resize(state.egl_window, kSparkSurfaceWidth * scale,
                                 kSparkSurfaceHeight * scale, 0, 0);
        if (state.frame_clock.surface)
            request_frame(state.frame_clock);
    };
    output_scale_watch(state.output_scale, state.surface);
    wl_surface_commit(state.surface);
    return true;
}

bool spark_init_egl(SparkState &state, Renderer &renderer, EGLDisplay display,
                    EGLConfig config, EGLContext context) {
    state.egl_display = display;
    state.egl_context = context;
    state.renderer = &renderer;
    int32_t scale = state.output_scale.scale;
    state.egl_window = wl_egl_window_create(
        state.surface, kSparkSurfaceWidth * scale, kSparkSurfaceHeight * scale);
    state.egl_surface = eglCreateWindowSurface(
        display, config,
        reinterpret_cast<EGLNativeWindowType>(state.egl_window), nullptr);
    if (state.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!eglMakeCurrent(display, state.egl_surface, state.egl_surface, context))
        return false;
    state.frame_clock.surface = state.surface;
    state.frame_clock.draw = [&state] { spark_paint(state); };
    return true;
}

void spark_request_frame(SparkState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(state.frame_clock);
}

void spark_show(SparkState &state, SparkKind kind, float level, bool muted) {
    auto now = std::chrono::steady_clock::now();
    if (now - state.created_at < kSparkReadyDelay)
        return;

    state.kind = kind;
    state.level = level;
    state.muted = muted;
    state.visible = true;
    state.hide_at = now + kSparkVisibleFor;

    const char *codepoint;
    if (kind == SparkKind::Brightness) {
        codepoint = icon::adjustments;
    } else if (kind == SparkKind::Mic) {
        codepoint = muted ? icon::mic_off : icon::mic_on;
    } else if (muted) {
        codepoint = icon::volume_mute;
    } else if (level < 0.01f) {
        codepoint = icon::volume_empty;
    } else if (level < 0.5f) {
        codepoint = icon::volume_low;
    } else {
        codepoint = icon::volume_high;
    }
    RasterizedText icon_text =
        rasterize_icon(codepoint, state.output_scale.scale);
    state.icon_texture = make_texture_from_raster(icon_text);

    std::string label_str =
        muted
            ? "muted"
            : std::to_string(static_cast<int>(std::lround(level * 100))) + "%";
    RasterizedText label_text =
        rasterize_text_with(label_str, spark_label_font(),
                            state.output_scale.scale, kSparkLabelWidth);
    state.label_texture = make_texture_from_raster(label_text);

    state.animations.animate(
        state.opacity, 1.0f, kSparkAnimNormal, Easing::EaseOutCubic,
        [&state](float v) { state.opacity = v; }, {}, kSparkOwnerOpacity);
    state.animations.animate(
        state.qixing_fill, std::clamp(level, 0.0f, 1.0f), kSparkAnimFast,
        Easing::EaseOutCubic, [&state](float v) { state.qixing_fill = v; }, {},
        kSparkOwnerBarFill);
    state.animations.animate(
        state.icon_color_t, muted ? 1.0f : 0.0f, kSparkAnimFast, Easing::Linear,
        [&state](float v) { state.icon_color_t = v; }, {},
        kSparkOwnerIconColor);
    spark_request_frame(state);
}

void spark_hide(SparkState &state) {
    state.animations.animate(
        state.opacity, 0.0f, kSparkAnimNormal, Easing::EaseOutCubic,
        [&state](float v) { state.opacity = v; },
        [&state] { state.visible = false; }, kSparkOwnerOpacity);
    spark_request_frame(state);
}
