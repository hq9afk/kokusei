#pragma once

#include <GLES3/gl32.h>
#include <vector>

#include "config/resonance_config.h"

class ResonanceAudioStages {
  public:
    bool init();
    void destroy();

    bool run(int size, const std::vector<float> &l, const std::vector<float> &r,
             int fps);

    GLuint smooth_l() const { return smooth_.tex_l; }
    GLuint smooth_r() const { return smooth_.tex_r; }
    int size() const { return size_; }
    bool ready() const { return ready_ && size_ > 0; }

  private:
    struct Target {
        GLuint fbo = 0;
        GLuint tex_r = 0;
        GLuint tex_l = 0;
    };

    static constexpr int kRing = 5;

    void upload_raw(GLuint tex, const float *data, int size);
    GLuint ensure_tex(GLuint &tex, int size);
    void bind_target(Target &t, int offset, int size);
    void draw_quad();
    void run_channel(int offset, int size);

    GLuint pass_prog_ = 0;
    GLuint gravity_prog_ = 0;
    GLuint average_prog_ = 0;
    GLuint smooth_prog_ = 0;

    GLuint raw_l_ = 0;
    GLuint raw_r_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    Target pass_;
    Target average_;
    Target smooth_;
    Target ring_[kRing];

    int out_idx_ = 0;
    int out_idx_l_ = 0;
    int size_ = 0;
    int fps_ = kResonanceFps;
    bool ready_ = false;
};
