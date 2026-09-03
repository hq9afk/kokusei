#include <GLES2/gl2.h>
#include <algorithm>
#include <cmath>
#include <string>

#include "app/monitor_output.h"
#include "app/user_info.h"
#include "app/wayland_state.h"

#include "modules/yuheng.h"

#include "render/arc_gauge.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/image.h"
#include "render/layer_surface.h"
#include "render/marquee_text.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/panel_scroll.h"
#include "render/slider.h"
#include "render/text.h"

#include "service/mpris_service.h"
#include "service/pipewire_service.h"
#include "service/telemetry_service.h"
#include "service/upower_service.h"

bool yuheng_create_surface(YuhengState &state, wl_compositor *compositor,
                           zwlr_layer_shell_v1 *layer_shell,
                           wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-yuheng", output);
}

bool yuheng_init_egl(YuhengState &state, Renderer &renderer, WaylandState &app,
                     EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state, &app] {
        yuheng_paint(state, app, state.pending_qixing_height,
                     state.pending_qixing_top_margin);
    };
    AnimatedImageStyle pfp_style;
    pfp_style.size = kProfileAvatarSize;
    pfp_style.circular = true;
    pfp_style.decode = {15, static_cast<int>(kProfileAvatarSize) * 2};
    animated_image_set_source(state.profile_pic,
                              user_info::profile_media_path(), pfp_style);
    return true;
}

void yuheng_retarget(YuhengState &state, wl_compositor *compositor,
                     zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                     Renderer &renderer, WaylandState &app,
                     EGLDisplay egl_display, EGLConfig egl_config,
                     EGLContext egl_context, wl_output *target_output,
                     const char *target_name) {
    wl_output *bound = overlay_panel_retarget(
        state.base, display, state.bound_output, target_output, target_name,
        [&](wl_output *out) {
            return yuheng_create_surface(state, compositor, layer_shell, out);
        },
        [&] {
            return yuheng_init_egl(state, renderer, app, egl_display,
                                   egl_config, egl_context);
        });
    if (bound)
        state.bound_output = bound;
}

void yuheng_request_frame(YuhengState &state, float qixing_height,
                          float qixing_top_margin) {
    state.pending_qixing_height = qixing_height;
    state.pending_qixing_top_margin = qixing_top_margin;
    overlay_panel_request_frame(state.base);
}

void yuheng_toggle(YuhengState &state, bool by_widget) {
    if (!state.base.open) {
        state.opened_by_widget = by_widget;
        animated_image_show(state.profile_pic, [&state] {
            overlay_panel_request_frame(state.base);
        });
    } else {
        state.dragging.reset();
    }
    overlay_panel_toggle(state.base);
}

std::vector<IpcHandler> yuheng_ipc_handlers(YuhengState &yuheng,
                                            WaylandState &state) {
    return {
        {"yuheng",
         [&yuheng, &state] {
             if (!yuheng.base.open) {
                 MonitorOutput *target =
                     app_detail::active_target_monitor(state);
                 if (target && (target->output.wl != yuheng.bound_output ||
                                !yuheng.base.layer_surface))
                     yuheng_retarget(
                         yuheng, state.compositor, state.layer_shell,
                         state.display, state.renderer, state,
                         state.egl_display, state.egl_config, state.egl_context,
                         target->output.wl, target->output.name.c_str());
                 cpu_temp_poll(state.cpu_temp);
                 system_stats_poll(state.system_stats);
                 gpu_temp_poll(state.gpu_temp);
             }
             yuheng_toggle(yuheng);
         },
         "toggle the control center"},
    };
}

namespace {
void open_trulla(WaylandState &app) {
    for (auto &m : app.overlays) {
        if (std::string(m->name()) != "trulla")
            continue;
        for (IpcHandler &h : m->ipc_handlers(app))
            if (std::string(h.verb) == "trulla") {
                h.fn();
                return;
            }
    }
}

void apply_brightness_drag(YuhengState &state, WaylandState &app, double px) {
    const Rect &r = state.dragging->rect;
    float v = std::clamp(static_cast<float>((px - r.x) / r.w), 0.0f, 1.0f);
    state.brightness_level = v;
    brightness_set(app.brightness, v);
}
} // namespace

