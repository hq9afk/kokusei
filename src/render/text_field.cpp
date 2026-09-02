#include <algorithm>

#include "render/panel_chrome.h"
#include "render/text.h"
#include "render/text_field.h"

namespace {

size_t utf8_char_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00)
        return 1;
    if ((lead & 0xE0) == 0xC0)
        return 2;
    if ((lead & 0xF0) == 0xE0)
        return 3;
    if ((lead & 0xF8) == 0xF0)
        return 4;
    return 1;
}

uint64_t type_anim_owner(uint64_t base, size_t index, uint64_t prop) {
    return base + (index % kTextFieldTypeAnimMax) * 2 + prop;
}

void type_anim_push(TextFieldTypeAnim &anim, AnimationManager &am,
                    uint64_t owner_base) {
    size_t idx = anim.chars.size();
    anim.chars.push_back({0.0f, kTextFieldPopSlideOffsetPx});
    am.animate(
        0.0f, 1.0f, kTextFieldPopScaleMs, Easing::EaseOutBack,
        [&anim, idx](float v) {
            if (idx < anim.chars.size())
                anim.chars[idx].scale = v;
        },
        {}, type_anim_owner(owner_base, idx, 0));
    am.animate(
        kTextFieldPopSlideOffsetPx, 0.0f, kTextFieldPopSlideMs, Easing::Linear,
        [&anim, idx](float v) {
            if (idx < anim.chars.size())
                anim.chars[idx].slide_x = v;
        },
        {}, type_anim_owner(owner_base, idx, 1));
}

void type_anim_pop(TextFieldTypeAnim &anim, AnimationManager &am,
                   uint64_t owner_base) {
    if (anim.chars.empty())
        return;
    size_t idx = anim.chars.size() - 1;
    am.cancelForOwner(type_anim_owner(owner_base, idx, 0));
    am.cancelForOwner(type_anim_owner(owner_base, idx, 1));
    anim.chars.pop_back();
}

} // namespace

void text_field_backspace(std::string &text) {
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80)
        text.pop_back();
    if (!text.empty())
        text.pop_back();
}

TextFieldResult text_field_handle_key(TextFieldState &field,
                                      const KeyEvent &event) {
    switch (event.kind) {
    case KeyKind::Text:
        field.text += event.text;
        field.preedit.clear();
        field.cursor_blink_visible = true;
        return TextFieldResult::Changed;
    case KeyKind::Preedit:
        field.preedit = event.text;
        field.cursor_blink_visible = true;
        return TextFieldResult::Changed;
    case KeyKind::Backspace:
        text_field_backspace(field.text);
        field.preedit.clear();
        field.cursor_blink_visible = true;
        return TextFieldResult::Changed;
    case KeyKind::Enter:
        field.preedit.clear();
        return TextFieldResult::Committed;
    case KeyKind::Escape:
        field.preedit.clear();
        return TextFieldResult::Cancelled;
    default:
        return TextFieldResult::None;
    }
}

size_t text_field_utf8_len(const std::string &text) {
    size_t n = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80)
            ++n;
    return n;
}

bool text_field_blink_toggle(TextFieldState &field) {
    field.cursor_blink_visible = !field.cursor_blink_visible;
    return true;
}

void draw_text_field_caret(Node *parent, const TextFieldState &field,
                           Rect caret, const float *color, bool active) {
    if (active && field.cursor_blink_visible)
        node_add_rect(parent, caret.x, caret.y, caret.w, caret.h, color);
}

void draw_text_field_preedit(Node *parent, TextureCache &tcache, int32_t scale,
                             const std::string &preedit, float x,
                             float center_y, const float *color) {
    if (preedit.empty())
        return;
    const Texture *tex =
        panel_chrome_detail::cached_text(tcache, preedit, scale);
    if (!tex)
        return;
    float y = center_y - static_cast<float>(tex->height) / 2.0f;
    node_add_texture(parent, x, y, *tex, color);
    node_add_rect(parent, x, y + static_cast<float>(tex->height),
                  static_cast<float>(tex->width), 1.0f, color);
}

void text_field_type_anim_sync(TextFieldTypeAnim &anim, AnimationManager &am,
                               uint64_t owner_base, const std::string &text) {
    size_t target = text_field_utf8_len(text);
    while (anim.chars.size() > target)
        type_anim_pop(anim, am, owner_base);
    while (anim.chars.size() < target)
        type_anim_push(anim, am, owner_base);
}

void text_field_type_anim_settle(TextFieldTypeAnim &anim, AnimationManager &am,
                                 uint64_t owner_base, const std::string &text) {
    text_field_type_anim_clear(anim, am, owner_base);
    size_t n = text_field_utf8_len(text);
    for (size_t i = 0; i < n; ++i)
        anim.chars.push_back({1.0f, 0.0f});
}

void text_field_type_anim_clear(TextFieldTypeAnim &anim, AnimationManager &am,
                                uint64_t owner_base) {
    while (!anim.chars.empty())
        type_anim_pop(anim, am, owner_base);
}

float text_field_row_slide(TextFieldRowSlide &slide, AnimationManager &am,
                           uint64_t owner, float anchor_x) {
    if (!slide.primed) {
        slide.primed = true;
        slide.target = anchor_x;
        slide.x = anchor_x;
        return slide.x;
    }
    if (anchor_x != slide.target) {
        slide.target = anchor_x;
        am.cancelForOwner(owner);
        am.animate(
            slide.x, anchor_x, kTextFieldRowSlideMs, Easing::EaseOutCubic,
            [&slide](float v) { slide.x = v; }, {}, owner);
    }
    return slide.x;
}

void text_field_row_slide_reset(TextFieldRowSlide &slide, AnimationManager &am,
                                uint64_t owner) {
    am.cancelForOwner(owner);
    slide.primed = false;
    slide.x = 0.0f;
    slide.target = 0.0f;
}

float draw_text_field_value(Node *parent, TextureCache &tcache, int32_t scale,
                            const std::string &text, float x, float center_y,
                            const float *color, const TextFieldTypeAnim *anim) {
    float cell_w = kokusei_text_advance();

    float cx = x;
    size_t char_index = 0;
    for (size_t i = 0; i < text.size();) {
        size_t len =
            std::min(utf8_char_len(static_cast<unsigned char>(text[i])),
                     text.size() - i);
        std::string ch = text.substr(i, len);
        i += len;

        const TextFieldCharAnim *ca = anim && char_index < anim->chars.size()
                                          ? &anim->chars[char_index]
                                          : nullptr;
        float glyph_scale = ca ? ca->scale : 1.0f;
        float slide = ca ? ca->slide_x : 0.0f;

        const Texture *ch_tex =
            panel_chrome_detail::cached_text(tcache, ch, scale);
        if (ch_tex && glyph_scale > 0.0f) {
            float inv = 1.0f / static_cast<float>(
                                   ch_tex->scale > 0 ? ch_tex->scale : 1);
            float w = static_cast<float>(ch_tex->width) * inv * glyph_scale;
            float h = static_cast<float>(ch_tex->height) * inv * glyph_scale;
            float cell_center_x = cx + cell_w / 2.0f + slide;
            node_add_texture_rect(parent, cell_center_x - w / 2.0f,
                                  center_y - h / 2.0f, w, h, *ch_tex, color);
        }
        cx += cell_w;
        ++char_index;
    }
    return cx - x;
}
