#pragma once

#include <GLES3/gl32.h>

class Renderer;

struct ThunderBurst {
    GLuint bolt_program = 0;
    GLuint shock_program = 0;
    bool bolt_tried = false;
    bool shock_tried = false;
    bool bolt_logged = false;
    bool shock_logged = false;
};

struct ThunderParams {
    float ax = 0.0f;
    float ay = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    float time_s = 0.0f;
    float progress = 1.0f;
    float intensity = 1.0f;
    float seed = 0.0f;
    float amp = 14.0f;
    float thick = 1.0f;
    float pad = 42.0f;
    const float *core = nullptr;
    const float *glow = nullptr;
};

struct ThunderShockParams {
    float cx = 0.0f;
    float cy = 0.0f;
    float radius = 0.0f;
    float time_s = 0.0f;
    float progress = 0.0f;
    float intensity = 1.0f;
    const float *core = nullptr;
    const float *glow = nullptr;
};

void thunder_burst_draw(ThunderBurst &tb, Renderer &renderer,
                        const ThunderParams &p);

void thunder_shock_draw(ThunderBurst &tb, Renderer &renderer,
                        const ThunderShockParams &p);