void yuheng_handle_click(YuhengState &state, WaylandState &app, double px,
                         double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };

    double scrolled_py = py + state.scroll_offset;
    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, scrolled_py))
            continue;
        switch (region.kind) {
        case PanelClickKind::Close:
            yuheng_toggle(state);
            break;
        case PanelClickKind::MuteToggle: {
            uint32_t id =
                volume_slider_resolve_tag_id(app.pipewire, region.tag);
            if (id != 0) {
                auto it = app.pipewire.nodes.find(id);
                if (it != app.pipewire.nodes.end())
                    pipewire_set_node_muted(app.pipewire, id,
                                            !it->second.muted);
            }
            break;
        }
        case PanelClickKind::SliderDrag: {
            state.dragging = DraggedSlider{region.tag, region.rect};
            state.selected_slider_tag = region.tag;
            if (region.tag == "brightness")
                apply_brightness_drag(state, app, px);
            else
                volume_slider_apply_drag(app.pipewire, *state.dragging, px);
            break;
        }
        case PanelClickKind::MediaPlayPause:
            mpris_play_pause(app.mpris);
            break;
        case PanelClickKind::MediaNext:
            mpris_next(app.mpris);
            break;
        case PanelClickKind::MediaPrevious:
            mpris_previous(app.mpris);
            break;
        case PanelClickKind::ProfileTrulla:
            open_trulla(app);
            break;
        default:
            break;
        }
        return;
    }

    if (!hit(state.panel_rect, px, py))
        yuheng_toggle(state);
}

void yuheng_handle_pointer_move(YuhengState &state, WaylandState &app,
                                double px) {
    if (!state.dragging)
        return;
    if (state.dragging->tag == "brightness")
        apply_brightness_drag(state, app, px);
    else
        volume_slider_apply_drag(app.pipewire, *state.dragging, px);
}

void yuheng_handle_scroll(YuhengState &state, double dy) {
    state.scroll_offset =
        panel_clamp_scroll(state.scroll_offset, static_cast<float>(dy),
                           state.content_height, state.visible_height);
}

void yuheng_handle_key_event(YuhengState &state, WaylandState &app,
                             const KeyEvent &event) {
    switch (event.kind) {
    case KeyKind::Escape:
        yuheng_toggle(state);
        break;
    case KeyKind::Left:
    case KeyKind::Right: {
        if (state.selected_slider_tag.empty())
            break;
        if (state.selected_slider_tag == "brightness") {
            float step = event.kind == KeyKind::Right ? kBrightnessKeyStep
                                                      : -kBrightnessKeyStep;
            state.brightness_level =
                std::clamp(state.brightness_level + step, 0.0f, 1.0f);
            brightness_set(app.brightness, state.brightness_level);
            break;
        }
        PipewireState &pw = app.pipewire;
        uint32_t id =
            volume_slider_resolve_tag_id(pw, state.selected_slider_tag);
        if (id == 0)
            break;
        auto it = pw.nodes.find(id);
        if (it == pw.nodes.end())
            break;
        float step = event.kind == KeyKind::Right ? 0.01f : -0.01f;
        pipewire_set_node_volume(
            pw, id, std::clamp(it->second.level + step, 0.0f, 1.0f));
        break;
    }
    default:
        break;
    }
}

using namespace panel_chrome_detail;

