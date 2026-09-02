#pragma once

#include <cstdint>
#include <string>

#include "render/animation.h"
#include "render/marquee_scroll.h"
#include "render/node.h"
#include "render/texture_cache.h"

void draw_marquee_text(Node *parent, TextureCache &cache,
                       AnimationManager &anim, MarqueeTextState &state,
                       std::int32_t scale, const std::string &text, float x,
                       float y, float w, const float *color);
