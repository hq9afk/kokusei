#pragma once

#include <cstdint>
#include <string>

#include "render/animation.h"

struct MarqueeTextState {
    float scroll_offset = 0.0f;
    bool marqueeing = false;
    std::string last_text;
    float last_box_w = -1.0f;
};

inline std::uint64_t marquee_owner(const MarqueeTextState &state) {
    return reinterpret_cast<std::uint64_t>(&state);
}

void marquee_scroll_update(AnimationManager &anim, MarqueeTextState &state,
                           const std::string &text, float text_width,
                           float box_w);
