#pragma once

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <cstdint>
#include <functional>
#include <vector>

#include "render/texture.h"
#include "render/video_texture.h"

struct Affine2D {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;

    static Affine2D translation(float x, float y) {
        return {1.0f, 0.0f, 0.0f, 1.0f, x, y};
    }
    static Affine2D scaling(float s) { return {s, 0.0f, 0.0f, s, 0.0f, 0.0f}; }
    static Affine2D rotation_deg(float deg);

    Affine2D compose(const Affine2D &l) const {
        return {a * l.a + b * l.c,        a * l.b + b * l.d,
                c * l.a + d * l.c,        c * l.b + d * l.d,
                a * l.tx + b * l.ty + tx, c * l.tx + d * l.ty + ty};
    }
};

class Renderer {
  public:
    bool init();

    void destroy();

    void begin_frame(int logical_width, int logical_height, int32_t scale = 1);

    void set_clip(float x, float y, float w, float h);

    int32_t scale() const { return scale_; }

    void set_opacity(float a) { opacity_ = a; }

    void push_model(const Affine2D &local);
    void pop_model();

    void clear_clip();

    void draw_rect(float x, float y, float w, float h, const float color[4]);

    void draw_rounded_rect(float x, float y, float w, float h, float radius,
                           float border_width, const float fill[4],
                           const float border[4]);

    void draw_texture(float x, float y, const Texture &tex,
                      const float tint[4]);

    void draw_texture_rect(float x, float y, float w, float h,
                           const Texture &tex, const float tint[4]);

    void draw_texture_rect_rounded(float x, float y, float w, float h,
                                   float radius, const Texture &tex,
                                   const float tint[4]);

    void draw_video_texture_rect(float x, float y, float w, float h,
                                 const VideoTexture &tex);

    void draw_custom(GLuint program, float x, float y, float w, float h,
                     const std::function<void(GLuint)> &set_uniforms);

  private:
    struct ClipRect {
        float x0, y0, x1, y1;
    };

    void apply_clip(const ClipRect &r);

    void set_common_uniforms(GLuint program, float x, float y, float w,
                             float h);

    void draw_quad(GLuint program);

    GLuint rect_program_ = 0;
    GLuint tex_program_ = 0;
    GLuint rrect_program_ = 0;
    GLuint rounded_tex_program_ = 0;
    GLuint video_program_ = 0;
    GLuint quad_vbo_ = 0;
    GLuint vao_ = 0;
    float viewport_[2] = {0, 0};
    int32_t scale_ = 1;
    float opacity_ = 1.0f;
    std::vector<Affine2D> model_stack_ = {Affine2D{}};
    std::vector<ClipRect> clip_stack_;
};

class ScopedClip {
  public:
    ScopedClip(Renderer &renderer, float x, float y, float w, float h)
        : renderer_(renderer) {
        renderer_.set_clip(x, y, w, h);
    }
    ~ScopedClip() { renderer_.clear_clip(); }
    ScopedClip(const ScopedClip &) = delete;
    ScopedClip &operator=(const ScopedClip &) = delete;

  private:
    Renderer &renderer_;
};
