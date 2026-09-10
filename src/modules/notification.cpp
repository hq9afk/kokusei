#include <GLES3/gl32.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <utility>
#include <vector>

#include "core/log.h"

#include "modules/notification.h"

#include "render/color_ops.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/text.h"

float notification_detail_texture_height(const Texture &tex) {
    return tex.id ? static_cast<float>(tex.height) /
                        static_cast<float>(tex.scale > 0 ? tex.scale : 1)
                  : 0.0f;
}

const Color &notification_detail_urgency_color(uint8_t urgency) {
    if (urgency == 2)
        return palette::critical;
    if (urgency == 0)
        return palette::text_muted;
    return palette::accent;
}

namespace {

void notification_layer_surface_configure(void *data,
                                    zwlr_layer_surface_v1 *layer_surface,
                                    uint32_t serial, uint32_t, uint32_t) {
    auto *view = static_cast<NotificationView *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    view->configured = true;
}

void notification_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener notification_layer_surface_listener = {
    .configure = notification_layer_surface_configure,
    .closed = notification_layer_surface_closed,
};

PangoFontDescription *font_app_name() {
    static PangoFontDescription *d =
        pango_font_description_from_string("ComicShannsMono Nerd Font Bold 13");
    return d;
}

PangoFontDescription *font_summary() {
    static PangoFontDescription *d = pango_font_description_from_string(
        "ComicShannsMono Nerd Font Semi-Bold 17");
    return d;
}

PangoFontDescription *font_body() {
    static PangoFontDescription *d =
        pango_font_description_from_string("ComicShannsMono Nerd Font 15");
    return d;
}

const Texture &close_glyph_texture() {
    static Texture tex = make_texture_from_raster(
        rasterize_icon(icon::close, kContentScale, kNotificationCloseIconPx));
    return tex;
}

float notification_entry_height(const NotificationEntry &entry) {
    float content_h =
        std::max(kNotificationUrgencyDotSize,
                 notification_detail_texture_height(entry.app_name_texture));
    if (entry.summary_texture.id)
        content_h += kNotificationContentSpacing +
                     notification_detail_texture_height(entry.summary_texture);
    if (entry.body_texture.id)
        content_h += kNotificationContentSpacing +
                     notification_detail_texture_height(entry.body_texture);
    return content_h + kNotificationCardPadding * 2.0f + kNotificationCardExtraHeight;
}

void notification_load_record(NotificationEntry &entry, const NotificationRecord &record) {
    entry.app_name = record.app_name;
    entry.summary = record.summary;
    entry.body = record.body;
    entry.urgency = record.urgency;
    entry.timeout_ms = record.timeout_ms;
    entry.app_name_texture = Texture{};
    entry.summary_texture = Texture{};
    entry.body_texture = Texture{};
    entry.progress = 1.0f;
    entry.content_built = false;
}

void notification_build_content(NotificationEntry &entry) {
    if (entry.content_built)
        return;
    entry.content_built = true;

    int content_width = kNotificationSurfaceWidth -
                        static_cast<int>(kNotificationCardPadding * 2.0f) -
                        static_cast<int>(kNotificationCloseHitAreaSize +
                                         kNotificationCardContentTrailingGap);
    int32_t scale = kContentScale;
    RasterizedText app_name_text = rasterize_text_with(
        entry.app_name.empty() ? "Notification" : entry.app_name,
        font_app_name(), scale, content_width);
    entry.app_name_texture = make_texture_from_raster(app_name_text);

    if (!entry.summary.empty()) {
        RasterizedText summary_text = rasterize_text_with(
            entry.summary, font_summary(), scale, content_width);
        entry.summary_texture = make_texture_from_raster(summary_text);
    }

    if (!entry.body.empty()) {
        RasterizedText body_text =
            rasterize_text_with(entry.body, font_body(), scale, content_width);
        entry.body_texture = make_texture_from_raster(body_text);
    }

    entry.height = notification_entry_height(entry);
}

uint64_t opacity_owner(uint32_t id) {
    return (static_cast<uint64_t>(id) << 2) | 0;
}

uint64_t slide_owner(uint32_t id) {
    return (static_cast<uint64_t>(id) << 2) | 1;
}

uint64_t exit_owner(uint32_t id) {
    return (static_cast<uint64_t>(id) << 2) | 2;
}

uint64_t progress_owner(uint32_t id) {
    return (static_cast<uint64_t>(id) << 2) | 3;
}

void notification_start_exit(NotificationRenderModel &service, uint32_t id) {
    auto it = std::find_if(service.entries.begin(), service.entries.end(),
                           [id](const NotificationEntry &e) { return e.id == id; });
    if (it == service.entries.end() || it->exiting)
        return;
    it->exiting = true;
    service.animations.cancelForOwner(progress_owner(id));

    service.animations.animate(
        it->opacity, 0.0f, kNotificationAnimNormal, Easing::EaseOutCubic,
        [&service, id](float v) {
            auto e = std::find_if(
                service.entries.begin(), service.entries.end(),
                [id](const NotificationEntry &en) { return en.id == id; });
            if (e != service.entries.end())
                e->opacity = v;
        },
        {}, opacity_owner(id));

    service.animations.animate(
        0.0f, 1.0f, kNotificationAnimNormal + kNotificationAnimExitBuffer, Easing::Linear,
        [](float) {},
        [&service, id] {
            std::erase_if(service.entries,
                          [id](const NotificationEntry &e) { return e.id == id; });
        },
        exit_owner(id));
}

void notification_start_progress(NotificationRenderModel &service, uint32_t id,
                           int32_t timeout_ms) {
    service.animations.animate(
        1.0f, 0.0f, static_cast<float>(timeout_ms), Easing::Linear,
        [&service, id](float v) {
            auto e = std::find_if(
                service.entries.begin(), service.entries.end(),
                [id](const NotificationEntry &en) { return en.id == id; });
            if (e != service.entries.end())
                e->progress = v;
        },
        [&service, id] { notification_start_exit(service, id); }, progress_owner(id));
}

} // namespace

