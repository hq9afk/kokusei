#pragma once

#include <cstdint>
#include <string>

class TextInputService;

enum class TextInputPurpose { Normal, Password };

struct TextInputState {
    int32_t cursor_rect_x = 0;
    int32_t cursor_rect_y = 0;
    int32_t cursor_rect_w = 1;
    int32_t cursor_rect_h = 1;
    TextInputPurpose purpose = TextInputPurpose::Normal;
};

struct TextInputEdit {
    std::string commit_text;
    std::string preedit_text;
    uint32_t delete_before_length = 0;
    uint32_t delete_after_length = 0;
    bool has_commit_text = false;
    bool has_preedit = false;
    bool has_delete = false;
};

class TextInputClient {
  public:
    virtual ~TextInputClient() = default;

    virtual TextInputState text_input_state() const = 0;
    virtual void text_input_apply_edit(const TextInputEdit &edit) = 0;
    virtual void text_input_reset_preedit() = 0;
    virtual void text_input_activated(TextInputService &service) = 0;
    virtual void text_input_deactivated(TextInputService &service) = 0;
};
