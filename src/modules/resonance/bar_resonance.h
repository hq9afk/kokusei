#pragma once

#include <GLES3/gl32.h>

#include "config/resonance_config.h"

class BarResonance {
  public:
    bool init();
    void destroy();

    void render(int width, int height, int tick, float fade, GLuint audio_l_tex,
                GLuint audio_r_tex, int audio_size,
                const ResonanceParams &params);

  private:
    void draw_quad();

    GLuint prog_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    bool ready_ = false;
};
