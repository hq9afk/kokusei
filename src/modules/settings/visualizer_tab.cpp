#include <array>
#include <cstdio>

#include "modules/settings/visualizer_tab.h"

#include "render/icons.h"

using panel_chrome_detail::cached_icon;
using panel_chrome_detail::cached_text;

namespace {

struct KnobRow {
    SettingsFieldId id;
    const char *label;
    const char *reset_tag;
};

constexpr std::array<KnobRow, 6> kKnobs = {{
    {SettingsFieldId::VisualizerFps, "Target framerate", "visualizerfpsreset"},
    {SettingsFieldId::VisualizerParticleThin, "Particle grid density",
     "visualizerthinreset"},
    {SettingsFieldId::VisualizerParticleSize, "Particle size",
     "visualizersizereset"},
    {SettingsFieldId::VisualizerComplexity, "Fractal complexity",
     "visualizercomplexityreset"},
    {SettingsFieldId::VisualizerGlowDirections, "Glow directions",
     "visualizerglowdirreset"},
    {SettingsFieldId::VisualizerGlowQuality, "Glow quality",
     "visualizerglowqualreset"},
}};

bool knob_is_float(SettingsFieldId id) {
    return id == SettingsFieldId::VisualizerParticleThin ||
           id == SettingsFieldId::VisualizerGlowDirections ||
           id == SettingsFieldId::VisualizerGlowQuality;
}

float knob_value(const VisualizerParams &p, SettingsFieldId id) {
    switch (id) {
    case SettingsFieldId::VisualizerFps:
        return static_cast<float>(p.fps);
    case SettingsFieldId::VisualizerParticleThin:
        return p.particle_thin;
    case SettingsFieldId::VisualizerParticleSize:
        return static_cast<float>(p.particle_size);
    case SettingsFieldId::VisualizerComplexity:
        return static_cast<float>(p.fractal_complexity);
    case SettingsFieldId::VisualizerGlowDirections:
        return p.glow_directions;
    case SettingsFieldId::VisualizerGlowQuality:
        return p.glow_quality;
    default:
        return 0.0f;
    }
}

float knob_default(SettingsFieldId id) {
    VisualizerParams d;
    return knob_value(d, id);
}

void reset_knob(VisualizerParams &p, SettingsFieldId id) {
    VisualizerParams d;
    switch (id) {
    case SettingsFieldId::VisualizerFps:
        p.fps = d.fps;
        break;
    case SettingsFieldId::VisualizerParticleThin:
        p.particle_thin = d.particle_thin;
        break;
    case SettingsFieldId::VisualizerParticleSize:
        p.particle_size = d.particle_size;
        break;
    case SettingsFieldId::VisualizerComplexity:
        p.fractal_complexity = d.fractal_complexity;
        break;
    case SettingsFieldId::VisualizerGlowDirections:
        p.glow_directions = d.glow_directions;
        break;
    case SettingsFieldId::VisualizerGlowQuality:
        p.glow_quality = d.glow_quality;
        break;
    default:
        break;
    }
}

std::string trim_float(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s(buf);
    while (s.find('.') != std::string::npos &&
           (s.back() == '0' || s.back() == '.'))
        s.pop_back();
    return s;
}

} // namespace

std::string visualizer_field_text(const VisualizerParams &params,
                                 SettingsFieldId id) {
    if (knob_is_float(id))
        return trim_float(knob_value(params, id));
    return std::to_string(static_cast<int>(knob_value(params, id)));
}

