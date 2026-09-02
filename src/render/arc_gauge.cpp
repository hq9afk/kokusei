#include <algorithm>
#include <cairo/cairo.h>
#include <cmath>

#include "render/arc_gauge.h"
#include "render/text.h"

const Texture *cached_arc_gauge(TextureCache &tcache, int32_t scale,
                                float diameter, float stroke, float value01,
                                const Color &fill_color) {
    int px_diameter = static_cast<int>(diameter * scale);
    int bucket =
        static_cast<int>(std::round(std::clamp(value01, 0.0f, 1.0f) * 100.0f));
    std::string key =
        "arc_gauge:" + std::to_string(px_diameter) + ":" +
        std::to_string(static_cast<int>(stroke * scale)) + ":" +
        std::to_string(bucket) + ":" +
        std::to_string(static_cast<int>(fill_color.r * 255)) + "," +
        std::to_string(static_cast<int>(fill_color.g * 255)) + "," +
        std::to_string(static_cast<int>(fill_color.b * 255));
    return tcache.get(key, [&]() -> RasterizedText {
        cairo_surface_t *surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, px_diameter, px_diameter);
        cairo_t *cr = cairo_create(surface);
        float cx = px_diameter / 2.0f, cy = px_diameter / 2.0f;
        float px_stroke = stroke * scale;
        float radius = px_diameter / 2.0f - px_stroke / 2.0f;
        float start = -static_cast<float>(M_PI) / 2.0f;
        float full = 2.0f * static_cast<float>(M_PI);

        cairo_set_line_width(cr, px_stroke);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        cairo_set_source_rgba(cr, fill_color.r, fill_color.g, fill_color.b,
                              0.15f);
        cairo_arc(cr, cx, cy, radius, 0.0, full);
        cairo_stroke(cr);

        float value = static_cast<float>(bucket) / 100.0f;
        if (value > 0.0f) {
            cairo_set_source_rgba(cr, fill_color.r, fill_color.g, fill_color.b,
                                  fill_color.a);
            cairo_arc(cr, cx, cy, radius, start, start + full * value);
            cairo_stroke(cr);
        }

        cairo_surface_flush(surface);
        RasterizedText result =
            surface_to_rgba(surface, px_diameter, px_diameter);
        result.scale = scale;
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return result;
    });
}

float draw_arc_gauge(Node *root, TextureCache &tcache, int32_t scale, float x,
                     float y, float diameter, float stroke, float value01,
                     const Color &fill_color, const Texture *icon_tex,
                     const float *icon_color, const Texture *value_tex,
                     const float *value_color, const Texture *sub_tex,
                     const float *sub_label_color, float icon_value_gap,
                     float label_spacing) {
    const Texture *gauge_tex =
        cached_arc_gauge(tcache, scale, diameter, stroke, value01, fill_color);
    if (gauge_tex)
        node_add_texture(root, std::round(x), std::round(y), *gauge_tex,
                         rgba(palette::text));

    float icon_h = icon_tex ? icon_tex->height : 0.0f;
    float value_h = value_tex ? value_tex->height : 0.0f;
    float gap = icon_tex && value_tex ? icon_value_gap : 0.0f;
    float stack_y = y + (diameter - icon_h - gap - value_h) / 2.0f;

    if (icon_tex) {
        node_add_texture(root,
                         std::round(x + (diameter - icon_tex->width) / 2.0f),
                         std::round(stack_y), *icon_tex, icon_color);
        stack_y += icon_h + gap;
    }
    if (value_tex)
        node_add_texture(root,
                         std::round(x + (diameter - value_tex->width) / 2.0f),
                         std::round(stack_y), *value_tex, value_color);

    float bottom = y + diameter;
    if (sub_tex) {
        float sub_y = y + diameter + label_spacing;
        node_add_texture(root,
                         std::round(x + (diameter - sub_tex->width) / 2.0f),
                         std::round(sub_y), *sub_tex, sub_label_color);
        bottom = sub_y + sub_tex->height;
    }
    return bottom;
}
