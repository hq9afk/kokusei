#include <GLES3/gl32.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <deque>
#include <string>
#include <thread>
#include <vector>

#include "app/monitor_output.h"
#include "app/user_info.h"
#include "app/wayland_state.h"

#include "core/deferred_call.h"
#include "core/log.h"

#include "modules/penance.h"
#include "modules/penance/layout.h"
#include "modules/penance/pam_authenticator.h"

#include "render/arc_gauge.h"
#include "render/color_ops.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/image.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text.h"
#include "render/text_elide.h"

#include "service/mpris_service.h"

constexpr Color kPenanceResGaugeGpuColor = color(kPenanceResGaugeGpuColorHex);

namespace {

void penance_paint(PenanceState &st, PenanceOutputSurface &los);
void start_init_anim(PenanceState &st, PenanceOutputSurface &los);
void start_unlock_anim(PenanceState &st, PenanceOutputSurface &los);
void finish_unlock(PenanceState &st);

float px_h(const Texture *t) {
    if (!t)
        return 0.0f;
    return static_cast<float>(t->height) /
           static_cast<float>(t->scale > 0 ? t->scale : 1);
}
float px_w(const Texture *t) {
    if (!t)
        return 0.0f;
    return static_cast<float>(t->width) /
           static_cast<float>(t->scale > 0 ? t->scale : 1);
}

std::string strftime_now(const char *fmt, size_t cap) {
    std::time_t now = std::time(nullptr);
    std::string buf(cap, '\0');
    size_t n = std::strftime(buf.data(), cap, fmt, std::localtime(&now));
    buf.resize(n);
    return buf;
}
std::string hour_string() { return strftime_now("%H", 8); }
std::string minute_string() { return strftime_now("%M", 8); }
std::string date_string() {
    std::string s = strftime_now("%a %Y-%m-%d", 64);
    for (char &c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

const Texture *tc_text(PenanceState &st, const std::string &s, int px,
                       bool bold, int32_t scale) {
    if (s.empty())
        return nullptr;
    std::string key = "pt:" + std::to_string(px) + (bold ? "b:" : "n:") + s;
    return st.tcache.get(key,
                         [&] { return rasterize_text_px(s, px, bold, scale); });
}

const Texture *tc_icon(PenanceState &st, const char *glyph, int px,
                       int32_t scale) {
    std::string key = std::string("pi:") + std::to_string(px) + ":" + glyph;
    return st.tcache.get(key, [&] { return rasterize_icon(glyph, scale, px); });
}

std::string wm_name(const WaylandState *app) {
    return app && app->compositor_backend ==
                       WaylandState::CompositorBackend::Hyprland
               ? "Hyprland"
               : "Wayland";
}

void draw_center_column(PenanceState &st, PenanceOutputSurface &los,
                        Node *content, const PenanceRect &col, int32_t scale,
                        float ca);
void draw_left_column(PenanceState &st, PenanceOutputSurface &los,
                      Node *content, const PenanceRect &col, int32_t scale,
                      float ca);
void draw_right_column(PenanceState &st, PenanceOutputSurface &los,
                       Node *content, const PenanceRect &col, int32_t scale,
                       float ca);

size_t utf8_len(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80)
            ++n;
    return n;
}

PenanceOutputSurface *surface_for(PenanceState &st, wl_surface *s) {
    for (auto &up : st.surfaces)
        if (up->surface == s)
            return up.get();
    return nullptr;
}

void request_all(PenanceState &st) {
    for (auto &up : st.surfaces)
        if (up->frame_clock.surface)
            request_frame(up->frame_clock);
    if (st.app)
        app_detail::rest_egl_current(*st.app);
}

void start_init_anim(PenanceState &st, PenanceOutputSurface &los) {
    (void)st;
    klog("penance: init anim start on '%s' box=%.0f target=%.0fx%.0f",
         los.output_name.c_str(), penance_icon_box_size(), los.panel_w_target,
         los.panel_h_target);
    los.anim_started = true;
    los.panel_scale = kPenanceScaleHidden;
    los.panel_rotation = 0.0f;
    los.icon_alpha = 1.0f;
    los.content_alpha = 0.0f;
    los.content_scale = kPenanceScaleHidden;

    float box = penance_icon_box_size();
    los.panel_w = box;
    los.panel_h = box;
    float target_w = los.panel_w_target > 0 ? los.panel_w_target : box;
    float target_h = los.panel_h_target > 0 ? los.panel_h_target : box;

    auto &a = los.animations;
    a.animate(
        kPenanceScaleHidden, kPenanceScaleFull, kPenanceAnimSpinMs,
        Easing::EaseOutBack, [&los](float v) { los.panel_scale = v; }, {},
        kPenanceOwnerPanelScale);
    a.animate(
        0.0f, 360.0f, kPenanceAnimSpinMs, Easing::EaseInOutCubic,
        [&los](float v) { los.panel_rotation = v; },
        [&los, target_w, target_h] {
            klog("penance: entrance expand begin on '%s' -> %.0fx%.0f",
                 los.output_name.c_str(), target_w, target_h);
            los.panel_rotation = 0.0f;
            auto &a2 = los.animations;
            a2.animate(
                los.panel_w, target_w, kPenanceAnimExpandMs,
                Easing::EaseOutCubic, [&los](float v) { los.panel_w = v; }, {},
                kPenanceOwnerPanelWidth);
            a2.animate(
                los.panel_h, target_h, kPenanceAnimExpandMs,
                Easing::EaseOutCubic, [&los](float v) { los.panel_h = v; }, {},
                kPenanceOwnerPanelHeight);
            a2.animate(
                1.0f, 0.0f, kPenanceAnimIconFadeOutMs, Easing::EaseOutCubic,
                [&los](float v) { los.icon_alpha = v; }, {},
                kPenanceOwnerIconAlpha);
            a2.animate(
                0.0f, 1.0f, kPenanceAnimContentFadeInMs, Easing::EaseOutCubic,
                [&los](float v) { los.content_alpha = v; }, {},
                kPenanceOwnerContentAlpha);
            a2.animate(
                kPenanceScaleHidden, kPenanceScaleFull,
                kPenanceAnimContentScaleInMs, Easing::EaseOutBack,
                [&los](float v) { los.content_scale = v; }, {},
                kPenanceOwnerContentScale);
        },
        kPenanceOwnerPanelRotation);
}

void start_unlock_anim(PenanceState &st, PenanceOutputSurface &los) {
    float box = penance_icon_box_size();
    auto &a = los.animations;
    a.animate(
        los.panel_w, box, kPenanceAnimShrinkMs, Easing::EaseInCubic,
        [&los](float v) { los.panel_w = v; }, {}, kPenanceOwnerPanelWidth);
    a.animate(
        los.panel_h, box, kPenanceAnimShrinkMs, Easing::EaseInCubic,
        [&los](float v) { los.panel_h = v; }, {}, kPenanceOwnerPanelHeight);
    a.animate(
        los.icon_alpha, 1.0f, kPenanceAnimIconFadeInMs, Easing::EaseInCubic,
        [&los](float v) { los.icon_alpha = v; }, {}, kPenanceOwnerIconAlpha);
    a.animate(
        los.content_alpha, 0.0f, kPenanceAnimContentFadeOutMs,
        Easing::EaseInCubic, [&los](float v) { los.content_alpha = v; }, {},
        kPenanceOwnerContentAlpha);
    a.animate(
        los.content_scale, kPenanceScaleHidden, kPenanceAnimContentScaleOutMs,
        Easing::EaseInBack, [&los](float v) { los.content_scale = v; }, {},
        kPenanceOwnerContentScale);

    bool is_primary = !st.surfaces.empty() && st.surfaces.front().get() == &los;
    a.animate(
        0.0f, 1.0f, kPenanceAnimShrinkMs, Easing::Linear, [](float) {},
        [&st, &los, is_primary] {
            auto &a2 = los.animations;
            a2.animate(
                los.panel_scale, kPenanceScaleHidden, kPenanceAnimSpinMs,
                Easing::EaseInBack, [&los](float v) { los.panel_scale = v; },
                {}, kPenanceOwnerPanelScale);
            a2.animate(
                0.0f, -360.0f, kPenanceAnimSpinMs, Easing::EaseInOutCubic,
                [&los](float v) { los.panel_rotation = v; },
                [&st, is_primary] {
                    if (is_primary)
                        DeferredCall::call_later([&st] { finish_unlock(st); });
                },
                kPenanceOwnerPanelRotation);
        },
        kPenanceOwnerSequence);
}

std::deque<Color> &color_pool() {
    static thread_local std::deque<Color> pool;
    return pool;
}

const float *cmod(const Color &base, float a) {
    auto &pool = color_pool();
    pool.push_back(with_alpha(base, base.a * a));
    return rgba(pool.back());
}

float rnd(float v) { return std::round(v); }

void build_panel(PenanceState &st, PenanceOutputSurface &los, Node *root) {
    int32_t scale = los.output_scale.scale;
    float ow = static_cast<float>(los.width);
    float oh = static_cast<float>(los.height);

    color_pool().clear();

    const char *glyph = st.unlocking ? icon::lock_open : icon::lock;
    const Texture *icon_tex =
        tc_icon(st, glyph, static_cast<int>(kPenanceFontIcon), scale);

    float card_w = penance_card_width(oh);
    float card_h = penance_card_height(oh);
    los.panel_w_target = card_w;
    los.panel_h_target = card_h;

    if (!los.anim_started)
        start_init_anim(st, los);
    else if (!los.animations.hasActive() && !st.unlocking) {
        los.panel_w = card_w;
        los.panel_h = card_h;
    }

    float pw = los.panel_w;
    float ph = los.panel_h;
    float px, py;
    penance_panel_origin(ow, oh, pw, ph, px, py);

    Node *panel = node_add_rrect(root, px, py, pw, ph, kPenanceCardRadius,
                                 kPenanceBgBorderWidth, rgba(palette::overlay),
                                 rgba(palette::accent));
    panel->rotation = los.panel_rotation;
    panel->scale = los.panel_scale;
    panel->clip_children = true;

    if (icon_tex && los.icon_alpha > 0.001f) {
        float iw = px_w(icon_tex), ih = px_h(icon_tex);
        node_add_texture_rect(panel, rnd((pw - iw) * 0.5f),
                              rnd((ph - ih) * 0.5f), iw, ih, *icon_tex,
                              cmod(palette::accent, los.icon_alpha));
    }

    if (los.content_alpha <= 0.001f) {
        los.media_prev = los.media_play = los.media_next = los.pill_button = {};
        return;
    }

    float ca = los.content_alpha;
    float center_w = kPenanceCenterWidth * penance_center_scale(oh);
    PenanceRect left, center, right;
    penance_columns(card_w, card_h, center_w, left, center, right);

    Node *content = node_add_group(panel, (pw - card_w) * 0.5f,
                                   (ph - card_h) * 0.5f, card_w, card_h);
    content->scale = los.content_scale;

    draw_left_column(st, los, content, left, scale, ca);
    draw_center_column(st, los, content, center, scale, ca);
    draw_right_column(st, los, content, right, scale, ca);

    if (los.content_scale < 0.99f) {
        los.media_prev = los.media_play = los.media_next = los.pill_button = {};
    } else {
        Rect *rects[] = {&los.media_prev, &los.media_play, &los.media_next,
                         &los.pill_button};
        for (Rect *rp : rects)
            if (rp->w > 0.0f) {
                rp->x += px;
                rp->y += py;
            }
    }
}

void draw_pill(PenanceState &st, PenanceOutputSurface &los, Node *content,
               float x, float y, float w, int32_t scale, float ca) {
    float h = kPenanceInputHeight;
    float r = h * 0.5f;
    node_add_rrect(content, x, y, w, h, r, kPenanceBgBorderWidth,
                   cmod(palette::field_bg, ca), cmod(palette::accent, ca));

    float cy = y + h * 0.5f;

    const Texture *lock_t =
        tc_icon(st, icon::lock, static_cast<int>(kPenancePillIconSize), scale);
    if (lock_t)
        node_add_texture_rect(content, rnd(x + r - px_w(lock_t) * 0.5f),
                              rnd(cy - px_h(lock_t) * 0.5f), px_w(lock_t),
                              px_h(lock_t), *lock_t,
                              cmod(palette::text_muted, ca));

    bool has_text = !st.password.text.empty();
    float btn = kPenancePillButtonSize;
    float btn_x = x + w - btn - (h - btn) * 0.5f;
    float btn_y = cy - btn * 0.5f;
    node_add_rrect(content, btn_x, btn_y, btn, btn, btn * 0.5f, 0.0f,
                   cmod(has_text ? palette::accent : palette::surface_alt, ca),
                   kNodeTransparent);
    const Texture *arrow_t = tc_icon(
        st, icon::arrow_right, static_cast<int>(kPenancePillIconSize), scale);
    if (arrow_t)
        node_add_texture_rect(
            content, rnd(btn_x + (btn - px_w(arrow_t)) * 0.5f),
            rnd(btn_y + (btn - px_h(arrow_t)) * 0.5f), px_w(arrow_t),
            px_h(arrow_t), *arrow_t,
            cmod(has_text ? palette::base : palette::text_muted, ca));
    los.pill_button = {btn_x, btn_y, btn, btn};

    float mid_x0 = x + h;
    float mid_w = w - 2.0f * h;
    if (mid_w < 10.0f) {
        mid_x0 = x + kPenancePillPad;
        mid_w = w - 2.0f * kPenancePillPad;
    }

    if (!has_text) {
        if (st.failed) {
            const Texture *ft =
                tc_text(st, kPenanceFailText,
                        static_cast<int>(kPenanceFontNormal), false, scale);
            if (ft)
                node_add_texture_rect(
                    content, rnd(mid_x0 + (mid_w - px_w(ft)) * 0.5f),
                    rnd(cy - px_h(ft) * 0.5f), px_w(ft), px_h(ft), *ft,
                    cmod(palette::critical, ca));
            return;
        }
        const char *ph =
            st.authenticating ? kPenanceLoadingText : kPenancePlaceholderText;
        const Texture *pt =
            tc_text(st, ph, static_cast<int>(kPenanceFontNormal), false, scale);
        if (pt)
            node_add_texture_rect(content,
                                  rnd(mid_x0 + (mid_w - px_w(pt)) * 0.5f),
                                  rnd(cy - px_h(pt) * 0.5f), px_w(pt), px_h(pt),
                                  *pt, cmod(palette::text_muted, ca));
        text_field_row_slide(st.pw_row_slide, los.animations,
                             kPenanceOwnerDotRowX,
                             mid_x0 + penance_dot_x(0, 0, mid_w));
        return;
    }

    int n = static_cast<int>(utf8_len(st.password.text));
    float row_x = text_field_row_slide(st.pw_row_slide, los.animations,
                                       kPenanceOwnerDotRowX,
                                       mid_x0 + penance_dot_x(0, n, mid_w));
    for (int i = 0; i < n; ++i) {
        const TextFieldCharAnim *anim =
            i < static_cast<int>(st.pw_anim.chars.size())
                ? &st.pw_anim.chars[static_cast<size_t>(i)]
                : nullptr;
        float dsc = anim ? anim->scale : 1.0f;
        float slide = anim ? anim->slide_x : 0.0f;
        float dx = row_x + static_cast<float>(i) * kPenanceDotSize + slide;
        float dsz = kPenanceDotSize * dsc;
        float gx = rnd(dx + (kPenanceDotSize - dsz) * 0.5f);
        float gy = rnd(cy - dsz * 0.5f);
        if (st.echo_glyph.id)
            node_add_texture_rect(content, gx, gy, dsz, dsz, st.echo_glyph,
                                  cmod(palette::text, ca));
        else
            node_add_rrect(content, gx, gy, dsz, dsz, dsz * 0.5f, 0.0f,
                           cmod(palette::text, ca), kNodeTransparent);
    }
}

struct PenanceCard {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
};

PenanceCard draw_card(PenanceState &st, Node *content, const PenanceRect &col,
                      const std::string &title, int32_t scale, float ca) {
    node_add_rrect(content, col.x, col.y, col.w, col.h, kPenanceSidePanelRadius,
                   kPenanceCardBorderWidth, cmod(palette::overlay, ca),
                   cmod(palette::accent, ca));

    float x = col.x + kPenanceSidePanelPad;
    float y = col.y + kPenanceSidePanelPad;
    float w = col.w - 2.0f * kPenanceSidePanelPad;
    const Texture *title_t =
        tc_text(st, title, static_cast<int>(kPenanceFontNormal), true, scale);
    if (!title_t)
        return {x, y, w};

    node_add_texture_rect(content, rnd(x), rnd(y), px_w(title_t), px_h(title_t),
                          *title_t, cmod(palette::text, ca));
    return {x, y + px_h(title_t) + kPenanceCardHeaderGap, w};
}

void draw_fetch(PenanceState &st, Node *content, const PenanceRect &col,
                int32_t scale, float ca) {
    float pad = kPenanceSidePanelPad;
    int fpx = static_cast<int>(kPenanceFontMono);
    float inner_w = col.w - 2.0f * pad;
    size_t maxc = static_cast<size_t>(
        std::max(1.0f, inner_w / (static_cast<float>(fpx) * 0.62f)));

    std::vector<std::string> lines;
    lines.push_back("OS  : " + user_info::os_pretty_name());
    lines.push_back("WM  : " + wm_name(st.app));
    lines.push_back("UP  : " + user_info::uptime_string());

    const Texture *label_t = tc_text(st, st.user, fpx, true, scale);
    const Texture *prompt_t = tc_text(st, ">", fpx, true, scale);
    float head_h = std::max(px_h(prompt_t), px_h(label_t));

    std::vector<const Texture *> line_tex;
    float line_h = 0.0f;
    for (const std::string &l : lines) {
        const Texture *t = tc_text(st, elide(l, maxc), fpx, false, scale);
        line_tex.push_back(t);
        line_h = std::max(line_h, px_h(t));
    }
    static const Color *term[12] = {
        &palette::accent,     &palette::accent_container,
        &palette::accent_alt, &palette::accent_alt_container,
        &palette::electro,    &palette::lavender,
        &palette::critical,   &palette::text,
        &palette::text_muted, &palette::surface_alt,
        &palette::field_bg,   &palette::base};
    int per_row = penance_fetch_colour_count(inner_w, 6);
    int rows = per_row > 0 ? 2 : 0;
    int total_boxes = std::min(12, per_row * rows);

    PenanceCard cc = draw_card(st, content, col, "System", scale, ca);
    float x = cc.x;
    float y = cc.y;

    float hx = x;
    if (prompt_t) {
        node_add_texture_rect(content, rnd(hx),
                              rnd(y + (head_h - px_h(prompt_t)) * 0.5f),
                              px_w(prompt_t), px_h(prompt_t), *prompt_t,
                              cmod(palette::accent, ca));
        hx += px_w(prompt_t) + kPenanceFetchChipPad;
    }
    if (label_t)
        node_add_texture_rect(
            content, rnd(hx), rnd(y + (head_h - px_h(label_t)) * 0.5f),
            px_w(label_t), px_h(label_t), *label_t, cmod(palette::text, ca));
    y += head_h + kPenanceFetchLineGap;

    for (const Texture *t : line_tex) {
        if (t)
            node_add_texture_rect(content, rnd(x), rnd(y), px_w(t), px_h(t), *t,
                                  cmod(palette::text_muted, ca));
        y += line_h + kPenanceFetchLineGap;
    }

    int drawn = 0;
    for (int r = 0; r < rows; ++r) {
        int in_row = std::min(per_row, total_boxes - drawn);
        float bx = x;
        for (int i = 0; i < in_row; ++i) {
            node_add_rrect(content, bx, y, kPenanceFetchColorBox,
                           kPenanceFetchColorBox, kPenanceResTileRadius * 0.4f,
                           0.0f, cmod(*term[drawn], ca), kNodeTransparent);
            bx += kPenanceFetchColorBox + kPenanceFetchColorGap;
            ++drawn;
        }
        y += kPenanceFetchColorBox + kPenanceFetchColorGap;
    }
}

void draw_media(PenanceState &st, PenanceOutputSurface &los, Node *content,
                const PenanceRect &col, int32_t scale, float ca) {
    los.media_prev = los.media_play = los.media_next = {};
    float pad = kPenanceSidePanelPad;
    if (col.h < kPenanceMediaArt + 2.0f * pad)
        return;

    PenanceCard cc = draw_card(st, content, col, "Media", scale, ca);

    const MprisState &m = st.app->mpris;
    float cx = col.x + col.w * 0.5f;
    float art = kPenanceMediaArt;
    float bs = kPenanceMediaBtnSize;
    int tpx = static_cast<int>(kPenanceFontNormal);
    size_t maxc = static_cast<size_t>(std::max(
        1.0f, (col.w - 2.0f * pad) / (static_cast<float>(tpx) * 0.62f)));
    std::string title = m.has_player && !m.track.title.empty()
                            ? m.track.title
                            : "Nothing playing";
    std::string artist = m.has_player && !m.track.artist.empty()
                             ? m.track.artist
                             : "Try playing some music";
    const Texture *tt = tc_text(st, elide(title, maxc), tpx, true, scale);
    const Texture *at =
        tc_text(st, elide(artist, maxc), static_cast<int>(kPenanceFontMono),
                false, scale);

    float block_h = art + kPenanceMediaTextGap * 3.0f + px_h(tt) +
                    kPenanceMediaTextGap + px_h(at) +
                    kPenanceMediaBtnGap * 1.5f + bs;
    float avail_h = col.y + col.h - pad - cc.y;
    float ay = cc.y + std::max(0.0f, (avail_h - block_h) * 0.5f);

    node_add_rrect(content, cx - art * 0.5f, ay, art, art,
                   kPenanceResTileRadius, 0.0f, cmod(palette::field_bg, ca),
                   kNodeTransparent);

    const Texture *art_tex = nullptr;
    if (m.has_player && mpris_detail_is_local_art_url(m.track.art_url)) {
        std::string path = m.track.art_url.substr(7);
        auto it = st.art_cache.find(path);
        if (it == st.art_cache.end())
            it = st.art_cache.emplace(path, load_image_texture(path)).first;
        if (it->second.id)
            art_tex = &it->second;
    }
    if (art_tex)
        node_add_texture_rect(content, cx - art * 0.5f, ay, art, art, *art_tex,
                              cmod(palette::text, ca));
    else {
        const Texture *note =
            tc_icon(st, icon::music_note, static_cast<int>(art * 0.4f), scale);
        if (note)
            node_add_texture_rect(content, rnd(cx - px_w(note) * 0.5f),
                                  rnd(ay + (art - px_h(note)) * 0.5f),
                                  px_w(note), px_h(note), *note,
                                  cmod(palette::text_dim, ca));
    }

    float ty = ay + art + kPenanceMediaTextGap * 3.0f;
    if (tt)
        node_add_texture_rect(content, rnd(cx - px_w(tt) * 0.5f), rnd(ty),
                              px_w(tt), px_h(tt), *tt,
                              cmod(palette::accent, ca));
    ty += px_h(tt) + kPenanceMediaTextGap;
    if (at)
        node_add_texture_rect(content, rnd(cx - px_w(at) * 0.5f), rnd(ty),
                              px_w(at), px_h(at), *at,
                              cmod(palette::text_muted, ca));
    ty += px_h(at) + kPenanceMediaBtnGap * 1.5f;

    if (ty + bs > col.y + col.h - pad)
        return;
    float row_w = 3.0f * bs + 2.0f * kPenanceMediaBtnGap;
    float bx = cx - row_w * 0.5f;

    auto button = [&](float rx, const char *glyph, Rect &out) {
        node_add_rrect(content, rx, ty, bs, bs, bs * 0.5f, 0.0f,
                       cmod(palette::field_bg, ca), kNodeTransparent);
        const Texture *g =
            tc_icon(st, glyph, static_cast<int>(bs * 0.5f), scale);
        if (g)
            node_add_texture_rect(content, rnd(rx + (bs - px_w(g)) * 0.5f),
                                  rnd(ty + (bs - px_h(g)) * 0.5f), px_w(g),
                                  px_h(g), *g, cmod(palette::text, ca));
        out = {rx, ty, bs, bs};
    };
    button(bx, icon::player_prev, los.media_prev);
    button(bx + bs + kPenanceMediaBtnGap,
           m.status == MprisPlaybackStatus::Playing ? icon::player_pause
                                                    : icon::player_play,
           los.media_play);
    button(bx + 2.0f * (bs + kPenanceMediaBtnGap), icon::player_next,
           los.media_next);
}

void draw_resources(PenanceState &st, Node *content, const PenanceRect &col,
                    int32_t scale, float ca) {
    float pad = kPenanceSidePanelPad;
    float gap = kPenanceResTileGap;

    PenanceCard cc = draw_card(st, content, col, "Resources", scale, ca);
    float avail_h = col.y + col.h - pad - cc.y;

    const SystemStatsState &s = st.app->system_stats;
    const CpuTempState &ct = st.app->cpu_temp;
    const GpuTempState &gt = st.app->gpu_temp;
    bool show_gpu = gpu_stats_available(gt);

    struct Tile {
        const char *glyph;
        std::string label;
        float frac;
        int pct;
        const Color *accent;
        const Color *label_color;
        int row;
    };
    auto temp_label = [](const char *base, float celsius,
                         const Color *&label_color) {
        if (celsius <= 0.0f)
            return std::string(base);
        int c = static_cast<int>(std::lround(celsius));
        label_color = celsius >= kPenanceResTempWarnC ? &palette::critical
                                                      : &palette::text_dim;
        return std::string(base) + " - " + std::to_string(c) +
               "\xC2\xB0"
               "C";
    };
    std::vector<Tile> tiles;
    const Color *cpu_label_color = &palette::text_dim;
    std::string cpu_label = temp_label("CPU", ct.celsius, cpu_label_color);
    tiles.push_back({icon::cpu, cpu_label, std::max(0.0f, s.cpu_usage),
                     s.cpu_usage >= 0.0f
                         ? static_cast<int>(std::lround(s.cpu_usage * 100.0f))
                         : -1,
                     &palette::accent, cpu_label_color, 0});
    if (show_gpu) {
        const Color *gpu_label_color = &palette::text_dim;
        std::string gpu_label = temp_label("GPU", gt.celsius, gpu_label_color);
        tiles.push_back({icon::gpu, gpu_label,
                         std::clamp(gt.usage_percent / 100.0f, 0.0f, 1.0f),
                         static_cast<int>(std::lround(gt.usage_percent)),
                         &kPenanceResGaugeGpuColor, gpu_label_color, 0});
    }
    tiles.push_back({icon::device_desktop, "RAM", std::max(0.0f, s.mem_usage),
                     s.mem_usage >= 0.0f
                         ? static_cast<int>(std::lround(s.mem_usage * 100.0f))
                         : -1,
                     &palette::lavender, &palette::text_dim, 1});
    tiles.push_back(
        {icon::folder, "DISK", std::max(0.0f, s.disk_pct / 100.0f),
         s.disk_pct >= 0.0f ? static_cast<int>(std::lround(s.disk_pct)) : -1,
         &palette::accent_alt, &palette::text_dim, 1});

    const Texture *label_probe =
        tc_text(st, "CPU", static_cast<int>(kPenanceFontMono), false, scale);
    float cell_extra = px_h(label_probe) + kPenanceResGaugeLabelGap;

    float tile_sz = std::min((col.w - 3.0f * gap) * 0.5f,
                             (avail_h - gap - 2.0f * cell_extra) * 0.5f);
    float stroke = tile_sz * kPenanceResGaugeStrokeRatio;

    float col_gap = (col.w - 2.0f * tile_sz) / 3.0f;
    float block_h = 2.0f * tile_sz + gap + 2.0f * cell_extra;
    float y0 = cc.y + std::max(0.0f, (avail_h - block_h) * 0.5f);
    float row_y[2] = {y0, y0 + tile_sz + cell_extra + gap};

    int row_count[2] = {0, 0};
    for (const Tile &t : tiles)
        ++row_count[t.row];
    int row_seen[2] = {0, 0};

    for (const Tile &t : tiles) {
        int within = row_seen[t.row]++;
        float tx = row_count[t.row] == 1
                       ? col.x + (col.w - tile_sz) * 0.5f
                       : (within == 0 ? col.x + col_gap
                                      : col.x + col.w - col_gap - tile_sz);
        float ty = row_y[t.row];

        float frac = std::clamp(t.frac, 0.0f, 1.0f);
        Color arc_color = with_alpha(*t.accent, t.accent->a * ca);
        const Texture *ic =
            tc_icon(st, t.glyph, static_cast<int>(kPenanceResIconSize), scale);
        const Texture *vt =
            tc_text(st, t.pct >= 0 ? std::to_string(t.pct) + "%" : "--",
                    static_cast<int>(kPenanceResValueFont), true, scale);
        const Texture *lt = tc_text(
            st, t.label, static_cast<int>(kPenanceFontMono), false, scale);
        draw_arc_gauge(content, st.tcache, scale, tx, ty, tile_sz, stroke, frac,
                       arc_color, ic, cmod(*t.accent, ca), vt,
                       cmod(palette::text, ca), lt, cmod(*t.label_color, ca),
                       kPenanceResGaugeIconValueGap, kPenanceResGaugeLabelGap);
    }
}

void draw_notifs(PenanceState &st, Node *content, const PenanceRect &col,
                 int32_t scale, float ca) {
    if (col.h < 60.0f)
        return;

    float pad = kPenanceSidePanelPad;
    const std::vector<NotificationRecord> &recs = st.app->notifications.records;

    std::string hdr = recs.empty() ? "Notifications"
                                   : std::to_string(recs.size()) +
                                         (recs.size() == 1 ? " notification"
                                                           : " notifications");
    PenanceCard cc = draw_card(st, content, col, hdr, scale, ca);
    float x = cc.x;
    float y = cc.y;

    if (recs.empty()) {
        const Texture *nt =
            tc_text(st, "No Notifications",
                    static_cast<int>(kPenanceFontNormal), false, scale);
        if (nt)
            node_add_texture_rect(content,
                                  rnd(col.x + (col.w - px_w(nt)) * 0.5f),
                                  rnd(col.y + col.h * 0.5f), px_w(nt), px_h(nt),
                                  *nt, cmod(palette::text_dim, ca));
        return;
    }

    float clip_h = col.y + col.h - y - pad;
    Node *clip = node_add_group(content, x, y, cc.w, clip_h, true);
    float cw = col.w - 2.0f * pad;
    float cardpad = kPenanceNotifCardPad;
    size_t maxc = static_cast<size_t>(
        std::max(1.0f, (cw - 2.0f * cardpad) / (kPenanceFontMono * 0.62f)));

    float cy = 0.0f;
    int shown = 0;
    for (const NotificationRecord &rec : recs) {
        if (shown >= kPenanceNotifMaxCards || cy >= clip_h)
            break;
        const Texture *appt = tc_text(
            st,
            elide(rec.app_name.empty() ? "Notification" : rec.app_name, maxc),
            static_cast<int>(kPenanceFontMono), true, scale);
        const Texture *sumt =
            rec.summary.empty()
                ? nullptr
                : tc_text(st, elide(rec.summary, maxc),
                          static_cast<int>(kPenanceFontNormal), false, scale);
        const Texture *bodyt =
            rec.body.empty()
                ? nullptr
                : tc_text(st, elide(rec.body, maxc),
                          static_cast<int>(kPenanceFontMono), false, scale);
        float ch = 2.0f * cardpad + px_h(appt) +
                   (sumt ? kPenanceMediaTextGap + px_h(sumt) : 0.0f) +
                   (bodyt ? kPenanceMediaTextGap + px_h(bodyt) : 0.0f);

        node_add_rrect(clip, 0.0f, cy, cw, ch, kPenanceNotifCardRadius, 0.0f,
                       cmod(palette::field_bg, ca), kNodeTransparent);
        float ix = cardpad;
        float iy = cy + cardpad;
        if (appt) {
            node_add_texture_rect(clip, rnd(ix), rnd(iy), px_w(appt),
                                  px_h(appt), *appt, cmod(palette::accent, ca));
            iy += px_h(appt) + kPenanceMediaTextGap;
        }
        if (sumt) {
            node_add_texture_rect(clip, rnd(ix), rnd(iy), px_w(sumt),
                                  px_h(sumt), *sumt, cmod(palette::text, ca));
            iy += px_h(sumt) + kPenanceMediaTextGap;
        }
        if (bodyt)
            node_add_texture_rect(clip, rnd(ix), rnd(iy), px_w(bodyt),
                                  px_h(bodyt), *bodyt,
                                  cmod(palette::text_muted, ca));

        cy += ch + kPenanceNotifCardGap;
        ++shown;
    }
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

float draw_battery(PenanceState &st, Node *content, const PenanceRect &col,
                   int32_t scale, float ca) {
    if (!st.app || !st.app->upower.present)
        return 0.0f;

    const UpowerState &u = st.app->upower;
    int fpx = static_cast<int>(kPenanceFontNormal);

    const Texture *title_t = tc_text(st, "Battery", fpx, true, scale);
    const Texture *icon_t = tc_icon(st, battery_glyph(u), fpx, scale);
    std::string label =
        u.full ? "Plugged in" : (std::to_string(u.percent) + "%");
    const Texture *label_t =
        tc_text(st, "Battery  " + label, fpx, false, scale);

    float row_h = std::max(px_h(icon_t), px_h(label_t));
    float card_h = kPenanceSidePanelPad + px_h(title_t) +
                   kPenanceCardHeaderGap + row_h + kPenanceBatteryRowGap +
                   kPenanceBatteryBarHeight + kPenanceSidePanelPad;

    PenanceCard cc = draw_card(st, content, {col.x, col.y, col.w, card_h},
                               "Battery", scale, ca);
    float x = cc.x;
    float y = cc.y;
    float content_w = cc.w;

    float rx = x;
    if (icon_t) {
        node_add_texture_rect(
            content, rnd(rx), rnd(y + (row_h - px_h(icon_t)) * 0.5f),
            px_w(icon_t), px_h(icon_t), *icon_t, cmod(palette::text, ca));
        rx += px_w(icon_t) + kPenanceBatteryIconGap;
    }
    if (label_t)
        node_add_texture_rect(
            content, rnd(rx), rnd(y + (row_h - px_h(label_t)) * 0.5f),
            px_w(label_t), px_h(label_t), *label_t, cmod(palette::text, ca));
    y += row_h + kPenanceBatteryRowGap;

    node_add_rrect(content, rnd(x), rnd(y), content_w, kPenanceBatteryBarHeight,
                   kPenanceBatteryBarRadius, 0.0f,
                   cmod(palette::text_alpha11, ca), kNodeTransparent);
    float fill_w = content_w * std::clamp(u.percent / 100.0f, 0.0f, 1.0f);
    if (fill_w > 0.0f)
        node_add_rrect(content, rnd(x), rnd(y), fill_w,
                       kPenanceBatteryBarHeight, kPenanceBatteryBarRadius, 0.0f,
                       cmod(palette::accent, ca), kNodeTransparent);

    return card_h;
}

void draw_left_column(PenanceState &st, PenanceOutputSurface &los,
                      Node *content, const PenanceRect &col, int32_t scale,
                      float ca) {
    float bat_h = draw_battery(st, content, col, scale, ca);
    float top = col.y + (bat_h > 0.0f ? bat_h + kPenancePanelGap : 0.0f);
    float rem = col.h - (top - col.y);
    float ch = std::max(0.0f, (rem - kPenancePanelGap) * 0.5f);
    PenanceRect fetch{col.x, top, col.w, ch};
    PenanceRect media{col.x, top + ch + kPenancePanelGap, col.w, ch};
    draw_fetch(st, content, fetch, scale, ca);
    draw_media(st, los, content, media, scale, ca);
}

void draw_right_column(PenanceState &st, PenanceOutputSurface &los,
                       Node *content, const PenanceRect &col, int32_t scale,
                       float ca) {
    (void)los;
    float ch = penance_side_card_height(col.h);
    PenanceRect res{col.x, col.y, col.w, ch};
    PenanceRect notif{col.x, col.y + ch + kPenancePanelGap, col.w, ch};
    draw_resources(st, content, res, scale, ca);
    draw_notifs(st, content, notif, scale, ca);
}

void draw_center_column(PenanceState &st, PenanceOutputSurface &los,
                        Node *content, const PenanceRect &col, int32_t scale,
                        float ca) {
    draw_card(st, content, col, "", scale, ca);

    float oh = static_cast<float>(los.height);
    float cscale = penance_center_scale(oh);
    int clock_px = std::max(1, static_cast<int>(kPenanceFontClock * cscale));

    const Texture *ht = tc_text(st, hour_string(), clock_px, true, scale);
    const Texture *colon_t = tc_text(st, ":", clock_px, true, scale);
    const Texture *mt = tc_text(st, minute_string(), clock_px, true, scale);
    const Texture *dt = tc_text(
        st, date_string(), static_cast<int>(kPenanceFontDate), true, scale);

    float clock_h = std::max(px_h(ht), px_h(mt));
    float clock_w = px_w(ht) + kPenanceClockGap + px_w(colon_t) +
                    kPenanceClockGap + px_w(mt);
    float date_h = px_h(dt);

    float total_h = penance_content_height(clock_h, date_h, 0.0f);
    float y = col.y + (col.h - total_h) * 0.5f;
    float cx = col.x + col.w * 0.5f;

    float mx = cx - clock_w * 0.5f;
    if (ht) {
        node_add_texture_rect(content, rnd(mx), rnd(y + clock_h - px_h(ht)),
                              px_w(ht), px_h(ht), *ht,
                              cmod(palette::accent, ca));
        mx += px_w(ht) + kPenanceClockGap;
    }
    if (colon_t) {
        node_add_texture_rect(content, rnd(mx),
                              rnd(y + clock_h - px_h(colon_t)), px_w(colon_t),
                              px_h(colon_t), *colon_t, cmod(palette::text, ca));
        mx += px_w(colon_t) + kPenanceClockGap;
    }
    if (mt)
        node_add_texture_rect(content, rnd(mx), rnd(y + clock_h - px_h(mt)),
                              px_w(mt), px_h(mt), *mt,
                              cmod(palette::lavender, ca));
    y += clock_h + kPenanceGapClockDate;

    if (dt)
        node_add_texture_rect(content, rnd(cx - px_w(dt) * 0.5f), rnd(y),
                              px_w(dt), px_h(dt), *dt, cmod(palette::text, ca));
    y += date_h + kPenanceGapDateAvatar;

    float ax = cx - kPenanceProfileSize * 0.5f;
    st.avatar.style.ring_fill = rgba(palette::field_bg);
    st.avatar.style.border_color = rgba(palette::accent);
    animated_image_draw(st.avatar, content, rnd(ax), rnd(y),
                        kPenanceProfileSize, kPenanceProfileSize, ca);
    y += kPenanceProfileSize + kPenanceGapAvatarInput;

    float pill_w = col.w * kPenanceInputWidthFrac;
    draw_pill(st, los, content, cx - pill_w * 0.5f, y, pill_w, scale, ca);
}

void penance_paint(PenanceState &st, PenanceOutputSurface &los) {
    if (!los.configured || los.egl_surface == EGL_NO_SURFACE || !st.app)
        return;
    using clk = std::chrono::steady_clock;
    clk::time_point t_begin = clk::now();
    clk::time_point t_make, t_expanse, t_panel, t_draw, t_swap;

    static int frame = 0;
    int f = ++frame;
    bool trace = f <= 90;
    auto step = [&](const char *what) {
        if (trace)
            klog("penance: paint #%d '%s' %s", f, los.output_name.c_str(),
                 what);
    };

    Renderer &r = st.app->renderer;
    step("enter -> eglMakeCurrent");
    if (!gl_make_current(st.app->egl_display, los.egl_surface,
                         st.app->egl_context))
        return;
    t_make = clk::now();
    step("begin_frame");
    r.begin_frame(los.width, los.height, los.output_scale.scale);
    glClearColor(palette::base.r, palette::base.g, palette::base.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto now = clk::now();
    los.animations.tick(now);
    animated_image_tick(st.avatar, now);

    los.scene.rebuild();
    Node &root = los.scene.root;

    step("draw_expanse");
    if (st.draw_expanse)
        st.draw_expanse(los.output_name, root, los.width, los.height);
    t_expanse = clk::now();

    step("build_panel");
    if (!los.panel_gated)
        build_panel(st, los, &root);
    t_panel = clk::now();

    step("scene.draw");
    r.set_opacity(1.0f);
    los.scene.draw(r);
    gl_check("penance_paint");
    t_draw = clk::now();
    step("eglSwapBuffers");
    if (!eglSwapBuffers(st.app->egl_display, los.egl_surface))
        klog("penance: eglSwapBuffers failed on '%s', egl error 0x%04x",
             los.output_name.c_str(), eglGetError());
    t_swap = clk::now();
    step("swapped");

    auto ms = [](clk::time_point a, clk::time_point b) {
        return std::chrono::duration<float, std::milli>(b - a).count();
    };
    if (ms(t_begin, t_swap) > 50.0f)
        klog("penance: SLOW frame #%d '%s' make=%.1f expanse=%.1f panel=%.1f "
             "draw=%.1f swap=%.1f",
             f, los.output_name.c_str(), ms(t_begin, t_make),
             ms(t_make, t_expanse), ms(t_expanse, t_panel), ms(t_panel, t_draw),
             ms(t_draw, t_swap));
    else if (f % 30 == 0)
        klog("penance: paint #%d '%s' locked=%d unlocking=%d gated=%d "
             "scale=%.2f content=%.2f",
             f, los.output_name.c_str(), st.locked, st.unlocking,
             los.panel_gated, los.panel_scale, los.content_alpha);

    bool avatar_running = st.locked && !st.unlocking && !los.panel_gated &&
                          animated_image_animating(st.avatar);
    if (los.animations.hasActive() || avatar_running)
        request_frame(los.frame_clock);
}

void surface_configure(void *data, ext_session_lock_surface_v1 *s,
                       uint32_t serial, uint32_t w, uint32_t h) {
    auto *los = static_cast<PenanceOutputSurface *>(data);
    ext_session_lock_surface_v1_ack_configure(s, serial);

    PenanceState *st = los->owner;
    int32_t scale = los->output_scale.scale;
    bool first = los->egl_surface == EGL_NO_SURFACE;
    los->width = static_cast<int32_t>(w);
    los->height = static_cast<int32_t>(h);

    if (first) {
        los->egl_window = wl_egl_window_create(los->surface, los->width * scale,
                                               los->height * scale);
        los->egl_surface = eglCreateWindowSurface(
            st->app->egl_display, st->app->egl_config,
            reinterpret_cast<EGLNativeWindowType>(los->egl_window), nullptr);
        if (los->egl_surface == EGL_NO_SURFACE) {
            klog("penance: eglCreateWindowSurface failed on '%s'",
                 los->output_name.c_str());
            return;
        }
        los->frame_clock.surface = los->surface;
        los->frame_clock.draw = [st, los] { penance_paint(*st, *los); };
    } else if (los->egl_window) {
        wl_egl_window_resize(los->egl_window, los->width * scale,
                             los->height * scale, 0, 0);
    }
    los->configured = true;
    request_frame(los->frame_clock);
    app_detail::rest_egl_current(*st->app);
}

constexpr ext_session_lock_surface_v1_listener kSurfaceListener = {
    .configure = surface_configure,
};

void handle_locked(void *data, ext_session_lock_v1 *) {
    auto *st = static_cast<PenanceState *>(data);
    if (!st->active)
        return;
    st->locked = true;
    st->locked_at = std::chrono::steady_clock::now();
    if (st->app)
        st->app->session_locked = true;
    klog("penance: session locked, %zu surface(s)", st->surfaces.size());
    for (auto &up : st->surfaces)
        klog("penance:   '%s' configured=%d egl=%d %dx%d",
             up->output_name.c_str(), up->configured,
             up->egl_surface != EGL_NO_SURFACE, up->width, up->height);
    request_all(*st);
}

void handle_finished(void *data, ext_session_lock_v1 *) {
    auto *st = static_cast<PenanceState *>(data);
    if (!st->penance)
        return;
    klog("penance: compositor sent finished");
    if (st->locked)
        ext_session_lock_v1_unlock_and_destroy(st->penance);
    else
        ext_session_lock_v1_destroy(st->penance);
    st->penance = nullptr;
    st->locked = false;
    penance_teardown(*st);
}

constexpr ext_session_lock_v1_listener kPenanceListener = {
    .locked = handle_locked,
    .finished = handle_finished,
};

void deliver_auth(PenanceState &st, uint64_t gen, pam_auth::Result res) {
    if (gen != st.auth_generation || !st.locked)
        return;
    st.authenticating = false;
    if (res.success) {
        penance_begin_unlock(st);
        return;
    }
    pam_auth::secure_clear(st.password.text);
    st.pw_anim.chars.clear();
    st.pw_row_slide = {};
    st.failed = true;
    st.fail_clear_at =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<int>(kPenanceTimerFailMs));
    request_all(st);
}

void try_authenticate(PenanceState &st) {
    if (st.authenticating || st.password.text.empty())
        return;
    st.authenticating = true;
    st.failed = false;
    uint64_t gen = ++st.auth_generation;
    std::string pw = st.password.text;
    std::thread([&st, gen, pw = std::move(pw)]() mutable {
        pam_auth::Result res = pam_auth::authenticate_current_user(pw);
        pam_auth::secure_clear(pw);
        DeferredCall::call_later(
            [&st, gen, res] { deliver_auth(st, gen, res); });
    }).detach();
    request_all(st);
}

void create_output_surface(PenanceState &st, wl_output *output,
                           const std::string &name) {
    auto los = std::make_unique<PenanceOutputSurface>();
    los->owner = &st;
    los->output = output;
    los->output_name = name;
    los->surface = wl_compositor_create_surface(st.app->compositor);
    los->penance_surface =
        ext_session_lock_v1_get_lock_surface(st.penance, los->surface, output);
    if (!los->penance_surface) {
        klog("penance: get_lock_surface failed on '%s'", name.c_str());
        wl_surface_destroy(los->surface);
        return;
    }
    ext_session_lock_surface_v1_add_listener(los->penance_surface,
                                             &kSurfaceListener, los.get());
    los->output_scale.on_change = [ptr = los.get()](int32_t s) {
        if (ptr->egl_window)
            wl_egl_window_resize(ptr->egl_window, ptr->width * s,
                                 ptr->height * s, 0, 0);
        if (ptr->frame_clock.surface)
            request_frame(ptr->frame_clock);
    };
    output_scale_watch(los->output_scale, los->surface);
    los->panel_gated = st.panel_gated_for && !st.panel_gated_for(name);
    st.surfaces.push_back(std::move(los));
}

void destroy_output_surface(PenanceState &st, PenanceOutputSurface &los) {
    if (los.frame_clock.callback) {
        wl_callback_destroy(los.frame_clock.callback);
        los.frame_clock.callback = nullptr;
    }
    if (los.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(st.app->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       st.app->egl_context);
        eglDestroySurface(st.app->egl_display, los.egl_surface);
        los.egl_surface = EGL_NO_SURFACE;
    }
    if (los.egl_window) {
        wl_egl_window_destroy(los.egl_window);
        los.egl_window = nullptr;
    }
    if (los.penance_surface) {
        ext_session_lock_surface_v1_destroy(los.penance_surface);
        los.penance_surface = nullptr;
    }
    if (los.surface) {
        wl_surface_destroy(los.surface);
        los.surface = nullptr;
    }
}

void finish_unlock(PenanceState &st) {
    if (!st.penance)
        return;
    ext_session_lock_v1_unlock_and_destroy(st.penance);
    st.penance = nullptr;
    wl_display_roundtrip(st.app->display);
    penance_teardown(st);
    klog("penance: session unlocked");
}

} // namespace

bool penance_request(PenanceState &st, WaylandState &app) {
    if (st.active)
        return true;
    if (!app.session_lock_manager) {
        klog("penance: compositor has no ext_session_lock_manager_v1");
        return false;
    }
    st.app = &app;
    st.penance = ext_session_lock_manager_v1_lock(app.session_lock_manager);
    if (!st.penance) {
        klog("penance: failed to create session lock");
        return false;
    }
    ext_session_lock_v1_add_listener(st.penance, &kPenanceListener, &st);

    st.active = true;
    st.locked = false;
    st.unlocking = false;
    st.failed = false;
    st.authenticating = false;
    st.password.text.clear();
    st.pw_anim.chars.clear();
    st.pw_row_slide = {};
    if (st.user.empty())
        st.user = user_info::username();

    AnimatedImageStyle avatar_style;
    avatar_style.size = kPenanceProfileSize;
    avatar_style.circular = true;
    avatar_style.border_width = kPenanceProfileBorderWidth;
    avatar_style.decode = {static_cast<int>(kPenanceAvatarFps),
                           static_cast<int>(kPenanceProfileSize) * 2};
    animated_image_set_source(st.avatar, user_info::profile_media_path(),
                              avatar_style);
    animated_image_show(st.avatar, [&st] { request_all(st); });

    for (auto &mon : app.outputs)
        create_output_surface(st, mon->output.wl, mon->output.name);

    wl_display_flush(app.display);
    klog("penance: requested (%zu surface(s))", st.surfaces.size());
    return true;
}

void penance_teardown(PenanceState &st) {
    for (auto &up : st.surfaces)
        destroy_output_surface(st, *up);
    st.surfaces.clear();
    animated_image_hide(st.avatar);
    st.active = false;
    st.locked = false;
    st.unlocking = false;
    if (st.app)
        st.app->session_locked = false;
    pam_auth::secure_clear(st.password.text);
    st.pw_anim.chars.clear();
    st.pw_row_slide = {};
    if (st.app) {
        app_detail::rest_egl_current(*st.app);
        for (auto &mon : st.app->outputs)
            request_all_frames(*mon);
        wl_display_flush(st.app->display);
    }
}

void penance_begin_unlock(PenanceState &st) {
    if (st.unlocking || !st.locked)
        return;
    st.unlocking = true;
    for (auto &up : st.surfaces) {
        start_unlock_anim(st, *up);
        if (up->frame_clock.surface)
            request_frame(up->frame_clock);
    }
    if (st.app)
        app_detail::rest_egl_current(*st.app);
}

void penance_handle_key(PenanceState &st, const KeyEvent &ev) {
    if (!st.locked || st.unlocking)
        return;
    TextFieldResult res = text_field_handle_key(st.password, ev);
    if (res == TextFieldResult::Committed) {
        try_authenticate(st);
        return;
    }
    if (res == TextFieldResult::Cancelled)
        pam_auth::secure_clear(st.password.text);
    if (res == TextFieldResult::Changed || res == TextFieldResult::Cancelled) {
        st.failed = false;
        PenanceOutputSurface *focus = nullptr;
        wl_surface *fs = penance_focused_surface(st);
        for (auto &up : st.surfaces)
            if (up->surface == fs)
                focus = up.get();
        if (focus)
            text_field_type_anim_sync(st.pw_anim, focus->animations,
                                      kPenanceOwnerDotBase, st.password.text);
        else
            st.pw_anim.chars.resize(text_field_utf8_len(st.password.text));
        request_all(st);
    }
}

void penance_handle_click(PenanceState &st, wl_surface *surf, double x,
                          double y) {
    PenanceOutputSurface *los = surface_for(st, surf);
    if (!los)
        return;
    if (x <= 140.0 && y >= static_cast<double>(los->height) - 48.0) {
        penance_begin_unlock(st);
        return;
    }
    if (st.unlocking || !st.app)
        return;

    auto hit = [x, y](const Rect &r) {
        return r.w > 0.0f && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };
    if (hit(los->pill_button)) {
        try_authenticate(st);
        return;
    }
    if (hit(los->media_prev)) {
        mpris_previous(st.app->mpris);
        request_all(st);
    } else if (hit(los->media_play)) {
        mpris_play_pause(st.app->mpris);
        request_all(st);
    } else if (hit(los->media_next)) {
        mpris_next(st.app->mpris);
        request_all(st);
    }
}

void penance_timer_tick(PenanceState &st) {
    if (!st.active)
        return;
    if (st.failed && std::chrono::steady_clock::now() >= st.fail_clear_at) {
        st.failed = false;
    }
    request_all(st);
}

void penance_hotplug_add(PenanceState &st, wl_output *output,
                         const char *name) {
    if (!st.active)
        return;
    for (auto &up : st.surfaces)
        if (up->output == output)
            return;
    create_output_surface(st, output, name ? name : "");
    wl_display_flush(st.app->display);
}

void penance_hotplug_remove(PenanceState &st, wl_output *output) {
    if (!st.active)
        return;
    auto it =
        std::find_if(st.surfaces.begin(), st.surfaces.end(),
                     [output](const std::unique_ptr<PenanceOutputSurface> &u) {
                         return u->output == output;
                     });
    if (it == st.surfaces.end())
        return;
    destroy_output_surface(st, **it);
    st.surfaces.erase(it);
}

wl_surface *penance_focused_surface(const PenanceState &st) {
    if (st.surfaces.empty())
        return nullptr;
    if (st.app && st.app->keyboard.focused_surface) {
        for (auto &up : st.surfaces)
            if (up->surface == st.app->keyboard.focused_surface)
                return up->surface;
    }
    return st.surfaces.front()->surface;
}

bool penance_owns_surface(const PenanceState &st, wl_surface *s) {
    if (!s)
        return false;
    for (auto &up : st.surfaces)
        if (up->surface == s)
            return true;
    return false;
}