void visualizer_tab_paint(SettingsState &state, Node *root, int32_t scale, float x,
                         float y, float w, const Config &cfg) {
    const VisualizerParams &p = cfg.visualizer;

    {
        static const char *kShapeLabels[2] = {"Bar", "Sphere"};
        static const char *kShapeTags[2] = {"visualizershapebar",
                                            "visualizershapesphere"};
        bool active_flags[2] = {
            p.visualizer_shape == VisualizerShape::Bar,
            p.visualizer_shape == VisualizerShape::Sphere};

        float tile_w = (w - kSettingsScreenSelectorSpacing) / 2.0f;
        float cx = x;
        for (int i = 0; i < 2; ++i) {
            bool active = active_flags[i];
            node_add_rrect(root, cx, y, tile_w, kSettingsScreenSelectorHeight,
                           kSettingsTileRadius, kSettingsSelectorBorderWidth,
                           rgba(palette::lavender_alpha20),
                           active ? rgba(palette::accent_alt) : kPanelNoBorder);
            const Texture *tex =
                cached_text(state.tcache, kShapeLabels[i], scale);
            if (tex)
                node_add_texture(
                    root, cx + (tile_w - tex->width) / 2.0f,
                    y + (kSettingsScreenSelectorHeight - tex->height) / 2.0f,
                    *tex, rgba(palette::text));
            state.click_regions.push_back(
                {PanelClickKind::ToggleFlip,
                 {cx, y, tile_w, kSettingsScreenSelectorHeight},
                 kShapeTags[i]});
            cx += tile_w + kSettingsScreenSelectorSpacing;
        }

        y += kSettingsScreenSelectorHeight + kPanelRowGap;
    }

    if (p.visualizer_shape != VisualizerShape::Sphere)
        return;

    for (const KnobRow &knob : kKnobs) {
        float h = kSettingsToggleTileHeight;
        node_add_rrect(
            root, x, y, w, h, kSettingsTileRadius, kSettingsToggleTileBorderWidth,
            rgba(palette::text_alpha04), rgba(palette::text_alpha07));
        float inset = kSettingsToggleTileContentMargin;

        const Texture *label_tex = cached_text(state.tcache, knob.label, scale);
        if (label_tex)
            node_add_texture(root, x + inset,
                             y + (h - label_tex->height) / 2.0f, *label_tex,
                             rgba(palette::text_alpha85));

        float field_w = kSettingsNumberFieldWidth;
        float field_x = x + w - inset - field_w;
        float field_y = y + (h - kSettingsFieldHeight) / 2.0f;
        float reset_x = field_x - kSettingsToggleTileContentSpacing -
                        kSettingsIdleResetIconSize;

        bool focused = state.focused_field == knob.id;
        node_add_rrect(root, field_x, field_y, field_w, kSettingsFieldHeight,
                       metrics::radius_sm, metrics::border_thin,
                       rgba(palette::field_bg),
                       focused ? rgba(palette::accent) : kPanelNoBorder);
        float field_center_y = field_y + kSettingsFieldHeight / 2.0f;
        if (focused) {
            float advance = draw_text_field_value(
                root, state.tcache, scale, state.field_buffer.text, field_x + 8,
                field_center_y, rgba(palette::text), &state.field_anim);
            float cursor_x = field_x + 8 + advance + 2;
            draw_text_field_preedit(root, state.tcache, scale,
                                    state.field_buffer.preedit, cursor_x,
                                    field_center_y, rgba(palette::text));
            Rect caret = {cursor_x, field_y + 5, 1.5f, kSettingsFieldHeight - 10};
            state.field_buffer.cursor_rect = caret;
            draw_text_field_caret(root, state.field_buffer, caret,
                                  rgba(palette::text), true);
        } else {
            const Texture *value_tex = cached_text(
                state.tcache, visualizer_field_text(p, knob.id), scale);
            if (value_tex)
                node_add_texture(
                    root, field_x + 8,
                    field_y + (kSettingsFieldHeight - value_tex->height) / 2.0f,
                    *value_tex, rgba(palette::text));
        }
        state.click_regions.push_back(
            {PanelClickKind::FieldFocus,
             {field_x, field_y, field_w, kSettingsFieldHeight},
             std::to_string(static_cast<int>(knob.id))});

        if (knob_value(p, knob.id) != knob_default(knob.id)) {
            float reset_y = y + (h - kSettingsIdleResetIconSize) / 2.0f;
            const Texture *reset_icon =
                cached_icon(state.tcache, icon::refresh, scale);
            if (reset_icon)
                node_add_texture(
                    root,
                    reset_x +
                        (kSettingsIdleResetIconSize - reset_icon->width) / 2.0f,
                    reset_y +
                        (kSettingsIdleResetIconSize - reset_icon->height) / 2.0f,
                    *reset_icon, rgba(palette::text_dim));
            state.click_regions.push_back(
                {PanelClickKind::ToggleFlip,
                 {reset_x, reset_y, kSettingsIdleResetIconSize,
                  kSettingsIdleResetIconSize},
                 knob.reset_tag});
        }

        y += kSettingsToggleTileHeight + kPanelRowGap;
    }
}

bool visualizer_tab_handle_click(SettingsState &state, const Config &cfg,
                                const SettingsCommitFn &on_commit,
                                const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;

    if (region.tag == "visualizershapesphere" ||
        region.tag == "visualizershapebar") {
        VisualizerShape shape = region.tag == "visualizershapesphere"
                                             ? VisualizerShape::Sphere
                                             : VisualizerShape::Bar;
        if (cfg.visualizer.visualizer_shape != shape) {
            settings_commit_focused_field(state, cfg, on_commit);
            Config updated = cfg;
            updated.visualizer.visualizer_shape = shape;
            on_commit(updated);
            settings_request_frame(state);
        }
        return true;
    }

    for (const KnobRow &knob : kKnobs) {
        if (region.tag != knob.reset_tag)
            continue;
        settings_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        reset_knob(updated.visualizer, knob.id);
        on_commit(updated);
        settings_request_frame(state);
        return true;
    }
    return false;
}