namespace {

struct CardChrome {
    float content_x;
    float content_y;
    float box_h;
};

CardChrome card_chrome_draw(Node *root, TextureCache &tcache, int32_t scale,
                            float x, float y, float w, float content_h,
                            const std::string &title) {
    float box_h = kCardTopPadding + kCardHeaderHeight + kCardHeaderContentGap +
                  content_h + kCardBottomPadding;
    node_add_rrect(root, x, y, w, box_h, kCardRadius, kCardBorderWidth,
                   rgba(palette::overlay), rgba(palette::accent));

    float header_y = y + kCardTopPadding;
    const Texture *title_tex = cached_text(tcache, title, scale);
    if (title_tex)
        node_add_texture(root, x + kCardHorizontalPadding,
                         header_y +
                             (kCardHeaderHeight - title_tex->height) / 2.0f,
                         *title_tex, rgba(palette::text));

    float content_x = x + kCardHorizontalPadding;
    float content_y = header_y + kCardHeaderHeight + kCardHeaderContentGap;
    return {content_x, content_y, box_h};
}

float draw_profile_card(Node *root, TextureCache &tcache, int32_t scale,
                        float x, float y, float w, AnimatedImage &profile_pic,
                        std::vector<PanelClickRegion> &regions) {
    const Texture *name_tex = cached_text(tcache, user_info::username(), scale);
    const Texture *uptime_tex =
        cached_text(tcache, user_info::uptime_string(), scale);
    float info_h = (name_tex ? name_tex->height : 0) + kProfileInfoSpacing +
                   (uptime_tex ? uptime_tex->height : 0);
    float h = kProfileVerticalPadding + kProfileAvatarSize + kProfileAvatarGap +
              info_h;

    node_add_rrect(root, x, y, w, h, kProfileRadius, kProfileBorderWidth,
                   rgba(palette::overlay), rgba(palette::accent));

    float avatar_x = x + (w - kProfileAvatarSize) / 2.0f;
    float avatar_y = y + kProfileTopPadding;
    node_add_rrect(root, avatar_x, avatar_y, kProfileAvatarSize,
                   kProfileAvatarSize, kProfileAvatarSize / 2.0f,
                   kProfileAvatarRingWidth, rgba(palette::overlay),
                   rgba(palette::accent));
    if (!profile_pic.frames.empty()) {
        animated_image_draw(profile_pic, root, avatar_x, avatar_y,
                            kProfileAvatarSize, kProfileAvatarSize, 1.0f);
    } else {
        const Texture *avatar_icon = cached_icon(tcache, icon::user, scale);
        if (avatar_icon)
            node_add_texture(
                root,
                avatar_x + (kProfileAvatarSize - avatar_icon->width) / 2.0f,
                avatar_y + (kProfileAvatarSize - avatar_icon->height) / 2.0f,
                *avatar_icon, rgba(palette::text));
    }

    const Texture *trulla_icon = cached_icon(tcache, icon::settings, scale);
    if (trulla_icon) {
        float sx = x + w - kCardHorizontalPadding - trulla_icon->width;
        float sy = y + kProfileTopPadding;
        node_add_texture(root, sx, sy, *trulla_icon, rgba(palette::text));
        Rect hit = {sx - kProfileTrullaHitPadding,
                    sy - kProfileTrullaHitPadding,
                    trulla_icon->width + 2 * kProfileTrullaHitPadding,
                    trulla_icon->height + 2 * kProfileTrullaHitPadding};
        regions.push_back({PanelClickKind::ProfileTrulla, hit, ""});
    }

    float info_y = avatar_y + kProfileAvatarSize + kProfileAvatarGap;
    if (name_tex)
        node_add_texture(root, x + (w - name_tex->width) / 2.0f, info_y,
                         *name_tex, rgba(palette::text));
    if (uptime_tex)
        node_add_texture(root, x + (w - uptime_tex->width) / 2.0f,
                         info_y + (name_tex ? name_tex->height : 0) +
                             kProfileInfoSpacing,
                         *uptime_tex, rgba(palette::text_dim));

    return h;
}

const char *battery_glyph(const UpowerState &u) {
    if (u.full)
        return icon::plugged_in;
    if (u.charging)
        return icon::battery_charging;
    if (u.percent <= 25)
        return icon::battery1;
    if (u.percent <= 50)
        return icon::battery2;
    if (u.percent <= 75)
        return icon::battery3;
    return icon::battery4;
}

float draw_battery_card(Node *root, TextureCache &tcache, int32_t scale,
                        float x, float y, float w, const UpowerState &upower) {
    if (!upower.present)
        return kCardGatedHeight;

    const Texture *icon_tex = cached_icon(tcache, battery_glyph(upower), scale);
    std::string label =
        upower.full ? "Plugged in" : (std::to_string(upower.percent) + "%");
    const Texture *label_tex = cached_text(tcache, "Battery  " + label, scale);

    float header_h =
        std::max(icon_tex ? icon_tex->height : 0.0f,
                 label_tex ? static_cast<float>(label_tex->height) : 0.0f);
    float content_h = header_h + kBatteryRowSpacing + kBatteryBarHeight;

    CardChrome chrome =
        card_chrome_draw(root, tcache, scale, x, y, w, content_h, "Battery");
    float cx = chrome.content_x, cy = chrome.content_y;
    float content_w = w - 2 * kCardHorizontalPadding;

    if (icon_tex)
        node_add_texture(root, cx, cy + (header_h - icon_tex->height) / 2.0f,
                         *icon_tex, rgba(palette::text));
    if (label_tex)
        node_add_texture(
            root, cx + (icon_tex ? icon_tex->width : 0) + kBatteryHeaderSpacing,
            cy + (header_h - label_tex->height) / 2.0f, *label_tex,
            rgba(palette::text));

    float qixing_y = cy + header_h + kBatteryRowSpacing;
    node_add_rrect(root, cx, qixing_y, content_w, kBatteryBarHeight,
                   kBatteryBarRadius, 0.0f, rgba(palette::text_alpha11),
                   kPanelNoBorder);
    float fill_w = content_w * std::clamp(upower.percent / 100.0f, 0.0f, 1.0f);
    if (fill_w > 0.0f)
        node_add_rrect(root, cx, qixing_y, fill_w, kBatteryBarHeight,
                       kBatteryBarRadius, 0.0f, rgba(palette::accent),
                       kPanelNoBorder);

    return chrome.box_h;
}

const Color &temp_color(float celsius) {
    if (celsius >= 85.0f)
        return palette::critical;
    if (celsius >= 70.0f)
        return kTempWarnColor;
    return palette::text;
}

float draw_system_stats_card(Node *root, TextureCache &tcache, int32_t scale,
                             float x, float y, float w,
                             const SystemStatsState &stats,
                             const GpuTempState &gpu_temp) {
    bool show_gpu = gpu_stats_available(gpu_temp);
    bool show_disk = stats.disk_pct >= 0.0f;
    int gauge_count = 2 + (show_gpu ? 1 : 0) + (show_disk ? 1 : 0);
    float total_gauge_w = kGaugeDiameter * gauge_count;
    float content_w = w - 2 * kCardHorizontalPadding;
    float gap = kStatsColumnGap;
    const Texture *label_h_tex = cached_text(tcache, "CPU", scale);
    float content_h = kGaugeDiameter + kStatsGaugeLabelSpacing +
                      (label_h_tex ? label_h_tex->height : 0.0f);

    CardChrome chrome =
        card_chrome_draw(root, tcache, scale, x, y, w, content_h, "Resources");
    float cx = chrome.content_x, cy = chrome.content_y;

    float row_w = total_gauge_w + gap * (gauge_count - 1);
    float gx = cx + (content_w - row_w) / 2.0f;

    auto gauge = [&](float gx01, const Color &color, const char *icon_glyph,
                     const std::string &value_label, const char *label) {
        const Texture *icon_tex = cached_icon(tcache, icon_glyph, scale);
        const Texture *value_tex = cached_text_clipped(
            tcache, value_label, scale, static_cast<int>(kGaugeDiameter));
        const Texture *sub_tex = cached_text(tcache, label, scale);
        return draw_arc_gauge(root, tcache, scale, gx, cy, kGaugeDiameter,
                              kGaugeStroke, gx01, color, icon_tex, rgba(color),
                              value_tex, rgba(palette::text), sub_tex,
                              rgba(palette::text_dim), kGaugeIconValueGap,
                              kStatsGaugeLabelSpacing);
    };

    float cpu01 = std::max(0.0f, stats.cpu_usage);
    gauge(cpu01, kGaugeColorCpu, icon::cpu,
          stats.cpu_usage >= 0.0f
              ? std::to_string(static_cast<int>(cpu01 * 100.0f)) + "%"
              : "--",
          "CPU");
    gx += kGaugeDiameter + gap;

    if (show_gpu) {
        float gpu01 = std::max(0.0f, gpu_temp.usage_percent / 100.0f);
        gauge(gpu01, kGaugeColorGpu, icon::gpu,
              std::to_string(static_cast<int>(gpu_temp.usage_percent)) + "%",
              "GPU");
        gx += kGaugeDiameter + gap;
    }

    float mem01 = std::max(0.0f, stats.mem_usage);
    gauge(mem01, kGaugeColorRam, icon::settings,
          stats.mem_usage >= 0.0f
              ? std::to_string(static_cast<int>(mem01 * 100.0f)) + "%"
              : "--",
          "RAM");
    gx += kGaugeDiameter + gap;

    if (show_disk) {
        float disk01 = std::clamp(stats.disk_pct / 100.0f, 0.0f, 1.0f);
        gauge(disk01, kGaugeColorDisk, icon::folder,
              std::to_string(static_cast<int>(stats.disk_pct)) + "%", "DISK");
    }

    return chrome.box_h;
}

float draw_cpu_temp_card(Node *root, TextureCache &tcache, int32_t scale,
                         float x, float y, float w,
                         const CpuTempState &cpu_temp) {
    std::vector<const CpuCoreTemp *> cores;
    for (const CpuCoreTemp &core : cpu_temp.cores)
        if (core.celsius >= 0.0f)
            cores.push_back(&core);

    std::string headline =
        (cpu_temp_available(cpu_temp)
             ? std::to_string(static_cast<int>(cpu_temp.celsius))
             : "--") +
        "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    float headline_h =
        std::max(headline_tex ? static_cast<float>(headline_tex->height) : 0.0f,
                 kTempRowHeight);

    float content_w = w - 2 * kCardHorizontalPadding;
    float cell_w = (content_w - (kCpuCoreColumns - 1) * kCpuCoreColumnSpacing) /
                   kCpuCoreColumns;
    int rows = cores.empty()
                   ? 0
                   : static_cast<int>((cores.size() + kCpuCoreColumns - 1) /
                                      kCpuCoreColumns);
    float grid_h = cores.empty()
                       ? 0.0f
                       : kCpuTempGridTopMargin + rows * kCpuCoreItemHeight +
                             std::max(0, rows - 1) * kCpuCoreRowSpacing;
    float content_h = headline_h + grid_h;

    CardChrome chrome = card_chrome_draw(root, tcache, scale, x, y, w,
                                         content_h, "CPU Temperature");
    float cx = chrome.content_x, cy = chrome.content_y;

    if (headline_tex)
        node_add_texture(root, cx, cy, *headline_tex,
                         rgba(temp_color(cpu_temp.celsius)));

    float grid_y = cy + headline_h + kCpuTempGridTopMargin;
    for (size_t i = 0; i < cores.size(); ++i) {
        const CpuCoreTemp &core = *cores[i];
        int col = static_cast<int>(i) % kCpuCoreColumns;
        int row = static_cast<int>(i) / kCpuCoreColumns;
        float cell_x = cx + col * (cell_w + kCpuCoreColumnSpacing);
        float cell_y = grid_y + row * (kCpuCoreItemHeight + kCpuCoreRowSpacing);
        node_add_rrect(root, cell_x, cell_y, cell_w, kCpuCoreItemHeight,
                       kCpuCoreItemRadius, 0.0f, rgba(palette::text_alpha08),
                       kPanelNoBorder);

        const Texture *name_tex =
            cached_text(tcache, "Core " + std::to_string(i), scale);
        if (name_tex)
            node_add_texture(root, cell_x + kCpuCoreTextMargin,
                             cell_y +
                                 (kCpuCoreItemHeight - name_tex->height) / 2.0f,
                             *name_tex, rgba(palette::text_dim));

        std::string value_label =
            std::to_string(static_cast<int>(core.celsius)) + "°C";
        const Texture *value_tex = cached_text(tcache, value_label, scale);
        if (value_tex)
            node_add_texture(
                root, cell_x + cell_w - kCpuCoreTextMargin - value_tex->width,
                cell_y + (kCpuCoreItemHeight - value_tex->height) / 2.0f,
                *value_tex, rgba(temp_color(core.celsius)));
    }

    return chrome.box_h;
}

float draw_gpu_temp_card(Node *root, TextureCache &tcache, int32_t scale,
                         float x, float y, float w,
                         const GpuTempState &gpu_temp) {
    if (!gpu_temp_available(gpu_temp))
        return kCardGatedHeight;

    std::string headline =
        std::to_string(static_cast<int>(gpu_temp.celsius)) + "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    float content_h =
        std::max(headline_tex ? static_cast<float>(headline_tex->height) : 0.0f,
                 kTempRowHeight);

    CardChrome chrome = card_chrome_draw(root, tcache, scale, x, y, w,
                                         content_h, "GPU Temperature");
    float cx = chrome.content_x, cy = chrome.content_y;

    if (headline_tex)
        node_add_texture(root, cx, cy, *headline_tex,
                         rgba(temp_color(gpu_temp.celsius)));

    return chrome.box_h;
}

Rect draw_media_button(Node *root, TextureCache &tcache, int32_t scale, float x,
                       float y, float size, float radius, const char *glyph,
                       std::vector<PanelClickRegion> &regions,
                       PanelClickKind kind) {
    Rect rect = {x, y, size, size};
    node_add_rrect(root, rect.x, rect.y, rect.w, rect.h, radius, 0.0f,
                   rgba(palette::overlay), kPanelNoBorder);
    const Texture *tex = cached_icon(tcache, glyph, scale);
    if (tex)
        node_add_texture(root, rect.x + (rect.w - tex->width) / 2.0f,
                         rect.y + (rect.h - tex->height) / 2.0f, *tex,
                         rgba(palette::text));
    regions.push_back({kind, rect, ""});
    return rect;
}

float draw_media_card(Node *root, TextureCache &tcache, int32_t scale, float x,
                      float y, float w, const MprisState &mpris,
                      std::unordered_map<std::string, Texture> &art_cache,
                      AnimationManager &anim, MarqueeTextState &title_marquee,
                      MarqueeTextState &artist_marquee,
                      std::vector<PanelClickRegion> &regions) {
    float content_h = kMediaThumbSize + kMediaProgressTopMargin +
                      kMediaProgressRowHeight + kMediaCtrlTopMargin +
                      kMediaCtrlRowHeight;

    CardChrome chrome =
        card_chrome_draw(root, tcache, scale, x, y, w, content_h, "Media");
    float cx = chrome.content_x, cy = chrome.content_y;
    float content_w = w - 2 * kCardHorizontalPadding;

    node_add_rrect(root, cx, cy, kMediaThumbSize, kMediaThumbSize,
                   kMediaThumbRadius, 0.0f, rgba(palette::overlay),
                   kPanelNoBorder);
    const Texture *art_tex = nullptr;
    if (mpris.has_player &&
        mpris_detail_is_local_art_url(mpris.track.art_url)) {
        std::string path = mpris.track.art_url.substr(7);
        auto it = art_cache.find(path);
        if (it == art_cache.end())
            it = art_cache.emplace(path, load_image_texture(path)).first;
        if (it->second.id)
            art_tex = &it->second;
    }
    if (art_tex) {
        node_add_texture_rect(root, cx, cy, kMediaThumbSize, kMediaThumbSize,
                              *art_tex, rgba(palette::text));
    } else {
        const Texture *note_tex = cached_icon(tcache, icon::music_note, scale);
        if (note_tex)
            node_add_texture(root,
                             cx + (kMediaThumbSize - note_tex->width) / 2.0f,
                             cy + (kMediaThumbSize - note_tex->height) / 2.0f,
                             *note_tex, rgba(palette::text_dim));
    }

    float text_x = cx + kMediaThumbSize + kMediaTitleLeftMargin;
    float text_w =
        std::max(20.0f, content_w - kMediaThumbSize - kMediaTitleLeftMargin);
    std::string title = mpris.has_player ? mpris.track.title : "No player";
    std::string artist = mpris.has_player ? mpris.track.artist : "";
    if (title.empty())
        title = "Unknown";
    const Texture *title_probe = cached_text(tcache, title, scale);
    float title_h =
        title_probe ? static_cast<float>(title_probe->height) : 0.0f;
    draw_marquee_text(root, tcache, anim, title_marquee, scale, title, text_x,
                      cy + kMediaThumbSize / 2.0f - title_h -
                          kMediaTitleSpacing / 2.0f,
                      text_w, rgba(palette::text));
    draw_marquee_text(root, tcache, anim, artist_marquee, scale, artist, text_x,
                      cy + kMediaThumbSize / 2.0f + kMediaTitleSpacing / 2.0f,
                      text_w, rgba(palette::text_dim));

    float progress_y = cy + kMediaThumbSize + kMediaProgressTopMargin;
    if (mpris.has_player) {
        std::string progress_label =
            mpris_detail_format_position(mpris.track.position_us) + " / " +
            mpris_detail_format_position(mpris.track.length_us);
        const Texture *progress_tex =
            cached_text(tcache, progress_label, scale);
        if (progress_tex)
            node_add_texture(
                root, cx + (content_w - progress_tex->width) / 2.0f,
                progress_y +
                    (kMediaProgressRowHeight - progress_tex->height) / 2.0f,
                *progress_tex, rgba(palette::text_dim));
    }

    float ctrl_y = progress_y + kMediaProgressRowHeight + kMediaCtrlTopMargin;
    float ctrl_row_w =
        2 * kMediaSideBtnSize + kMediaPlayBtnSize + 2 * kMediaCtrlSpacing;
    float btn_x = cx + (content_w - ctrl_row_w) / 2.0f;
    float side_btn_y =
        ctrl_y + (kMediaCtrlRowHeight - kMediaSideBtnSize) / 2.0f;
    float play_btn_y =
        ctrl_y + (kMediaCtrlRowHeight - kMediaPlayBtnSize) / 2.0f;

    draw_media_button(root, tcache, scale, btn_x, side_btn_y, kMediaSideBtnSize,
                      kMediaSideBtnRadius, icon::player_prev, regions,
                      PanelClickKind::MediaPrevious);
    btn_x += kMediaSideBtnSize + kMediaCtrlSpacing;
    const char *play_glyph = mpris.status == MprisPlaybackStatus::Playing
                                 ? icon::player_pause
                                 : icon::player_play;
    draw_media_button(root, tcache, scale, btn_x, play_btn_y, kMediaPlayBtnSize,
                      kMediaPlayBtnRadius, play_glyph, regions,
                      PanelClickKind::MediaPlayPause);
    btn_x += kMediaPlayBtnSize + kMediaCtrlSpacing;
    draw_media_button(root, tcache, scale, btn_x, side_btn_y, kMediaSideBtnSize,
                      kMediaSideBtnRadius, icon::player_next, regions,
                      PanelClickKind::MediaNext);

    return chrome.box_h;
}

std::string default_node_label(const PipewireState &pw, bool is_sink) {
    uint32_t id = is_sink ? pw.default_sink_id : pw.default_source_id;
    auto it = pw.nodes.find(id);
    if (it == pw.nodes.end())
        return "";
    return it->second.description.empty() ? it->second.name
                                          : it->second.description;
}

float draw_volume_row(Node *root, TextureCache &tcache, int32_t scale, float x,
                      float y, float w, const char *label,
                      const std::string &device, const char *glyph, bool muted,
                      float level, std::vector<PanelClickRegion> &regions,
                      const char *region_tag) {
    const Texture *label_tex = cached_text(tcache, label, scale);
    std::string device_text = device.empty() ? "" : " \xE2\x80\x94 " + device;
    const Texture *device_tex =
        cached_text_clipped(tcache, device_text, scale,
                            static_cast<int>(kVolumeDeviceTextMaxWidth));
    float label_row_h = label_tex ? label_tex->height : 0.0f;

    if (label_tex)
        node_add_texture(root, x, y, *label_tex, rgba(palette::text));
    if (device_tex)
        node_add_texture(root,
                         x + (label_tex ? label_tex->width : 0) +
                             kVolumeLabelRowSpacing,
                         y, *device_tex, rgba(palette::text_dim));

    float slider_y = y + label_row_h + kVolumeRowSpacing;
    float mute_x = x + w - kVolumeMuteBtnSize;
    float pct_x = mute_x - kVolumePctMuteGap - kVolumePctTextWidth;
    float slider_right = pct_x - kVolumeSliderPctGap;
    Rect slider_rect = {x, slider_y, slider_right - x, kVolumeSliderRowHeight};
    draw_slider_track(root, regions, slider_rect, slider_rect,
                      kVolumeCardSliderTrackHeight, muted ? 0.0f : level, muted,
                      region_tag);

    std::string pct_label =
        muted ? "muted"
              : std::to_string(static_cast<int>(std::round(level * 100.0f))) +
                    "%";
    const Texture *pct_tex = cached_text(tcache, pct_label, scale);
    if (pct_tex)
        node_add_texture(root, pct_x + kVolumePctTextWidth - pct_tex->width,
                         slider_y +
                             (kVolumeSliderRowHeight - pct_tex->height) / 2.0f,
                         *pct_tex, rgba(palette::text_dim));

    Rect mute_rect = {
        mute_x, slider_y + (kVolumeSliderRowHeight - kVolumeMuteBtnSize) / 2.0f,
        kVolumeMuteBtnSize, kVolumeMuteBtnSize};
    node_add_rrect(root, mute_rect.x, mute_rect.y, mute_rect.w, mute_rect.h,
                   kVolumeMuteBtnRadius, 0.0f, rgba(palette::overlay),
                   kPanelNoBorder);
    const Texture *icon_tex = cached_icon(tcache, glyph, scale);
    if (icon_tex)
        node_add_texture(root,
                         mute_rect.x + (mute_rect.w - icon_tex->width) / 2.0f,
                         mute_rect.y + (mute_rect.h - icon_tex->height) / 2.0f,
                         *icon_tex, rgba(palette::text));
    regions.push_back({PanelClickKind::MuteToggle, mute_rect, region_tag});

    return label_row_h + kVolumeRowSpacing + kVolumeSliderRowHeight;
}

float draw_volume_card(Node *root, TextureCache &tcache, int32_t scale, float x,
                       float y, float w, const PipewireState &pw,
                       std::vector<PanelClickRegion> &regions) {
    bool sink_muted = false, source_muted = false;
    float sink_level = pipewire_sink_level(pw, sink_muted);
    float source_level = pipewire_source_level(pw, source_muted);

    const Texture *probe = cached_text(tcache, "Output", scale);
    float row_h = (probe ? probe->height : 0.0f) + kVolumeRowSpacing +
                  kVolumeSliderRowHeight;
    float content_h = 2 * row_h + kVolumeCardSpacing;

    CardChrome chrome =
        card_chrome_draw(root, tcache, scale, x, y, w, content_h, "Volume");
    float cx = chrome.content_x, cy = chrome.content_y;
    float content_w = w - 2 * kCardHorizontalPadding;

    float sink_glyph_level = sink_muted ? 0.0f : sink_level;
    const char *sink_glyph =
        volume_threshold_icon(sink_muted, sink_glyph_level);
    float row1_h =
        draw_volume_row(root, tcache, scale, cx, cy, content_w, "Output",
                        default_node_label(pw, true), sink_glyph, sink_muted,
                        sink_level, regions, "sink");

    float row2_y = cy + row1_h + kVolumeCardSpacing;
    const char *source_glyph = source_muted ? icon::mic_off : icon::mic_on;
    draw_volume_row(root, tcache, scale, cx, row2_y, content_w, "Input",
                    default_node_label(pw, false), source_glyph, source_muted,
                    source_level, regions, "source");

    return chrome.box_h;
}

float draw_brightness_card(Node *root, TextureCache &tcache, int32_t scale,
                           float x, float y, float w, bool present, float level,
                           std::vector<PanelClickRegion> &regions) {
    if (!present)
        return kCardGatedHeight;

    const Texture *icon_tex = cached_icon(tcache, icon::sun, scale);
    float icon_h = icon_tex ? icon_tex->height : 0.0f;
    float content_h = std::max(icon_h, kBrightnessSliderRowHeight);

    CardChrome chrome =
        card_chrome_draw(root, tcache, scale, x, y, w, content_h, "Brightness");
    float cx = chrome.content_x, cy = chrome.content_y;
    float content_w = w - 2 * kCardHorizontalPadding;

    if (icon_tex)
        node_add_texture(root, cx, cy + (content_h - icon_h) / 2.0f, *icon_tex,
                         rgba(palette::text));

    std::string pct_label =
        std::to_string(static_cast<int>(std::round(level * 100.0f))) + "%";
    const Texture *pct_tex = cached_text(tcache, pct_label, scale);

    float pct_x = cx + content_w - kBrightnessPctTextWidth;
    float slider_x =
        cx + (icon_tex ? icon_tex->width : 0.0f) + kBrightnessIconGap;
    float slider_right = pct_x - kBrightnessSliderPctGap;
    Rect slider_rect = {slider_x, cy, slider_right - slider_x,
                        kBrightnessSliderRowHeight};
    draw_slider_track(root, regions, slider_rect, slider_rect,
                      kBrightnessSliderTrackHeight, level, false, "brightness");

    if (pct_tex)
        node_add_texture(root, pct_x + kBrightnessPctTextWidth - pct_tex->width,
                         cy + (kBrightnessSliderRowHeight - pct_tex->height) /
                                  2.0f,
                         *pct_tex, rgba(palette::text_dim));

    return chrome.box_h;
}

} // namespace

