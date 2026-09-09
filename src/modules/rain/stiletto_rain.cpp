#include <algorithm>
#include <cairo/cairo.h>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <librsvg/rsvg.h>
#include <random>

#include "config/rain_config.h"

#include "modules/rain/stiletto_rain.h"

#include "render/palette.h"
#include "render/text.h"

namespace {

std::mt19937 &rng() {
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

float random01() {
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng());
}

float random_range(float lo, float hi) { return lo + (hi - lo) * random01(); }

cairo_surface_t *stiletto_sprite() {
    static cairo_surface_t *sprite = []() -> cairo_surface_t * {
        const char *candidates[] = {KOKUSEI_STILETTO_SPRITE,
                                    "assets/stiletto.svg"};
        const char *path = candidates[1];
        for (const char *c : candidates) {
            if (std::filesystem::exists(c)) {
                path = c;
                break;
            }
        }
        GError *error = nullptr;
        RsvgHandle *handle = rsvg_handle_new_from_file(path, &error);
        if (!handle) {
            if (error)
                g_error_free(error);
            return nullptr;
        }
        double aspect = 1.0;
        gdouble nat_w = 0.0, nat_h = 0.0;
        if (rsvg_handle_get_intrinsic_size_in_pixels(handle, &nat_w, &nat_h) &&
            nat_h > 0.0)
            aspect = nat_w / nat_h;
        int h = static_cast<int>(kStilettoRainHeadHeightPx);
        int w = std::max(1, static_cast<int>(std::lround(h * aspect)));
        cairo_surface_t *s =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        cairo_t *cr = cairo_create(s);
        RsvgRectangle viewport = {0.0, 0.0, static_cast<double>(w),
                                  static_cast<double>(h)};
        rsvg_handle_render_document(handle, cr, &viewport, nullptr);
        cairo_destroy(cr);
        cairo_surface_flush(s);
        g_object_unref(handle);
        return s;
    }();
    return sprite;
}

void draw_sprite_centered(cairo_t *cr, cairo_surface_t *sprite, float cx,
                          float y, const Color &color) {
    if (!sprite)
        return;
    float sw = static_cast<float>(cairo_image_surface_get_width(sprite));
    float sh = static_cast<float>(cairo_image_surface_get_height(sprite));
    float tx = cx - sw / 2.0f;
    float ty = y - sh / 2.0f;
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    cairo_mask_surface(cr, sprite, std::round(tx), std::round(ty));
}

void draw_trail_segment(cairo_t *cr, float x, float y0, float y1) {
    const Color &a = palette::accent;
    cairo_set_source_rgba(cr, a.r, a.g, a.b, a.a);
    cairo_set_line_width(cr, kStilettoRainTrailWidthPx);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, std::round(x), y0);
    cairo_line_to(cr, std::round(x), y1);
    cairo_stroke(cr);
}

} // namespace

void StilettoRain::rebuild(int width, int height, bool async_speed) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);

    column_count_ =
        std::max(1, static_cast<int>(width_ / kStilettoRainColumnSpacingPx));
    float content_w = (column_count_ - 1) * kStilettoRainColumnSpacingPx;
    origin_x_ = (width_ - content_w) / 2.0f;

    comets_.assign(static_cast<size_t>(column_count_), Comet{});
    for (Comet &c : comets_) {
        c.drop = start_drop();
        c.speed = async_speed
                      ? random_range(kRainAsyncSpeedMin, kRainAsyncSpeedMax)
                      : 1.0f;
    }

    sweeping_ = true;
    sweep_drop_ = start_drop();

    stride_ = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width_);
    buffer_.assign(static_cast<size_t>(stride_) * static_cast<size_t>(height_),
                   0);
    frame_.assign(buffer_.size(), 0);
    texture_ = Texture{};
}

void StilettoRain::decay() {
    int alpha_step =
        std::clamp(static_cast<int>(255.0f * kRainFadeAlpha), 1, 255);
    for (int y = 0; y < height_; ++y) {
        uint8_t *row = buffer_.data() + static_cast<size_t>(y) * stride_;
        for (int x = 0; x < width_; ++x) {
            uint8_t *px = row + x * 4;
            int old_alpha = px[3];
            if (old_alpha == 0)
                continue;
            int new_alpha = std::max(0, old_alpha - alpha_step);
            if (new_alpha == 0) {
                px[0] = px[1] = px[2] = px[3] = 0;
                continue;
            }
            px[0] = static_cast<uint8_t>(px[0] * new_alpha / old_alpha);
            px[1] = static_cast<uint8_t>(px[1] * new_alpha / old_alpha);
            px[2] = static_cast<uint8_t>(px[2] * new_alpha / old_alpha);
            px[3] = static_cast<uint8_t>(new_alpha);
        }
    }
}

void StilettoRain::tick() {
    if (buffer_.empty() || comets_.empty())
        return;

    decay();

    cairo_surface_t *sprite = stiletto_sprite();

    cairo_surface_t *trail = cairo_image_surface_create_for_data(
        buffer_.data(), CAIRO_FORMAT_ARGB32, width_, height_, stride_);
    cairo_t *tcr = cairo_create(trail);
    for (int i = 0; i < column_count_; ++i) {
        Comet &c = comets_[static_cast<size_t>(i)];
        float x = origin_x_ + i * kStilettoRainColumnSpacingPx;

        float head = sweeping_ ? sweep_drop_ : c.drop;
        if (head >= 0.0f) {
            if (c.last_valid)
                draw_trail_segment(tcr, x, c.last_drop, head);
            c.last_drop = head;
            c.last_valid = true;
        } else {
            c.last_valid = false;
        }
    }
    cairo_destroy(tcr);
    cairo_surface_flush(trail);
    cairo_surface_destroy(trail);

    frame_ = buffer_;
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        frame_.data(), CAIRO_FORMAT_ARGB32, width_, height_, stride_);
    cairo_t *cr = cairo_create(surface);
    for (int i = 0; i < column_count_; ++i) {
        const Comet &c = comets_[static_cast<size_t>(i)];
        float x = origin_x_ + i * kStilettoRainColumnSpacingPx;
        float head = sweeping_ ? sweep_drop_ : c.drop;
        if (head >= 0.0f)
            draw_sprite_centered(cr, sprite, x, head, palette::text);
    }

    if (sweeping_) {
        sweep_drop_ += kStilettoRainStepPx;
        if (sweep_drop_ > static_cast<float>(height_) + kStilettoRainStepPx) {
            sweeping_ = false;
            for (Comet &c : comets_) {
                c.drop = -(random01() * static_cast<float>(height_));
                c.ever_reset = true;
                c.last_valid = false;
            }
        }
    } else {
        for (Comet &c : comets_) {
            c.drop += kStilettoRainStepPx * c.speed;
            if (c.drop > static_cast<float>(height_) &&
                random01() < kRainResetChance) {
                c.drop = c.ever_reset
                             ? -(random01() * static_cast<float>(height_))
                             : start_drop();
                c.ever_reset = true;
                c.last_valid = false;
            }
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(surface);

    RasterizedText raster = surface_to_rgba(surface, width_, height_);
    cairo_surface_destroy(surface);

    texture_ =
        make_texture_rgba(raster.width, raster.height, raster.rgba.data());
}
