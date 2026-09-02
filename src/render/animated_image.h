#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "render/node.h"

#include "service/media_service.h"

struct AnimatedImageStyle {
    float size = 0;
    bool circular = false;
    float border_width = 0;
    const float *border_color = nullptr;
    const float *ring_fill = nullptr;
    AnimateDecodeParams decode;
};

struct AnimatedImage {
    std::string source;
    AnimatedImageStyle style;
    AnimateJob job;
    std::vector<Texture> frames;
    int cur_frame = 0;
    std::chrono::steady_clock::time_point started{};
    bool shown = false;
    float draw_tint[4] = {1, 1, 1, 1};
    float border_tint[4] = {0, 0, 0, 0};
    float ring_tint[4] = {0, 0, 0, 0};
};

void animated_image_set_source(AnimatedImage &img, std::string source_path,
                               const AnimatedImageStyle &style);

void animated_image_show(AnimatedImage &img, std::function<void()> on_ready);

void animated_image_hide(AnimatedImage &img);

void animated_image_tick(AnimatedImage &img,
                         std::chrono::steady_clock::time_point now);

void animated_image_draw(AnimatedImage &img, Node *parent, float x, float y,
                         float w, float h, float alpha);

bool animated_image_animating(const AnimatedImage &img);