bool notification_view_create_surface(NotificationView &view, wl_compositor *compositor,
                                zwlr_layer_shell_v1 *layer_shell,
                                wl_output *output) {
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        .name_space = "kokusei-notification",
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        .width = kNotificationSurfaceWidth,
        .height = kNotificationSurfaceHeight,
        .margin_right = 10,
        .margin_bottom = 10,
        .empty_input_region = true,
    };
    view.layer_surface =
        layer_surface_create(view.surface, compositor, layer_shell, cfg,
                             &notification_layer_surface_listener, &view, output);
    if (!view.layer_surface)
        return false;
    view.compositor = compositor;
    view.output_scale.on_change = [&view](int32_t scale) {
        if (view.egl_window)
            wl_egl_window_resize(view.egl_window, kNotificationSurfaceWidth * scale,
                                 kNotificationSurfaceHeight * scale, 0, 0);
        if (view.frame_clock.surface)
            request_frame(view.frame_clock);
    };
    output_scale_watch(view.output_scale, view.surface);
    wl_surface_commit(view.surface);
    return true;
}

bool notification_view_init_egl(NotificationView &view, NotificationRenderModel &service,
                          Renderer &renderer, EGLDisplay display,
                          EGLConfig config, EGLContext context) {
    view.egl_display = display;
    view.egl_context = context;
    view.renderer = &renderer;
    int32_t scale = view.output_scale.scale;
    view.egl_window =
        wl_egl_window_create(view.surface, kNotificationSurfaceWidth * scale,
                             kNotificationSurfaceHeight * scale);
    view.egl_surface = eglCreateWindowSurface(
        display, config, reinterpret_cast<EGLNativeWindowType>(view.egl_window),
        nullptr);
    if (view.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, view.egl_surface, context))
        return false;
    view.frame_clock.surface = view.surface;
    view.frame_clock.draw = [&view, &service] { notification_paint(view, service); };
    return true;
}

