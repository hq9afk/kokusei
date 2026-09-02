#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

#include "render/animation.h"
#include "render/node.h"
#include "render/rect.h"
#include "render/texture_cache.h"

#include "service/input_service.h"

void text_field_backspace(std::string &text);

struct TextFieldState {
    std::string text;
    std::string preedit;
    bool cursor_blink_visible = true;
    Rect cursor_rect;
};

enum class TextFieldResult { None, Changed, Committed, Cancelled };

TextFieldResult text_field_handle_key(TextFieldState &field,
                                      const KeyEvent &event);

size_t text_field_utf8_len(const std::string &text);

bool text_field_blink_toggle(TextFieldState &field);

void draw_text_field_caret(Node *parent, const TextFieldState &field,
                           Rect caret, const float *color, bool active);

void draw_text_field_preedit(Node *parent, TextureCache &tcache, int32_t scale,
                             const std::string &preedit, float x,
                             float center_y, const float *color);

inline constexpr float kTextFieldPopScaleMs = 200.0f;
inline constexpr float kTextFieldPopSlideMs = 80.0f;
inline constexpr float kTextFieldPopSlideOffsetPx = 8.0f;
inline constexpr float kTextFieldRowSlideMs = 200.0f;
inline constexpr size_t kTextFieldTypeAnimMax = 256;

struct TextFieldCharAnim {
    float scale = 1.0f;
    float slide_x = 0.0f;
};

struct TextFieldTypeAnim {
    std::deque<TextFieldCharAnim> chars;
};

struct TextFieldRowSlide {
    float x = 0.0f;
    float target = 0.0f;
    bool primed = false;
};

void text_field_type_anim_sync(TextFieldTypeAnim &anim, AnimationManager &am,
                               uint64_t owner_base, const std::string &text);

void text_field_type_anim_settle(TextFieldTypeAnim &anim, AnimationManager &am,
                                 uint64_t owner_base, const std::string &text);

void text_field_type_anim_clear(TextFieldTypeAnim &anim, AnimationManager &am,
                                uint64_t owner_base);

float text_field_row_slide(TextFieldRowSlide &slide, AnimationManager &am,
                           uint64_t owner, float anchor_x);

void text_field_row_slide_reset(TextFieldRowSlide &slide, AnimationManager &am,
                                uint64_t owner);

float draw_text_field_value(Node *parent, TextureCache &tcache, int32_t scale,
                            const std::string &text, float x, float center_y,
                            const float *color, const TextFieldTypeAnim *anim);
