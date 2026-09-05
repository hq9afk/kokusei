#include <array>
#include <cstdio>

#include "modules/trulla/resonance_tab.h"

#include "render/icons.h"

using panel_chrome_detail::cached_icon;
using panel_chrome_detail::cached_text;

namespace {

struct KnobRow {
    TrullaFieldId id;
    const char *label;
    const char *reset_tag;
};

constexpr std::array<KnobRow, 6> kKnobs = {{
    {TrullaFieldId::ResonanceFps, "Target framerate", "resonancefpsreset"},
    {TrullaFieldId::ResonanceParticleThin, "Particle grid density",
     "resonancethinreset"},
    {TrullaFieldId::ResonanceParticleSize, "Particle size",
     "resonancesizereset"},
    {TrullaFieldId::ResonanceComplexity, "Fractal complexity",
     "resonancecomplexityreset"},
    {TrullaFieldId::ResonanceGlowDirections, "Glow directions",
     "resonanceglowdirreset"},
    {TrullaFieldId::ResonanceGlowQuality, "Glow quality",
     "resonanceglowqualreset"},
}};

bool knob_is_float(TrullaFieldId id) {
    return id == TrullaFieldId::ResonanceParticleThin ||
           id == TrullaFieldId::ResonanceGlowDirections ||
           id == TrullaFieldId::ResonanceGlowQuality;
}

float knob_value(const ResonanceParams &p, TrullaFieldId id) {
    switch (id) {
    case TrullaFieldId::ResonanceFps:
        return static_cast<float>(p.fps);
    case TrullaFieldId::ResonanceParticleThin:
        return p.particle_thin;
    case TrullaFieldId::ResonanceParticleSize:
        return static_cast<float>(p.particle_size);
    case TrullaFieldId::ResonanceComplexity:
        return static_cast<float>(p.fractal_complexity);
    case TrullaFieldId::ResonanceGlowDirections:
        return p.glow_directions;
    case TrullaFieldId::ResonanceGlowQuality:
        return p.glow_quality;
    default:
        return 0.0f;
    }
}

float knob_default(TrullaFieldId id) {
    ResonanceParams d;
    return knob_value(d, id);
}

void reset_knob(ResonanceParams &p, TrullaFieldId id) {
    ResonanceParams d;
    switch (id) {
    case TrullaFieldId::ResonanceFps:
        p.fps = d.fps;
        break;
    case TrullaFieldId::ResonanceParticleThin:
        p.particle_thin = d.particle_thin;
        break;
    case TrullaFieldId::ResonanceParticleSize:
        p.particle_size = d.particle_size;
        break;
    case TrullaFieldId::ResonanceComplexity:
        p.fractal_complexity = d.fractal_complexity;
        break;
    case TrullaFieldId::ResonanceGlowDirections:
        p.glow_directions = d.glow_directions;
        break;
    case TrullaFieldId::ResonanceGlowQuality:
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

std::string resonance_field_text(const ResonanceParams &params,
                                 TrullaFieldId id) {
    if (knob_is_float(id))
        return trim_float(knob_value(params, id));
    return std::to_string(static_cast<int>(knob_value(params, id)));
}

void resonance_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                         float y, float w, const Config &cfg) {
    const ResonanceParams &p = cfg.resonance;

    for (const KnobRow &knob : kKnobs) {
        float h = kTrullaToggleTileHeight;
        node_add_rrect(
            root, x, y, w, h, kTrullaTileRadius, kTrullaToggleTileBorderWidth,
            rgba(palette::text_alpha04), rgba(palette::text_alpha07));
        float inset = kTrullaToggleTileContentMargin;

        const Texture *label_tex = cached_text(state.tcache, knob.label, scale);
        if (label_tex)
            node_add_texture(root, x + inset,
                             y + (h - label_tex->height) / 2.0f, *label_tex,
                             rgba(palette::text_alpha85));

        float field_w = kTrullaNumberFieldWidth;
        float field_x = x + w - inset - field_w;
        float field_y = y + (h - kTrullaFieldHeight) / 2.0f;
        float reset_x = field_x - kTrullaToggleTileContentSpacing -
                        kTrullaIdleResetIconSize;

        bool focused = state.focused_field == knob.id;
        node_add_rrect(root, field_x, field_y, field_w, kTrullaFieldHeight,
                       metrics::radius_sm, metrics::border_thin,
                       rgba(palette::field_bg),
                       focused ? rgba(palette::accent) : kPanelNoBorder);
        float field_center_y = field_y + kTrullaFieldHeight / 2.0f;
        if (focused) {
            float advance = draw_text_field_value(
                root, state.tcache, scale, state.field_buffer.text, field_x + 8,
                field_center_y, rgba(palette::text), &state.field_anim);
            float cursor_x = field_x + 8 + advance + 2;
            draw_text_field_preedit(root, state.tcache, scale,
                                    state.field_buffer.preedit, cursor_x,
                                    field_center_y, rgba(palette::text));
            Rect caret = {cursor_x, field_y + 5, 1.5f, kTrullaFieldHeight - 10};
            state.field_buffer.cursor_rect = caret;
            draw_text_field_caret(root, state.field_buffer, caret,
                                  rgba(palette::text), true);
        } else {
            const Texture *value_tex = cached_text(
                state.tcache, resonance_field_text(p, knob.id), scale);
            if (value_tex)
                node_add_texture(
                    root, field_x + 8,
                    field_y + (kTrullaFieldHeight - value_tex->height) / 2.0f,
                    *value_tex, rgba(palette::text));
        }
        state.click_regions.push_back(
            {PanelClickKind::FieldFocus,
             {field_x, field_y, field_w, kTrullaFieldHeight},
             std::to_string(static_cast<int>(knob.id))});

        if (knob_value(p, knob.id) != knob_default(knob.id)) {
            float reset_y = y + (h - kTrullaIdleResetIconSize) / 2.0f;
            const Texture *reset_icon =
                cached_icon(state.tcache, icon::refresh, scale);
            if (reset_icon)
                node_add_texture(
                    root,
                    reset_x +
                        (kTrullaIdleResetIconSize - reset_icon->width) / 2.0f,
                    reset_y +
                        (kTrullaIdleResetIconSize - reset_icon->height) / 2.0f,
                    *reset_icon, rgba(palette::text_dim));
            state.click_regions.push_back(
                {PanelClickKind::ToggleFlip,
                 {reset_x, reset_y, kTrullaIdleResetIconSize,
                  kTrullaIdleResetIconSize},
                 knob.reset_tag});
        }

        y += kTrullaToggleTileHeight + kPanelRowGap;
    }
}

bool resonance_tab_handle_click(TrullaState &state, const Config &cfg,
                                const TrullaCommitFn &on_commit,
                                const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;
    for (const KnobRow &knob : kKnobs) {
        if (region.tag != knob.reset_tag)
            continue;
        trulla_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        reset_knob(updated.resonance, knob.id);
        on_commit(updated);
        trulla_request_frame(state);
        return true;
    }
    return false;
}