void yuheng_paint(YuhengState &state, WaylandState &app, float qixing_height,
                  float qixing_top_margin) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    if (state.base.open)
        animated_image_tick(state.profile_pic, now);
    else
        animated_image_hide(state.profile_pic);
    gl_make_current(state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.click_regions.clear();
    state.panel_rect = {};
    state.scene.rebuild();

    if (!state.base.open) {
        state.scene.draw(*state.renderer);
        eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
        return;
    }

    Node *root = &state.scene.root;

    float panel_w = kYuhengPanelWidth;
    float panel_x =
        static_cast<float>(state.base.width) - panel_w - kPanelSideMargin;
    float panel_y = qixing_top_margin + qixing_height + kPanelGap;

    float screen_budget = std::max(0.0f, static_cast<float>(state.base.height) -
                                             panel_y - kPanelSideMargin);
    float content_h_est =
        state.content_height > 0.0f ? state.content_height : screen_budget;
    float visible_height = std::min(screen_budget, content_h_est);
    state.scroll_offset = panel_clamp_scroll(state.scroll_offset, 0.0f,
                                             content_h_est, visible_height);

    Node *scroll_clip =
        node_add_group(root, panel_x, panel_y, panel_w, visible_height, true);
    Node *scroll_content =
        node_add_group(scroll_clip, -panel_x, -panel_y - state.scroll_offset,
                       panel_w, content_h_est, false);

    float content_y = panel_y;
    content_y += draw_profile_card(scroll_content, state.tcache, scale, panel_x,
                                   content_y, panel_w, state.profile_pic,
                                   state.click_regions);

    float battery_y = content_y + kPanelColumnSpacing;
    float battery_h =
        draw_battery_card(scroll_content, state.tcache, scale, panel_x,
                          battery_y, panel_w, app.upower);
    if (battery_h > 0.0f)
        content_y = battery_y + battery_h;

    bool brightness_present = !app.brightness.device.empty();
    if (brightness_present &&
        !(state.dragging && state.dragging->tag == "brightness"))
        state.brightness_level = brightness_get(app.brightness);
    float brightness_y = content_y + kPanelColumnSpacing;
    float brightness_h = draw_brightness_card(
        scroll_content, state.tcache, scale, panel_x, brightness_y, panel_w,
        brightness_present, state.brightness_level, state.click_regions);
    if (brightness_h > 0.0f)
        content_y = brightness_y + brightness_h;

    float volume_y = content_y + kPanelColumnSpacing;
    content_y = volume_y + draw_volume_card(scroll_content, state.tcache, scale,
                                            panel_x, volume_y, panel_w,
                                            app.pipewire, state.click_regions);

    float media_y = content_y + kPanelColumnSpacing;
    content_y = media_y + draw_media_card(
                              scroll_content, state.tcache, scale, panel_x,
                              media_y, panel_w, app.mpris, state.art_cache,
                              state.base.animations, state.media_title_marquee,
                              state.media_artist_marquee, state.click_regions);

    float stats_y = content_y + kPanelColumnSpacing;
    content_y = stats_y + draw_system_stats_card(
                              scroll_content, state.tcache, scale, panel_x,
                              stats_y, panel_w, app.system_stats, app.gpu_temp);

    float cpu_y = content_y + kPanelColumnSpacing;
    content_y =
        cpu_y + draw_cpu_temp_card(scroll_content, state.tcache, scale, panel_x,
                                   cpu_y, panel_w, app.cpu_temp);

    float gpu_y = content_y + kPanelColumnSpacing;
    float gpu_h = draw_gpu_temp_card(scroll_content, state.tcache, scale,
                                     panel_x, gpu_y, panel_w, app.gpu_temp);
    if (gpu_h > 0.0f)
        content_y = gpu_y + gpu_h;

    state.content_height = content_y - panel_y;
    state.visible_height = visible_height;
    state.panel_rect = {panel_x, panel_y, panel_w, visible_height};

    state.renderer->set_opacity(state.base.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive() ||
        animated_image_animating(state.profile_pic))
        overlay_panel_request_frame(state.base);
}