void notification_view_request_frame(NotificationView &view) {
    if (view.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(view.frame_clock);
}

namespace {

void notification_add_entry(NotificationRenderModel &service,
                      const NotificationRecord &record) {
    uint32_t id = record.id;
    NotificationEntry entry;
    entry.id = id;
    notification_load_record(entry, record);
    service.entries.insert(service.entries.begin(), std::move(entry));

    service.animations.animate(
        0.0f, 1.0f, kNotificationAnimNormal, Easing::EaseOutCubic,
        [&service, id](float v) {
            auto e = std::find_if(
                service.entries.begin(), service.entries.end(),
                [id](const NotificationEntry &en) { return en.id == id; });
            if (e != service.entries.end())
                e->opacity = v;
        },
        {}, opacity_owner(id));
    service.animations.animate(
        kNotificationSlideOffset, 0.0f, kNotificationAnimNormal, Easing::EaseOutCubic,
        [&service, id](float v) {
            auto e = std::find_if(
                service.entries.begin(), service.entries.end(),
                [id](const NotificationEntry &en) { return en.id == id; });
            if (e != service.entries.end())
                e->slide_offset = v;
        },
        {}, slide_owner(id));
    notification_start_progress(service, id, record.timeout_ms);
}

bool notification_content_matches(const NotificationEntry &entry,
                            const NotificationRecord &record) {
    return entry.app_name == record.app_name &&
           entry.summary == record.summary && entry.body == record.body;
}

} // namespace

void notification_sync(NotificationRenderModel &service,
                 const NotificationService &notifications) {
    for (auto it = notifications.records.rbegin();
         it != notifications.records.rend(); ++it) {
        const NotificationRecord &record = *it;
        auto e = std::find_if(
            service.entries.begin(), service.entries.end(),
            [&record](const NotificationEntry &en) { return en.id == record.id; });
        if (e == service.entries.end()) {
            notification_add_entry(service, record);
        } else if (!notification_content_matches(*e, record)) {
            notification_load_record(*e, record);
            notification_start_progress(service, record.id, record.timeout_ms);
        }
    }

    for (NotificationEntry &entry : service.entries) {
        if (entry.exiting)
            continue;
        bool present = std::any_of(
            notifications.records.begin(), notifications.records.end(),
            [&entry](const NotificationRecord &r) { return r.id == entry.id; });
        if (!present)
            notification_start_exit(service, entry.id);
    }
}

void notification_paint(NotificationView &view, NotificationRenderModel &service) {
    if (!gl_make_current(view.egl_display, view.egl_surface, view.egl_context))
        return;
    view.renderer->begin_frame(kNotificationSurfaceWidth, kNotificationSurfaceHeight,
                               view.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    auto now = std::chrono::steady_clock::now();
    service.animations.tick(now);
    view.local_animations.tick(now);

    view.scene.rebuild();
    float y_cursor = static_cast<float>(kNotificationSurfaceHeight);

    std::vector<std::pair<uint32_t, Rect>> hitboxes;
    std::deque<Color> blend;
    for (NotificationEntry &entry : service.entries) {
        notification_build_content(entry);

        float local_mult = 1.0f;
        bool fading_out = false;
        if (auto it = view.local_exit.find(entry.id);
            it != view.local_exit.end()) {
            if (it->second <= 0.0f)
                continue;
            local_mult = it->second;
            fading_out = true;
        }

        y_cursor -= entry.height;
        if (y_cursor < 0.0f)
            break;
        float card_y = y_cursor + entry.slide_offset;
        float card_opacity = entry.opacity * local_mult;

        const Color &urgency_color = notification_detail_urgency_color(entry.urgency);

        blend.push_back(with_alpha(palette::overlay, card_opacity));
        const Color &card_fill = blend.back();
        blend.push_back(with_alpha(urgency_color, card_opacity));
        const Color &card_border = blend.back();

        Node *card = node_add_rrect(&view.scene.root, 0, card_y,
                                    kNotificationSurfaceWidth, entry.height,
                                    kNotificationCardRadius, kNotificationCardBorderWidth,
                                    rgba(card_fill), rgba(card_border));
        card->clip_children = true;

        float progress = std::clamp(entry.progress, 0.0f, 1.0f);
        float bar_rrect_h = 2.0f * kNotificationCardRadius;
        blend.push_back(with_alpha(urgency_color,
                                   kNotificationProgressTrackOpacity * card_opacity));
        Node *track_clip = node_add_group(card, 0, 0, kNotificationSurfaceWidth,
                                          kNotificationProgressHeight, true);
        node_add_rrect(track_clip, 0, 0, kNotificationSurfaceWidth, bar_rrect_h,
                       kNotificationCardRadius, 0, rgba(blend.back()),
                       kNodeTransparent);

        float fill_x = kNotificationSurfaceWidth * (1.0f - progress);
        float fill_w = kNotificationSurfaceWidth * progress;
        blend.push_back(with_alpha(urgency_color, card_opacity));
        Node *fill_clip = node_add_group(card, fill_x, 0, fill_w,
                                         kNotificationProgressHeight, true);
        node_add_rrect(fill_clip, -fill_x, 0, kNotificationSurfaceWidth,
                       bar_rrect_h, kNotificationCardRadius, 0, rgba(blend.back()),
                       kNodeTransparent);

        float content_x = kNotificationCardPadding;
        float content_y = kNotificationCardPadding;
        float header_h =
            std::max(kNotificationUrgencyDotSize,
                     notification_detail_texture_height(entry.app_name_texture));

        blend.push_back(with_alpha(urgency_color, card_opacity));
        node_add_rrect(card, content_x,
                       content_y + (header_h - kNotificationUrgencyDotSize) / 2.0f,
                       kNotificationUrgencyDotSize, kNotificationUrgencyDotSize,
                       kNotificationUrgencyDotRadius, 0, rgba(blend.back()),
                       kNodeTransparent);

        if (entry.app_name_texture.id) {
            blend.push_back(with_alpha(palette::text, 0.65f * card_opacity));
            node_add_texture(
                card, content_x + kNotificationUrgencyDotSize + kNotificationHeaderSpacing,
                content_y + (header_h - notification_detail_texture_height(
                                            entry.app_name_texture)) /
                                2.0f,
                entry.app_name_texture, rgba(blend.back()));
        }

        float row_y = content_y + header_h;
        if (entry.summary_texture.id) {
            row_y += kNotificationContentSpacing;
            blend.push_back(with_alpha(palette::text, card_opacity));
            node_add_texture(card, content_x, row_y, entry.summary_texture,
                             rgba(blend.back()));
            row_y += notification_detail_texture_height(entry.summary_texture);
        }
        if (entry.body_texture.id) {
            row_y += kNotificationContentSpacing;
            blend.push_back(with_alpha(palette::text, 0.72f * card_opacity));
            node_add_texture(card, content_x, row_y, entry.body_texture,
                             rgba(blend.back()));
        }

        float close_x = kNotificationSurfaceWidth - kNotificationCloseHitAreaSize;
        if (!fading_out)
            hitboxes.push_back(
                {entry.id, Rect{close_x, card_y, kNotificationCloseHitAreaSize,
                                kNotificationCloseHitAreaSize}});

        const Texture &glyph = close_glyph_texture();
        if (glyph.id) {
            float gw = static_cast<float>(glyph.width) /
                       static_cast<float>(glyph.scale > 0 ? glyph.scale : 1);
            float gh = static_cast<float>(glyph.height) /
                       static_cast<float>(glyph.scale > 0 ? glyph.scale : 1);
            float opacity = card_opacity * (entry.id == view.hovered_close_id
                                                ? 1.0f
                                                : kNotificationCloseIconOpacityIdle);
            blend.push_back(with_alpha(palette::text, opacity));
            node_add_texture(
                card,
                std::round(close_x + (kNotificationCloseHitAreaSize - gw) / 2.0f),
                std::round((kNotificationCloseHitAreaSize - gh) / 2.0f), glyph,
                rgba(blend.back()));
        }

        y_cursor -= kNotificationCardGap;
    }

    std::erase_if(view.local_exit, [&service](const auto &kv) {
        return std::none_of(
            service.entries.begin(), service.entries.end(),
            [&kv](const NotificationEntry &e) { return e.id == kv.first; });
    });

    if (view.compositor && hitboxes != view.close_hitboxes) {
        view.close_hitboxes = hitboxes;
        wl_region *region = wl_compositor_create_region(view.compositor);
        for (const auto &[id, r] : hitboxes)
            wl_region_add(region, static_cast<int>(r.x), static_cast<int>(r.y),
                          static_cast<int>(r.w), static_cast<int>(r.h));
        wl_surface_set_input_region(view.surface, region);
        wl_region_destroy(region);
    }

    view.scene.draw(*view.renderer);
    if (!eglSwapBuffers(view.egl_display, view.egl_surface))
        klog("notification: eglSwapBuffers failed, egl error 0x%04x", eglGetError());

    if (service.animations.hasActive() || view.local_animations.hasActive())
        request_frame(view.frame_clock);
}

namespace {

uint32_t notification_close_hit(const NotificationView &view, double x, double y) {
    for (const auto &[id, r] : view.close_hitboxes)
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return id;
    return 0;
}

} // namespace

bool notification_view_handle_close_click(NotificationView &view, double x, double y) {
    uint32_t id = notification_close_hit(view, x, y);
    if (id == 0 || view.local_exit.contains(id))
        return false;
    view.local_exit[id] = 1.0f;
    view.local_animations.animate(
        1.0f, 0.0f, kNotificationAnimNormal, Easing::EaseOutCubic,
        [&view, id](float v) {
            if (auto it = view.local_exit.find(id); it != view.local_exit.end())
                it->second = v;
        },
        [&view, id] {
            if (auto it = view.local_exit.find(id); it != view.local_exit.end())
                it->second = 0.0f;
        },
        id);
    return true;
}

bool notification_view_set_close_hover(NotificationView &view, double x, double y) {
    uint32_t id = notification_close_hit(view, x, y);
    if (id == view.hovered_close_id)
        return false;
    view.hovered_close_id = id;
    return true;
}

bool notification_view_clear_close_hover(NotificationView &view) {
    if (view.hovered_close_id == 0)
        return false;
    view.hovered_close_id = 0;
    return true;
}
