#pragma once

#include <GLES3/gl32.h>

#include "config/resonance_config.h"

class ResonanceBlobPipeline {
  public:
    bool init();
    void destroy();

    void render(int width, int height, int tick, float fade, GLuint audio_l_tex,
                GLuint audio_r_tex, int audio_size,
                const ResonanceParams &params);

  private:
    void ensure_targets(int canvas);
    void draw_quad();
    void set_audio_uniforms(GLuint prog, GLuint audio_l_tex, GLuint audio_r_tex,
                            int audio_size, int tick, int canvas,
                            const ResonanceParams &params);

    GLuint ncs1_prog_ = 0;
    GLuint ncs2_prog_ = 0;
    GLuint glow_prog_ = 0;

    GLuint atomic_tex_ = 0;
    GLuint clear_fbo_ = 0;
    GLuint fbo_[2] = {0, 0};
    GLuint fbo_tex_[2] = {0, 0};
    GLuint glow_fbo_ = 0;
    GLuint glow_tex_ = 0;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    int canvas_ = 0;
    bool ready_ = false;
};
